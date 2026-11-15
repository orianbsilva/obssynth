#include "sound.hpp"

#include "one_driver.hpp"
#include "../common/audio_constants.hpp"
#include "../common/platform_io.hpp"
#include "../../lib/synth/engine_profile.hpp"

#include <algorithm>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>

// The threads: the audio callback is the engine's only writer. The UI and the MIDI threads put commands in queues of
// their own (one producer, one consumer, no lock, no allocation), applied at the start of each block, MIDI first.

namespace synth::oneapp
{
    namespace
    {
        struct Cmd
        {
            enum Type : uint8_t { NoteOn, NoteOff, Param, Control, Bend, Pressure, AllOff } type;
            uint8_t a;  // note, controller
            uint16_t b; // parameter
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
            bool pop(Cmd &c)
            {
                const size_t t = tail_.load(std::memory_order_relaxed);
                if (t == head_.load(std::memory_order_acquire))
                    return false;
                c = ring_[t & (N - 1)];
                tail_.store(t + 1, std::memory_order_release);
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

        private:
            std::array<Cmd, N> ring_{};
            alignas(64) std::atomic<size_t> head_{0};
            alignas(64) std::atomic<size_t> tail_{0};
        };
    } // namespace

    struct Sound::Impl
    {
        EngineProfile prof = engineProfile();
        std::unique_ptr<ObsSynthOne> engine;
        std::unique_ptr<onep::Driver> driver;
        std::shared_ptr<const GrainSource> grain;          // kept here: never freed on the audio thread
        std::vector<std::shared_ptr<const GrainSource>> samples;
        io::AudioOut output;
        io::MidiIn midi;
        Spsc<4096> fromUi;
        Spsc<1024> fromMidi;
        Spsc<512> toUi; // the CCs, for the overlay
        bool audio = false;
        std::array<std::atomic<float>, 2> out{};
        std::atomic<int> voices{0};

        void apply(const Cmd &c)
        {
            ObsSynthOne &e = *engine;
            switch (c.type)
            {
            case Cmd::NoteOn: e.noteOn(c.a, c.value); break;
            case Cmd::NoteOff: e.noteOff(c.a); break;
            case Cmd::Param:
                if (c.b < onep::NPARAMS)
                    onep::drive(*driver, c.b, c.value);
                break;
            case Cmd::Bend: e.setPitchBend(c.value); break;
            case Cmd::Pressure: e.setChannelAftertouch(c.value); break;
            case Cmd::AllOff: e.allNotesOff(); break;
            case Cmd::Control:
                if (c.a == 1)
                    e.setModWheel(c.value);
                else if (c.a == 120 || c.a == 123)
                    e.allNotesOff();
                break;
            }
        }

        void block(float *buffer, int frames, int channels)
        {
#if defined(__SSE__) || defined(_M_X64)
            _mm_setcsr(_mm_getcsr() | 0x8040); // flush to zero: decaying states cost nothing in silence
#endif
            fromMidi.drain([this](const Cmd &c) { apply(c); });
            fromUi.drain([this](const Cmd &c) { apply(c); });
            float stereo[1024];
            float l = 0, r = 0;
            for (int done = 0; done < frames;)
            {
                const int n = std::min(512, frames - done);
                engine->generateStereoBuffer(stereo, n);
                for (int i = 0; i < n; ++i)
                {
                    for (int ch = 0; ch < channels; ++ch)
                        buffer[(done + i) * channels + ch] = channels == 2 ? stereo[i * 2 + ch] : 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);
                    l = std::max(l, std::fabs(stereo[i * 2]));
                    r = std::max(r, std::fabs(stereo[i * 2 + 1]));
                }
                done += n;
            }
            out[0].store(l, std::memory_order_relaxed);
            out[1].store(r, std::memory_order_relaxed);
            voices.store(engine->sounding(), std::memory_order_relaxed);
        }
    };

    Sound::Sound() : impl_(std::make_unique<Impl>()) {}

    Sound::~Sound()
    {
        impl_->midi.close(); // no more commands from MIDI
        impl_->output.close();
    }

    bool Sound::start(const float *values, const std::string &bank, const std::vector<std::string> &samples, const std::string &grain,
                      const io::Settings &settings)
    {
        // before any audio thread: the engine is still this thread's alone
        Impl &m = *impl_;
        m.engine = std::make_unique<ObsSynthOne>(static_cast<float>(SAMPLE_RATE), m.prof.oversample);
        m.engine->applyProfile(m.prof);
        m.driver = std::make_unique<onep::Driver>(onep::Driver{*m.engine});
        if (!bank.empty())
            m.engine->loadWtBank(bank);
        m.driver->tables = m.engine->wtTableCount();
        for (const auto &p : samples)
            m.samples.push_back(GrainSource::loadWav(p));
        m.driver->samples = &m.samples;
        if (!grain.empty())
            m.grain = GrainSource::loadWav(grain);
        if (m.grain)
            m.engine->setGrainSource(m.grain);
        for (int i = 0; i < onep::NPARAMS; ++i)
            onep::drive(*m.driver, i, values[i]);
        std::cout << "[obssynthone] profile " << m.prof.name << ": " << m.prof.maxVoices << " voices, control every " << m.prof.controlRate
                  << " samples, " << m.prof.oversample << "x" << std::endl;

        m.audio = m.output.open("ObsSynthOne", settings, [this](float *b, int f, int c) { impl_->block(b, f, c); });
        if (!m.audio)
            std::cerr << "[obssynthone] no audio output" << std::endl;
        io::MidiHandlers h;
        h.note = [this](const MidiNote &n, int)
        {
            if (n.is_note_on && n.velocity > 0)
                impl_->fromMidi.push({Cmd::NoteOn, n.note, 0, static_cast<float>(n.velocity) / 127.0f});
            else
                impl_->fromMidi.push({Cmd::NoteOff, n.note, 0, 0.0f});
        };
        h.control = [this](uint8_t ch, uint8_t cc, uint8_t v, int)
        {
            impl_->fromMidi.push({Cmd::Control, cc, 0, static_cast<float>(v) / 127.0f});
            impl_->toUi.push({Cmd::Control, cc, ch, static_cast<float>(v) / 127.0f});
        };
        h.bend = [this](uint8_t, int v) // ±2 semitones
        { impl_->fromMidi.push({Cmd::Bend, 0, 0, static_cast<float>(v) / 8192.0f * 2.0f}); };
        h.pressure = [this](uint8_t, uint8_t p) { impl_->fromMidi.push({Cmd::Pressure, 0, 0, static_cast<float>(p) / 127.0f}); };
        m.midi.open("ObsSynthOne", std::move(h), settings);
        return m.audio;
    }

    void Sound::setParam(int id, float value) { impl_->fromUi.push({Cmd::Param, 0, static_cast<uint16_t>(id), value}); }
    void Sound::noteOn(int note, float velocity) { impl_->fromUi.push({Cmd::NoteOn, static_cast<uint8_t>(note), 0, velocity}); }
    void Sound::noteOff(int note) { impl_->fromUi.push({Cmd::NoteOff, static_cast<uint8_t>(note), 0, 0.0f}); }
    void Sound::allNotesOff() { impl_->fromUi.push({Cmd::AllOff, 0, 0, 0.0f}); }

    float Sound::outPeak(int side) const { return impl_->out[static_cast<size_t>(side & 1)].load(std::memory_order_relaxed); }
    const char *Sound::profile() const { return impl_->prof.name; }
    int Sound::sounding() const { return impl_->voices.load(std::memory_order_relaxed); }
    bool Sound::pollCc(int &channel, int &cc, float &value)
    {
        Cmd c;
        if (!impl_->toUi.pop(c))
            return false;
        channel = static_cast<int>(c.b) & 15;
        cc = c.a;
        value = c.value;
        return true;
    }
    bool Sound::reopenAudio(const io::Settings &settings)
    {
        impl_->audio = impl_->output.open("ObsSynthOne", settings, [this](float *b, int f, int c) { impl_->block(b, f, c); });
        return impl_->audio;
    }
    const io::AudioOut &Sound::audioOut() const { return impl_->output; }
    io::MidiIn &Sound::midiIn() { return impl_->midi; }
} // namespace synth::oneapp
