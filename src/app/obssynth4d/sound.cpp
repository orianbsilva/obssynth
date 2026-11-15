#include "sound.hpp"
#include "params.hpp"

#include "../common/audio_constants.hpp"
#include "../../lib/audio/audio_sink.hpp"
#include "../../lib/audio/audio_backend.hpp"
#include "../common/platform_io.hpp"
#include "../../products/obs_synth/obs_synth_4d.hpp"
#include "driver.hpp"
#include "../common/remote_link.hpp"

#include <cstdint>
#include <algorithm>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

// The threads: the audio callback is the engine's only writer. The UI thread and the MIDI thread never call
// the engine: each puts commands in a queue of its own (one producer, one consumer, no lock, no allocation),
// and the audio callback applies them at the start of each block, MIDI first, then generates. What is heavy
// (reading the banks from disk) happens in start(), before the audio thread exists.

namespace synth
{
    namespace
    {
        struct Cmd
        {
            enum Type : uint8_t { NoteOn, NoteOff, Osc, Global, Route, Extra, Control, Bend, Pressure, PolyPressure } type;
            uint8_t a;  // note, oscillator or slot
            uint16_t b; // parameter, or the route's destination
            uint8_t c;  // the route's source, enabled in the top bit
            float value;
        };

        template <size_t N>
        class Spsc
        {
        public:
            bool push(const Cmd &c) // the producer's thread only
            {
                const size_t h = head_.load(std::memory_order_relaxed);
                if (h - tail_.load(std::memory_order_acquire) == N)
                    return false; // full: dropped (a knob's next change follows)
                ring_[h & (N - 1)] = c;
                head_.store(h + 1, std::memory_order_release);
                return true;
            }
            template <typename F>
            void drain(F &&apply) // the audio thread only
            {
                size_t t = tail_.load(std::memory_order_relaxed);
                const size_t h = head_.load(std::memory_order_acquire);
                for (; t != h; ++t)
                    apply(ring_[t & (N - 1)]);
                tail_.store(t, std::memory_order_release);
            }
            bool pop(Cmd &c) // the consumer's thread only
            {
                const size_t t = tail_.load(std::memory_order_relaxed);
                if (t == head_.load(std::memory_order_acquire))
                    return false;
                c = ring_[t & (N - 1)];
                tail_.store(t + 1, std::memory_order_release);
                return true;
            }

        private:
            std::array<Cmd, N> ring_{};
            alignas(64) std::atomic<size_t> head_{0};
            alignas(64) std::atomic<size_t> tail_{0};
        };

        constexpr int kScope = 2048;
    } // namespace

    struct Sound::Impl
    {
        ObsSynth4D synth{static_cast<float>(SAMPLE_RATE), engineProfile().oversample}; // the render rate is the profile's
        io::AudioOut output; // the output, reopened from SETUP (platform_io.hpp)
        io::MidiIn midi; // the product's MIDI in (platform_io.hpp)
        Spsc<4096> fromUi;
        Spsc<1024> fromMidi;
        Spsc<512> toUi; // the MIDI thread's CCs, for the panel's CC overlay (c: the channel)
        bool audio = false;

        std::array<float, kScope> scopeRing{};
        std::atomic<int> scopeAt{0};
        std::atomic<int> notesApplied{0};
        std::atomic<float> peak{0.0f};
        std::atomic<int> blocks{0};
        std::array<std::atomic<float>, 64> sources{}; // the mod sources' values, for the matrix's meters

        obs::Driver drive{synth}; // the panel's values into the engine (driver.hpp)
        void osc(int o, int p, float v) { drive.osc(o, p, v); }
        void global(int p, float v) { drive.global(p, v); }

        std::atomic<float> vecX{0.5f}, vecY{0.5f};

        // remote (a plugin plays): the link, what the host changed (values; the whole state, under its lock)
        panel::RemoteLink link;
        Spsc<4096> fromHost; // Cmd::Global: b the plugin's id, value 0..1
        std::mutex wholeLock;
        std::string whole;
        bool wholeNew = false;

        void hear(const std::string &line) // the link's reader thread
        {
            int id = 0;
            float x = 0, y = 0;
            if (line.size() > 2 && line[0] == 'S' && line[1] == ' ')
            {
                std::lock_guard<std::mutex> g(wholeLock);
                whole = line.substr(2);
                wholeNew = true;
            }
            else if (std::sscanf(line.c_str(), "v %d %f", &id, &x) == 2 && id >= 0)
                fromHost.push({Cmd::Global, 0, static_cast<uint16_t>(id), 0, x});
            else if (line.size() > 2 && line[0] == 's' && line[1] == ' ') // the vector's point, then the mod sources
            {
                const char *p = line.c_str() + 2;
                char *end = nullptr;
                x = std::strtof(p, &end), p = end;
                y = std::strtof(p, &end), p = end;
                vecX.store(x, std::memory_order_relaxed), vecY.store(y, std::memory_order_relaxed);
                for (size_t i = 0; i < sources.size(); ++i, p = end)
                {
                    const float v = std::strtof(p, &end);
                    if (end == p)
                        break;
                    sources[i].store(v, std::memory_order_relaxed);
                }
            }
            else if (line.size() > 2 && line[0] == 'w' && line[1] == ' ') // the last samples (left), oldest first
            {
                const char *p = line.c_str() + 2;
                char *end = nullptr;
                int n = 0;
                for (; n < kScope; ++n, p = end)
                {
                    const float v = std::strtof(p, &end);
                    if (end == p)
                        break;
                    scopeRing[static_cast<size_t>(n)] = v;
                }
                scopeAt.store(n % kScope, std::memory_order_release);
            }
        }

        void apply(const Cmd &c)
        {
            switch (c.type)
            {
            case Cmd::NoteOn:
                synth.noteOn(c.a, c.value);
                notesApplied.fetch_add(1, std::memory_order_relaxed);
                break;
            case Cmd::NoteOff: synth.noteOff(c.a); break;
            case Cmd::Control: // the controllers the presets route: wheel, expression, CC A..D (16..19); all notes off
                if (c.a == 1)
                    synth.setModWheel(c.value);
                else if (c.a == 11)
                    synth.setExpression(c.value);
                else if (c.a >= 16 && c.a <= 19)
                    synth.setCC(c.a - 16, c.value);
                else if (c.a == 120 || c.a == 123)
                    synth.allNotesOff();
                break;
            case Cmd::Bend: synth.setPitchBend(c.value); break;
            case Cmd::Pressure: synth.setChannelAftertouch(c.value); break;
            case Cmd::PolyPressure: synth.setPolyAftertouch(c.a, c.value); break;
            case Cmd::Osc: osc(c.a, c.b, c.value); break;
            case Cmd::Global: global(c.b, c.value); break;
            case Cmd::Extra: drive.extra(c.b, c.value); break;
            case Cmd::Route: drive.route(c.a, c.c & 0x7f, c.b, c.value, (c.c & 0x80) != 0); break;
            }
        }

        // the recordings handed over: the UI fills a slot while its flag is 0 and sets it; the audio thread takes
        // it (a shared_ptr copy, no allocation) and clears the flag
        std::shared_ptr<const GrainSource> grainSlot[4];
        std::atomic<int> grainReady[4]{};
        std::vector<std::shared_ptr<const GrainSource>> grainKept; // the UI's: no recording is freed on the audio thread

        void block(float *buffer, int frames, int channels)
        {
#if defined(__SSE__) || defined(_M_X64)
            _mm_setcsr(_mm_getcsr() | 0x8040); // flush to zero, denormals are zero: decaying states cost nothing in silence
#endif
            for (int o = 0; o < 4; ++o)
                if (grainReady[o].load(std::memory_order_acquire))
                {
                    synth.setGrainSource(o, grainSlot[o]);
                    grainReady[o].store(0, std::memory_order_release);
                }
            fromMidi.drain([this](const Cmd &c) { apply(c); });
            fromUi.drain([this](const Cmd &c) { apply(c); });
            if (channels == 2)
                synth.generateStereoBuffer(buffer, frames);
            else
                synth.generateBuffer(buffer, frames);
            float m = 0.0f;
            int at = scopeAt.load(std::memory_order_relaxed);
            for (int i = 0; i < frames; ++i)
            {
                const float l = buffer[i * channels];
                m = std::max(m, std::fabs(l));
                scopeRing[static_cast<size_t>(at)] = l;
                at = (at + 1) % kScope;
            }
            scopeAt.store(at, std::memory_order_release);
            if (m > peak.load(std::memory_order_relaxed))
                peak.store(m, std::memory_order_relaxed);
            blocks.fetch_add(1, std::memory_order_relaxed);
            vecX.store(synth.getEngine().vectorX(), std::memory_order_relaxed);
            vecY.store(synth.getEngine().vectorY(), std::memory_order_relaxed);
            const auto &mm = synth.getEngine().getModMatrix();
            for (size_t i = 0; i < sources.size(); ++i)
                sources[i].store(mm.getSourceValue(static_cast<int>(i)), std::memory_order_relaxed);
        }
    };

    Sound::Sound() : impl_(std::make_unique<Impl>())
    {
        // a small machine renders less (engine_profile.hpp): OBS_PROFILE=lite|full, else by the CPU
        const EngineProfile p = engineProfile();
        impl_->synth.applyProfile(p);
        std::fprintf(stderr, "obssynth4d: profile %s (%d voices, modulation every %d samples, oversampling %dx%s)\n", p.name, p.maxVoices, p.controlRate, p.oversample, p.skipSilent ? ", silent oscillators skipped" : "");
    }

    Sound::~Sound()
    {
        impl_->midi.close(); // no more commands from MIDI
        impl_->output.close();
    }

    bool Sound::start(const std::array<std::string, 4> &banks, const std::string &grain, const float osc[4][48], const float glb[48], const io::Settings &settings)
    {
        // before any audio thread: here the engine is still this thread's alone
        ObsSynth4D &s = impl_->synth;
        for (int o = 0; o < 4; ++o)
        {
            const size_t oi = static_cast<size_t>(o);
            if (!banks[oi].empty())
                s.loadWtBank(o, banks[oi]);
            if (o == 0 && !grain.empty()) // one recording for the four, kept here (never freed on the audio thread)
            {
                if (auto src = GrainSource::loadWav(grain))
                    impl_->grainKept.push_back(std::move(src));
                else
                    std::fprintf(stderr, "obssynth4d: cannot read %s for the grains\n", grain.c_str());
            }
            if (!impl_->grainKept.empty())
                s.setGrainSource(o, impl_->grainKept.front());
            impl_->drive.tables[oi] = s.getEngine().getAllocator().getVoice(0).getChain(o).getWtTableCount();
            for (int p = 0; p < obs::OSC_PARAM_COUNT; ++p)
                impl_->osc(o, p, osc[o][p]);
        }
        for (int p = 0; p < obs::GLB_PARAM_COUNT; ++p)
            impl_->global(p, glb[p]);
        if (remote()) // the plugin plays: no audio, no MIDI here (the banks above: the tables' counts)
            return true;

        impl_->audio = impl_->output.open("ObsSynth4D", settings, [this](float *buffer, int frames, int channels) { impl_->block(buffer, frames, channels); });
        if (!impl_->audio)
            std::cerr << "[obssynth4d] no audio output" << std::endl;

        io::MidiHandlers h;
        h.note = [this](const MidiNote &n, int)
        {
            if (n.is_note_on && n.velocity > 0)
                impl_->fromMidi.push({Cmd::NoteOn, n.note, 0, 0, static_cast<float>(n.velocity) / 127.0f});
            else
                impl_->fromMidi.push({Cmd::NoteOff, n.note, 0, 0, 0.0f});
        };
        h.control = [this](uint8_t ch, uint8_t cc, uint8_t v, int)
        {
            impl_->fromMidi.push({Cmd::Control, cc, 0, 0, static_cast<float>(v) / 127.0f});
            impl_->toUi.push({Cmd::Control, cc, 0, ch, static_cast<float>(v) / 127.0f});
        };
        h.bend = [this](uint8_t, int v) // ±2 semitones
        { impl_->fromMidi.push({Cmd::Bend, 0, 0, 0, static_cast<float>(v) / 8192.0f * 2.0f}); };
        h.pressure = [this](uint8_t, uint8_t p)
        { impl_->fromMidi.push({Cmd::Pressure, 0, 0, 0, static_cast<float>(p) / 127.0f}); };
        h.polyPressure = [this](uint8_t, uint8_t note, uint8_t p)
        { impl_->fromMidi.push({Cmd::PolyPressure, note, 0, 0, static_cast<float>(p) / 127.0f}); };
        impl_->midi.open("ObsSynth4D", std::move(h), settings);
        return impl_->audio;
    }

    bool Sound::connect(const char *link)
    {
        impl_->link.onLine = [this](const std::string &line) { impl_->hear(line); };
        return impl_->link.connect(link);
    }
    bool Sound::remote() const { return impl_->link.linked(); }
    bool Sound::remoteAlive() const { return impl_->link.alive(); }
    bool Sound::pollRemote(int &id, float &value)
    {
        Cmd c;
        if (!impl_->fromHost.pop(c))
            return false;
        id = c.b;
        value = c.value;
        return true;
    }
    void Sound::setGrainFile(int osc, const std::string &file)
    {
        if (remote())
            impl_->link.send("g " + std::to_string(osc) + " " + file + "\n");
    }
    bool Sound::pollWhole(std::string &json)
    {
        std::lock_guard<std::mutex> g(impl_->wholeLock);
        if (!impl_->wholeNew)
            return false;
        json.swap(impl_->whole);
        impl_->wholeNew = false;
        return true;
    }

    namespace
    {
        std::string num(float v)
        {
            char b[32];
            std::snprintf(b, sizeof b, "%.9g", static_cast<double>(v));
            return b;
        }
    } // namespace

    void Sound::control(int cc, float value)
    {
        if (remote())
            return impl_->link.send("c " + std::to_string(cc) + " " + num(value) + "\n");
        impl_->fromUi.push({Cmd::Control, static_cast<uint8_t>(cc), 0, 0, value});
    }

    bool Sound::pollCc(int &channel, int &cc, float &value)
    {
        Cmd c;
        if (!impl_->toUi.pop(c))
            return false;
        channel = c.c & 15;
        cc = c.a;
        value = c.value;
        return true;
    }

    // remote: the plugin's values are the panel's 0..1 (its id: o * OSC_PARAM_COUNT + p, then the globals)
    void Sound::setOsc(int osc, int param, float value)
    {
        if (remote())
            return impl_->link.send("p " + std::to_string(osc * obs::OSC_PARAM_COUNT + param) + " " + num(obs::toNorm(obs::OSC_RANGES[param], value)) + "\n");
        impl_->fromUi.push({Cmd::Osc, static_cast<uint8_t>(osc), static_cast<uint16_t>(param), 0, value});
    }
    void Sound::setGlobal(int param, float value)
    {
        if (remote())
            return impl_->link.send("p " + std::to_string(4 * obs::OSC_PARAM_COUNT + param) + " " + num(obs::toNorm(obs::GLB_RANGES[param], value)) + "\n");
        impl_->fromUi.push({Cmd::Global, 0, static_cast<uint16_t>(param), 0, value});
    }
    void Sound::setRoute(int slot, int source, int dest, float amount, bool enabled)
    {
        if (remote())
            return impl_->link.send("r " + std::to_string(slot) + " " + std::to_string(source) + " " + std::to_string(dest) + " " + num(amount) + " " +
                                    (enabled ? "1" : "0") + "\n");
        impl_->fromUi.push({Cmd::Route, static_cast<uint8_t>(slot), static_cast<uint16_t>(dest),
                            static_cast<uint8_t>((source & 0x7f) | (enabled ? 0x80 : 0)), amount});
    }
    void Sound::setExtra(int id, float value)
    {
        if (remote())
            return impl_->link.send("x " + std::to_string(id) + " " + num(value) + "\n");
        impl_->fromUi.push({Cmd::Extra, 0, static_cast<uint16_t>(id), 0, value});
    }
    void Sound::noteOn(int note, float velocity)
    {
        if (remote())
            return impl_->link.send("n " + std::to_string(note) + " " + num(velocity) + "\n");
        impl_->fromUi.push({Cmd::NoteOn, static_cast<uint8_t>(note), 0, 0, velocity});
    }
    void Sound::noteOff(int note)
    {
        if (remote())
            return impl_->link.send("o " + std::to_string(note) + "\n");
        impl_->fromUi.push({Cmd::NoteOff, static_cast<uint8_t>(note), 0, 0, 0.0f});
    }
    float Sound::source(int id) const
    {
        return id >= 0 && id < 64 ? impl_->sources[static_cast<size_t>(id)].load(std::memory_order_relaxed) : 0.0f;
    }

    bool Sound::setGrainSource(int osc, std::shared_ptr<const GrainSource> src)
    {
        if (remote()) // the plugin keeps its own recording: the panel shows this one's shape only
            return true;
        if (osc < 0 || osc > 3 || impl_->grainReady[osc].load(std::memory_order_acquire))
            return false;
        // the recordings no chain holds any more (only this list): free them here, on this thread
        auto &kept = impl_->grainKept;
        kept.erase(std::remove_if(kept.begin(), kept.end(), [](const auto &p) { return p.use_count() == 1; }), kept.end());
        kept.push_back(src);
        impl_->grainSlot[osc] = std::move(src);
        impl_->grainReady[osc].store(1, std::memory_order_release);
        return true;
    }

    void Sound::vectorNow(float &x, float &y) const
    {
        x = impl_->vecX.load(std::memory_order_relaxed);
        y = impl_->vecY.load(std::memory_order_relaxed);
    }

    int Sound::tables(int osc) const { return impl_->drive.tables[static_cast<size_t>(osc)]; }

    void Sound::scope(float *out, int n) const
    {
        const int at = impl_->scopeAt.load(std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            out[i] = impl_->scopeRing[static_cast<size_t>((at - n + i + kScope) % kScope)];
    }

    std::string Sound::debugLine()
    {
        char b[160];
        std::snprintf(b, sizeof b, "blocks %d, notes applied %d, peak %.3f, vector %.2f %.2f", impl_->blocks.load(), impl_->notesApplied.load(),
                      static_cast<double>(impl_->peak.exchange(0.0f)), static_cast<double>(impl_->vecX.load()), static_cast<double>(impl_->vecY.load()));
        return b;
    }
} // namespace synth

namespace synth
{
    bool Sound::reopenAudio(const io::Settings &settings)
    {
        impl_->audio = impl_->output.open("ObsSynth4D", settings, [this](float *buffer, int frames, int channels) { impl_->block(buffer, frames, channels); });
        return impl_->audio;
    }
    const io::AudioOut &Sound::audioOut() const { return impl_->output; }
    io::MidiIn &Sound::midiIn() { return impl_->midi; }
}
