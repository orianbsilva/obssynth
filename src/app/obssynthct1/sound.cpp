#include "sound.hpp"

#include "../common/audio_constants.hpp"
#include "../common/platform_io.hpp"
#include "../../products/obs_ct1/obs_synth_ct1.hpp"

#include <algorithm>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <poll.h>
#include <unistd.h>
#endif

// The threads: the audio callback is the engine's only writer. The UI and the MIDI threads put commands in queues
// of their own (one producer, one consumer, no lock, no allocation), applied at the start of each block, MIDI first.

namespace synth::ct1app
{
    namespace
    {
        struct Cmd
        {
            enum Type : uint8_t { NoteOn, NoteOff, Param, Control, Bend, AllOff } type;
            uint8_t a;  // note, controller, channel
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
        std::unique_ptr<ct1::Engine> engine;
        io::AudioOut output;
        io::MidiIn midi;
        Spsc<4096> fromUi;
        Spsc<1024> fromMidi;
        Spsc<512> toUi; // the CCs, for the overlay
        bool audio = false;
        std::array<std::atomic<float>, 2> out{};
        std::array<std::atomic<float>, ct1::OPS> pos{};
        std::atomic<int> voices{0};
        // remote (a plugin plays): its socket (Windows: its two pipes), the reader, what the host changed, the writes' lock
        bool linked = false;
        int fd = -1;
#if defined(_WIN32)
        HANDLE rd = nullptr, wr = nullptr;
#endif
        std::thread reader;
        std::atomic<bool> alive{false};
        Spsc<4096> fromHost; // Cmd::Param: b the id (0xFFFF: the program), value
        std::mutex writeLock;

        void send(const std::string &line)
        {
#if defined(_WIN32)
            std::lock_guard<std::mutex> g(writeLock);
            size_t done = 0;
            while (alive && done < line.size())
            {
                DWORD w = 0;
                if (!WriteFile(wr, line.data() + done, static_cast<DWORD>(line.size() - done), &w, nullptr) || w == 0)
                {
                    alive = false;
                    break;
                }
                done += w;
            }
#elif defined(__unix__) || defined(__APPLE__)
            std::lock_guard<std::mutex> g(writeLock);
            size_t done = 0;
            while (alive && done < line.size())
            {
                const ssize_t w = ::write(fd, line.data() + done, line.size() - done);
                if (w <= 0)
                {
                    alive = false;
                    break;
                }
                done += static_cast<size_t>(w);
            }
#else
            (void)line;
#endif
        }

        // some bytes from the plugin, waiting a little at most; false: it is gone
        bool receive(std::string &buf)
        {
            char chunk[4096];
#if defined(_WIN32)
            DWORD avail = 0;
            if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr))
                return false;
            if (avail == 0)
            {
                Sleep(15);
                return true;
            }
            DWORD got = 0;
            if (!ReadFile(rd, chunk, avail < sizeof chunk ? avail : static_cast<DWORD>(sizeof chunk), &got, nullptr) || got == 0)
                return false;
            buf.append(chunk, got);
            return true;
#elif defined(__unix__) || defined(__APPLE__)
            pollfd pf{fd, POLLIN, 0};
            if (::poll(&pf, 1, 200) <= 0)
                return true;
            const ssize_t r = ::read(fd, chunk, sizeof chunk);
            if (r <= 0)
                return false;
            buf.append(chunk, static_cast<size_t>(r));
            return true;
#else
            (void)buf, (void)chunk;
            return false;
#endif
        }

        void read()
        {
            std::string buf;
            while (alive)
            {
                if (!receive(buf))
                {
                    alive = false;
                    break;
                }
                for (size_t nl; (nl = buf.find('\n')) != std::string::npos; buf.erase(0, nl + 1))
                {
                    const std::string line = buf.substr(0, nl);
                    int id = 0, v = 0;
                    float x = 0, a = 0, b = 0, q[ct1::OPS]{};
                    if (std::sscanf(line.c_str(), "v %d %f", &id, &x) == 2)
                        fromHost.push({Cmd::Param, 0, static_cast<uint16_t>(id), x});
                    else if (std::sscanf(line.c_str(), "P %f", &x) == 1)
                        fromHost.push({Cmd::Param, 0, 0xFFFF, x});
                    else if (std::sscanf(line.c_str(), "s %d %f %f %f %f %f %f %f", &v, &a, &b, &q[0], &q[1], &q[2], &q[3], &q[4]) == 8)
                    {
                        voices.store(v), out[0].store(a), out[1].store(b);
                        for (int o = 0; o < ct1::OPS; ++o)
                            pos[static_cast<size_t>(o)].store(q[o]);
                    }
                }
            }
        }

        void apply(const Cmd &c)
        {
            ct1::Engine &e = *engine;
            switch (c.type)
            {
            case Cmd::NoteOn: e.noteOn(c.a, c.value); break;
            case Cmd::NoteOff: e.noteOff(c.a); break;
            case Cmd::Param: e.setParam(c.b, c.value); break;
            case Cmd::Bend: e.setPitchBend(c.value); break;
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
                engine->render(stereo, n);
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
            for (int o = 0; o < ct1::OPS; ++o)
                pos[static_cast<size_t>(o)].store(engine->livePos(o), std::memory_order_relaxed);
            voices.store(engine->sounding(), std::memory_order_relaxed);
        }
    };

    Sound::Sound() : impl_(std::make_unique<Impl>()) {}

    Sound::~Sound()
    {
        if (impl_->linked)
        {
            impl_->alive = false;
            if (impl_->reader.joinable())
                impl_->reader.join();
#if defined(_WIN32)
            CloseHandle(impl_->rd);
            CloseHandle(impl_->wr);
#elif defined(__unix__) || defined(__APPLE__)
            ::close(impl_->fd);
#endif
        }
        impl_->midi.close(); // no more commands from MIDI
        impl_->output.close();
    }

    bool Sound::connect(const char *link)
    {
#if defined(_WIN32)
        unsigned long long r = 0, w = 0;
        if (std::sscanf(link, "%llu,%llu", &r, &w) != 2)
            return false;
        impl_->rd = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(r));
        impl_->wr = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(w));
#else
        impl_->fd = std::atoi(link);
#endif
        impl_->linked = true;
        impl_->alive = true;
        impl_->reader = std::thread([this] { impl_->read(); });
        return true;
    }
    bool Sound::remote() const { return impl_->linked; }
    bool Sound::remoteAlive() const { return impl_->alive; }
    bool Sound::pollRemote(int &id, float &value)
    {
        Cmd c;
        if (!impl_->fromHost.pop(c))
            return false;
        id = c.b == 0xFFFF ? -1 : c.b;
        value = c.value;
        return true;
    }

    bool Sound::start(const std::vector<std::shared_ptr<const ct1::Table>> &banks, const float *params, const io::Settings &settings)
    {
        // before any audio thread: the engine is still this thread's alone
        impl_->engine = std::make_unique<ct1::Engine>(static_cast<float>(SAMPLE_RATE), banks);
        for (int i = 0; i < ct1::NUM_PARAMS; ++i)
            impl_->engine->setParam(i, params[i]);
        impl_->audio = impl_->output.open("ObsSynthCT1", settings, [this](float *b, int f, int c) { impl_->block(b, f, c); });
        if (!impl_->audio)
            std::cerr << "[obssynthct1] no audio output" << std::endl;
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
        impl_->midi.open("ObsSynthCT1", std::move(h), settings);
        return impl_->audio;
    }

    void Sound::setParam(int id, float value)
    {
        if (remote())
            return impl_->send("p " + std::to_string(id) + " " + std::to_string(value) + "\n");
        impl_->fromUi.push({Cmd::Param, 0, static_cast<uint16_t>(id), value});
    }
    void Sound::noteOn(int note, float velocity)
    {
        if (remote())
            return impl_->send("n " + std::to_string(note) + " " + std::to_string(velocity) + "\n");
        impl_->fromUi.push({Cmd::NoteOn, static_cast<uint8_t>(note), 0, velocity});
    }
    void Sound::noteOff(int note)
    {
        if (remote())
            return impl_->send("o " + std::to_string(note) + "\n");
        impl_->fromUi.push({Cmd::NoteOff, static_cast<uint8_t>(note), 0, 0.0f});
    }
    void Sound::allNotesOff() { impl_->fromUi.push({Cmd::AllOff, 0, 0, 0.0f}); }

    float Sound::outPeak(int side) const { return impl_->out[static_cast<size_t>(side & 1)].load(std::memory_order_relaxed); }
    float Sound::livePos(int op) const { return impl_->pos[static_cast<size_t>(std::clamp(op, 0, ct1::OPS - 1))].load(std::memory_order_relaxed); }
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
        impl_->audio = impl_->output.open("ObsSynthCT1", settings, [this](float *b, int f, int c) { impl_->block(b, f, c); });
        return impl_->audio;
    }
    const io::AudioOut &Sound::audioOut() const { return impl_->output; }
    io::MidiIn &Sound::midiIn() { return impl_->midi; }
} // namespace synth::ct1app
