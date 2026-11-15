#include "sound.hpp"

#include "../common/audio_constants.hpp"
#include "../../lib/audio/audio_sink.hpp"
#include "../../lib/audio/audio_backend.hpp"
#include "../common/platform_io.hpp"
#include "../../products/obs_drum/obs_drum_kits.hpp"
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
#include <utility>
#include <vector>

// The threads: the audio callback is the engine's only writer. The UI thread and the MIDI thread never call the
// engine: each puts commands in a queue of its own (one producer, one consumer, no lock, no allocation), applied at
// the start of each block, MIDI first.

namespace synth::drums
{
    namespace
    {
        struct Cmd
        {
            enum Type : uint8_t { Hit, Pad, Master, Route, ClearRoutes, Control } type;
            uint8_t a;  // pad, slot, controller
            uint16_t b; // parameter, the route's destination
            uint8_t c;  // the route's source, enabled in the top bit
            float value;
        };

        template <size_t N>
        class Spsc
        {
        public:
            bool push(const Cmd &c)
            {
                const size_t h = head_.load(std::memory_order_relaxed);
                if (h - tail_.load(std::memory_order_acquire) == N)
                    return false;
                ring_[h & (N - 1)] = c;
                head_.store(h + 1, std::memory_order_release);
                return true;
            }
            template <typename F>
            void drain(F &&apply)
            {
                size_t t = tail_.load(std::memory_order_relaxed);
                const size_t h = head_.load(std::memory_order_acquire);
                for (; t != h; ++t)
                    apply(ring_[t & (N - 1)]);
                tail_.store(t, std::memory_order_release);
            }
            bool pop(Cmd &c)
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
    } // namespace

    struct Sound::Impl
    {
        ObsDrums drums{static_cast<float>(SAMPLE_RATE)};
        io::AudioOut output; // the output, reopened from SETUP (platform_io.hpp)
        io::MidiIn midi; // the product's MIDI in (platform_io.hpp)
        Spsc<4096> fromUi;
        Spsc<1024> fromMidi;
        Spsc<512> toUi; // the MIDI thread's, for the panel: CCs and the hits (c: the channel; value; b: the pad + 1)
        std::array<std::atomic<int>, 128> padOf{}; // the controller's notes to pads (-1 none), set by the panel
        std::atomic<int> mapClient{-1};
        bool audio = false;
        std::array<PadSettings, NUM_DRUM_PADS> pads{}; // the audio thread's copy, changed a field at a time
        DrumMaster master;
        std::array<std::atomic<float>, NUM_DRUM_PADS> peak{}, flash{};
        std::array<std::atomic<float>, 2> out{};
        // the recordings handed over: the UI fills a slot while its flag is 0; the audio thread takes it
        std::array<std::shared_ptr<const GrainSource>, NUM_DRUM_PADS> slot;
        std::array<std::atomic<int>, NUM_DRUM_PADS> ready{};
        std::vector<std::shared_ptr<const GrainSource>> kept; // the UI's: freed there, never on the audio thread

        // remote (a plugin plays): the link, what the host changed (values; the whole kit, under its lock), a change
        // the plugin's parameters cannot carry
        panel::RemoteLink link;
        Spsc<4096> fromHost; // Cmd::Pad (a: the pad, 0xFF the master; b: the param), value
        std::mutex kitLock;
        std::string kit;
        bool kitNew = false, kitDirty = false;

        void hear(const std::string &line) // the link's reader thread
        {
            int pad = 0, param = 0;
            float v = 0;
            if (line.size() > 2 && line[0] == 'K' && line[1] == ' ')
            {
                std::lock_guard<std::mutex> g(kitLock);
                kit = line.substr(2);
                kitNew = true;
            }
            else if (std::sscanf(line.c_str(), "v %d %d %f", &pad, &param, &v) == 3)
                fromHost.push({Cmd::Pad, static_cast<uint8_t>(pad < 0 ? 0xFF : pad), static_cast<uint16_t>(param), 0, v});
            else if (line.size() > 2 && line[0] == 's' && line[1] == ' ') // the outputs, then each pad's peak and flash
            {
                const char *p = line.c_str() + 2;
                char *end = nullptr;
                for (size_t i = 0; i < 2 + 2 * NUM_DRUM_PADS; ++i, p = end)
                {
                    const float x = std::strtof(p, &end);
                    if (end == p)
                        break;
                    if (i < 2)
                        out[i].store(x, std::memory_order_relaxed);
                    else if (i < 2 + NUM_DRUM_PADS)
                        peak[i - 2].store(x, std::memory_order_relaxed);
                    else
                        flash[i - 2 - NUM_DRUM_PADS].store(x, std::memory_order_relaxed);
                }
            }
        }

        void pad(int i, int p, float v)
        {
            PadSettings &s = pads[static_cast<size_t>(i)];
            switch (p)
            {
            case P_ENGINE: s.engine = static_cast<DrumEngine>(std::clamp(static_cast<int>(v + 0.5f), 0, NUM_DRUM_ENGINES - 1)); break;
            case P_CIRCUIT: s.circuit = static_cast<DrumKind>(std::clamp(static_cast<int>(v + 0.5f), 0, NUM_DRUM_KINDS - 1)); break;
            case P_TUNE: s.p.tune = v; break;
            case P_DECAY: s.p.decay = v; break;
            case P_A: s.p.a = v; break;
            case P_B: s.p.b = v; break;
            case P_BENT: s.p.bent = v > 0.5f; break;
            case P_LEVEL: s.level = v; break;
            case P_PAN: s.pan = v; break;
            case P_FX: s.fx = v; break;
            case P_MUTE: s.mute = v > 0.5f; break;
            case P_SOLO: s.solo = v > 0.5f; break;
            case P_NOTE: s.note = static_cast<int>(v + 0.5f); break;
            case P_CHOKE: s.choke = static_cast<int>(std::lround(v)); break;
            case P_GRAINPOS: s.grainPos = v; break;
            default: return;
            }
            drums.setPad(i, s);
        }

        void masterParam(int p, float v)
        {
            DrumMaster &m = master;
            switch (p)
            {
            case M_DELAY_TIME: m.delayTime = v; break;
            case M_DELAY_FB: m.delayFeedback = v; break;
            case M_DELAY_MIX: m.delayMix = v; break;
            case M_REV_SIZE: m.reverbSize = v; break;
            case M_REV_DAMP: m.reverbDamp = v; break;
            case M_REV_MIX: m.reverbMix = v; break;
            case M_DRIVE: m.drive = v; break;
            case M_GLUE: m.glue = v; break;
            case M_TUBE_MIX: m.tubeMix = v; break;
            case M_VOLUME: m.volume = v; break;
            default: return;
            }
            drums.setMaster(m);
        }

        void apply(const Cmd &c)
        {
            switch (c.type)
            {
            case Cmd::Hit: drums.noteOn(c.a, c.value); break; // MIDI: by note
            case Cmd::Pad:
                if (c.b == 0xFFFF)
                    drums.hit(c.a, c.value); // the UI: by pad
                else
                    pad(c.a, c.b, c.value);
                break;
            case Cmd::Master: masterParam(c.b, c.value); break;
            case Cmd::Route: drums.setRoute(c.a, c.c & 0x7f, c.b, c.value, (c.c & 0x80) != 0); break;
            case Cmd::ClearRoutes: drums.clearRoutes(); break;
            case Cmd::Control: // the wheel, two CCs (16, 17), all notes off does nothing on drums
                if (c.a == 1)
                    drums.setModWheel(c.value);
                else if (c.a == 16 || c.a == 17)
                    drums.setCC(c.a - 16, c.value);
                break;
            }
        }

        void block(float *buffer, int frames, int channels)
        {
#if defined(__SSE__) || defined(_M_X64)
            _mm_setcsr(_mm_getcsr() | 0x8040); // flush to zero: decaying states cost nothing in silence
#endif
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
                if (ready[static_cast<size_t>(i)].load(std::memory_order_acquire))
                {
                    drums.setSource(i, slot[static_cast<size_t>(i)]);
                    ready[static_cast<size_t>(i)].store(0, std::memory_order_release);
                }
            fromMidi.drain([this](const Cmd &c) { apply(c); });
            fromUi.drain([this](const Cmd &c) { apply(c); });
            float stereo[1024];
            for (int done = 0; done < frames;)
            {
                const int n = std::min(512, frames - done);
                drums.render(stereo, n);
                for (int i = 0; i < n; ++i)
                    for (int c = 0; c < channels; ++c)
                        buffer[(done + i) * channels + c] = channels == 2 ? stereo[i * 2 + c] : 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);
                float l = 0, r = 0;
                for (int i = 0; i < n; ++i)
                {
                    l = std::max(l, std::fabs(stereo[i * 2]));
                    r = std::max(r, std::fabs(stereo[i * 2 + 1]));
                }
                out[0].store(l, std::memory_order_relaxed);
                out[1].store(r, std::memory_order_relaxed);
                done += n;
            }
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
            {
                peak[static_cast<size_t>(i)].store(drums.padPeak(i), std::memory_order_relaxed);
                flash[static_cast<size_t>(i)].store(drums.padFlash(i), std::memory_order_relaxed);
            }
        }
    };

    Sound::Sound() : impl_(std::make_unique<Impl>()) {}

    Sound::~Sound()
    {
        impl_->midi.close(); // no more commands from MIDI
        impl_->output.close();
    }

    bool Sound::start(const DrumKit &kit, const std::array<std::shared_ptr<const GrainSource>, 16> &sources, const io::Settings &settings)
    {
        // before any audio thread: the engine is still this thread's alone
        applyDrumKit(impl_->drums, kit);
        impl_->pads = kit.pads;
        impl_->master = kit.master;
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
            if (sources[static_cast<size_t>(i)])
            {
                impl_->kept.push_back(sources[static_cast<size_t>(i)]);
                impl_->drums.setSource(i, sources[static_cast<size_t>(i)]);
            }
        if (remote()) // the plugin plays: no audio, no MIDI here
            return true;
        impl_->audio = impl_->output.open("ObsDrums", settings, [this](float *buffer, int frames, int channels) { impl_->block(buffer, frames, channels); });
        if (!impl_->audio)
            std::cerr << "[obsdrums] no audio output" << std::endl;
        io::MidiHandlers h;
        h.note = [this](const MidiNote &n, int source)
        {
            if (!n.is_note_on || n.velocity == 0) // drums: the note-offs mean nothing
                return;
            const float v = static_cast<float>(n.velocity) / 127.0f;
            const int pad = source == impl_->mapClient.load(std::memory_order_relaxed) && n.note < 128
                                ? impl_->padOf[n.note].load(std::memory_order_relaxed) : -1;
            if (pad >= 0)
                impl_->fromMidi.push({Cmd::Pad, static_cast<uint8_t>(pad), 0xFFFF, 0, v});
            else
                impl_->fromMidi.push({Cmd::Hit, n.note, 0, 0, v});
            impl_->toUi.push({Cmd::Hit, n.note, static_cast<uint16_t>(pad + 1), n.channel, v});
        };
        h.control = [this](uint8_t ch, uint8_t cc, uint8_t v, int)
        {
            impl_->fromMidi.push({Cmd::Control, cc, 0, 0, static_cast<float>(v) / 127.0f});
            impl_->toUi.push({Cmd::Control, cc, 0, ch, static_cast<float>(v) / 127.0f});
        };
        impl_->midi.open("ObsDrums", std::move(h), settings);
        return impl_->audio;
    }

    bool Sound::connect(const char *link)
    {
        impl_->link.onLine = [this](const std::string &line) { impl_->hear(line); };
        return impl_->link.connect(link);
    }
    bool Sound::remote() const { return impl_->link.linked(); }
    bool Sound::remoteAlive() const { return impl_->link.alive(); }
    bool Sound::kitWanted() { return std::exchange(impl_->kitDirty, false); }
    void Sound::sendKit(const std::string &json) { impl_->link.send("K " + json + "\n"); }
    bool Sound::pollRemote(int &pad, int &param, float &value)
    {
        Cmd c;
        if (!impl_->fromHost.pop(c))
            return false;
        pad = c.a == 0xFF ? -1 : c.a;
        param = c.b;
        value = c.value;
        return true;
    }
    bool Sound::pollKit(std::string &json)
    {
        std::lock_guard<std::mutex> g(impl_->kitLock);
        if (!impl_->kitNew)
            return false;
        json.swap(impl_->kit);
        impl_->kitNew = false;
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

    // remote: "p pad param value" (pad -1: the master), as the panel numbers them; the plugin maps them
    void Sound::setPad(int pad, int param, float value)
    {
        if (remote())
        {
            if (param == P_NOTE || param == P_CHOKE) // no parameter of the plugin's: the whole kit
                impl_->kitDirty = true;
            else
                impl_->link.send("p " + std::to_string(pad) + " " + std::to_string(param) + " " + num(value) + "\n");
            return;
        }
        impl_->fromUi.push({Cmd::Pad, static_cast<uint8_t>(pad), static_cast<uint16_t>(param), 0, value});
    }
    void Sound::setMaster(int param, float value)
    {
        if (remote())
            return impl_->link.send("p -1 " + std::to_string(param) + " " + num(value) + "\n");
        impl_->fromUi.push({Cmd::Master, 0, static_cast<uint16_t>(param), 0, value});
    }
    void Sound::setRoute(int slot, int source, int dest, float amount, bool enabled)
    {
        if (remote())
        {
            impl_->kitDirty = true;
            return;
        }
        impl_->fromUi.push({Cmd::Route, static_cast<uint8_t>(slot), static_cast<uint16_t>(dest), static_cast<uint8_t>((source & 0x7f) | (enabled ? 0x80 : 0)), amount});
    }
    void Sound::clearRoutes()
    {
        if (remote())
        {
            impl_->kitDirty = true;
            return;
        }
        impl_->fromUi.push({Cmd::ClearRoutes, 0, 0, 0, 0.0f});
    }
    void Sound::hit(int pad, float velocity)
    {
        if (remote())
            return impl_->link.send("h " + std::to_string(pad) + " " + num(velocity) + "\n");
        impl_->fromUi.push({Cmd::Pad, static_cast<uint8_t>(pad), 0xFFFF, 0, velocity});
    }

    bool Sound::setSource(int pad, std::shared_ptr<const GrainSource> src)
    {
        if (remote()) // the plugin reads the kit's recordings itself
        {
            impl_->kitDirty = true;
            return true;
        }
        if (pad < 0 || pad >= NUM_DRUM_PADS || impl_->ready[static_cast<size_t>(pad)].load(std::memory_order_acquire))
            return false;
        auto &kept = impl_->kept;
        kept.erase(std::remove_if(kept.begin(), kept.end(), [](const auto &p) { return p.use_count() == 1; }), kept.end());
        kept.push_back(src);
        impl_->slot[static_cast<size_t>(pad)] = std::move(src);
        impl_->ready[static_cast<size_t>(pad)].store(1, std::memory_order_release);
        return true;
    }

    bool Sound::pollMidi(Seen &e)
    {
        Cmd c;
        if (!impl_->toUi.pop(c))
            return false;
        e = {c.type == Cmd::Control, c.c & 15, c.a, static_cast<int>(c.b) - 1, c.value};
        return true;
    }

    void Sound::setNoteMap(int client, const std::array<int, 128> &padOf)
    {
        for (size_t i = 0; i < 128; ++i)
            impl_->padOf[i].store(padOf[i], std::memory_order_relaxed);
        impl_->mapClient.store(client, std::memory_order_release);
    }

    float Sound::padPeak(int pad) const { return impl_->peak[static_cast<size_t>(std::clamp(pad, 0, NUM_DRUM_PADS - 1))].load(std::memory_order_relaxed); }
    float Sound::padFlash(int pad) const { return impl_->flash[static_cast<size_t>(std::clamp(pad, 0, NUM_DRUM_PADS - 1))].load(std::memory_order_relaxed); }
    float Sound::outPeak(int side) const { return impl_->out[static_cast<size_t>(side & 1)].load(std::memory_order_relaxed); }
} // namespace synth::drums

namespace synth::drums
{
    bool Sound::reopenAudio(const io::Settings &settings)
    {
        impl_->audio = impl_->output.open("ObsDrums", settings, [this](float *buffer, int frames, int channels) { impl_->block(buffer, frames, channels); });
        return impl_->audio;
    }
    const io::AudioOut &Sound::audioOut() const { return impl_->output; }
    io::MidiIn &Sound::midiIn() { return impl_->midi; }
}
