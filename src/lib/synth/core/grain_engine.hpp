#pragma once
// Granular synthesis: short windowed grains read from a recording, scattered around a position. The source is
// one buffer, read-only, shared by every voice (load it before the audio starts; swap it whole). A grain plays
// the source faster or slower with the note (the recording's own speed at its root, C4 unless said), under a
// Hann window from a table; grains start at the density's pace with a little jitter, each from the position
// moved at random by the spray. The level follows the overlap (density x size) so a cloud is not louder than
// a single grain. No allocation, no lock, no trig per sample. FLOAT-ONLY.
#include <algorithm>
#include <cctype>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace synth
{
    struct GrainSource
    {
        std::vector<float> samples; // mono, -1..1
        float rate = 48000.0f;      // the recording's sample rate
        float rootHz = 261.626f;    // the pitch it plays at its own speed (C4)
        std::string name;

        // a WAV file: PCM 16/24/32-bit or IEEE float, any channel count (mixed to mono), peak-normalized
        static std::shared_ptr<GrainSource> loadWav(const std::string &path)
        {
            std::FILE *f = std::fopen(path.c_str(), "rb");
            if (!f)
                return nullptr;
            std::vector<uint8_t> b;
            uint8_t chunk[65536];
            size_t n;
            while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0)
                b.insert(b.end(), chunk, chunk + n);
            std::fclose(f);
            if (b.size() < 44 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4))
                return nullptr;
            auto u16 = [&](size_t i) { return static_cast<uint32_t>(b[i] | b[i + 1] << 8); };
            auto u32 = [&](size_t i) { return u16(i) | u16(i + 2) << 16; };
            uint32_t format = 0, channels = 0, rate = 0, bits = 0;
            size_t data = 0, dataLen = 0;
            for (size_t i = 12; i + 8 <= b.size();)
            {
                const uint32_t len = u32(i + 4);
                if (!std::memcmp(b.data() + i, "fmt ", 4) && i + 24 <= b.size())
                {
                    format = u16(i + 8);
                    channels = u16(i + 10);
                    rate = u32(i + 12);
                    bits = u16(i + 22);
                    if (format == 0xFFFE && i + 34 <= b.size()) // extensible: the sub-format's first two bytes
                        format = u16(i + 32);
                }
                else if (!std::memcmp(b.data() + i, "data", 4))
                {
                    data = i + 8;
                    dataLen = std::min<size_t>(len, b.size() - data);
                }
                i += 8 + len + (len & 1);
            }
            if (!data || !channels || !rate || (format != 1 && format != 3))
                return nullptr;
            const size_t bytes = bits / 8, frame = bytes * channels, frames = dataLen / frame;
            auto src = std::make_shared<GrainSource>();
            src->rate = static_cast<float>(rate);
            src->samples.resize(frames);
            float peak = 0.0f;
            for (size_t k = 0; k < frames; ++k)
            {
                float sum = 0.0f;
                for (uint32_t c = 0; c < channels; ++c)
                {
                    const uint8_t *p = b.data() + data + k * frame + c * bytes;
                    float v = 0.0f;
                    if (format == 3 && bits == 32)
                        std::memcpy(&v, p, 4);
                    else if (bits == 16)
                        v = static_cast<float>(static_cast<int16_t>(p[0] | p[1] << 8)) / 32768.0f;
                    else if (bits == 24)
                        v = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(p[0] << 8 | p[1] << 16 | p[2] << 24)) >> 8) / 8388608.0f;
                    else if (bits == 32)
                        v = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24))) / 2147483648.0f;
                    sum += v;
                }
                src->samples[k] = sum / static_cast<float>(channels);
                peak = std::max(peak, std::fabs(src->samples[k]));
            }
            if (peak > 1e-6f)
                for (float &s : src->samples)
                    s /= peak;
            const size_t slash = path.find_last_of('/');
            src->name = slash == std::string::npos ? path : path.substr(slash + 1);
            src->rootHz = rootFromName(src->name);
            return src;
        }

        // the pitch a recording plays at its own speed, from a note in its name: C4, Ds4 or D#4, Bb2 (-, _, or
        // the name's ends around it); C4 without one
        static float rootFromName(const std::string &name)
        {
            const std::string n = name.substr(0, name.rfind('.'));
            for (size_t i = 0; i < n.size(); ++i)
            {
                if (i > 0 && n[i - 1] != '-' && n[i - 1] != '_' && n[i - 1] != ' ')
                    continue;
                const char *letters = "CDEFGAB";
                const char *at = std::strchr(letters, n[i]);
                if (!at || n[i] == '\0')
                    continue;
                const int semis[7] = {0, 2, 4, 5, 7, 9, 11};
                int note = semis[at - letters];
                size_t j = i + 1;
                if (j < n.size() && (n[j] == '#' || n[j] == 's'))
                    ++note, ++j;
                else if (j < n.size() && n[j] == 'b')
                    --note, ++j;
                if (j >= n.size() || !std::isdigit(static_cast<unsigned char>(n[j])))
                    continue;
                const int octave = n[j] - '0';
                ++j;
                if (j < n.size() && n[j] != '-' && n[j] != '_' && n[j] != ' ')
                    continue;
                const int midi = 12 * (octave + 1) + note;
                return 440.0f * std::exp2(static_cast<float>(midi - 69) / 12.0f);
            }
            return 261.626f;
        }
    };

    class GrainEngine
    {
    public:
        static constexpr int MAX_GRAINS = 32;

        void setSampleRate(float sr)
        {
            sr_ = sr;
            (void)table(); // built here, not on the audio thread's first grain
        }
        void setSource(std::shared_ptr<const GrainSource> s) { src_ = std::move(s); }
        bool hasSource() const { return src_ && src_->samples.size() > 64; }
        const GrainSource *source() const { return src_.get(); }

        // 0..1 through the recording; the matrix's offset on top (±1 = the whole recording)
        void setPosition(float p) { position_ = std::clamp(p, 0.0f, 1.0f); }
        void setPositionMod(float m) { position_mod_ = m; }
        void setSize(float seconds) { size_ = std::clamp(seconds, 0.005f, 1.0f); }
        void setDensity(float perSecond) { density_ = std::clamp(perSecond, 0.5f, 200.0f); }
        void setSpray(float s) { spray_ = std::clamp(s, 0.0f, 1.0f); }
        float position() const { return position_; }

        void noteOn(uint32_t seed, bool fresh = true)
        {
            rng_ = seed | 1u;
            if (!fresh)
                return;
            for (auto &g : grains_)
                g.active = false;
            until_next_ = 0.0f; // a grain at once
        }

        // one output sample; hz: the note's frequency now (pitch modulation and drift included)
        float process(float hz)
        {
            if (!hasSource())
                return 0.0f;
            const std::vector<float> &s = src_->samples;
            const float n = static_cast<float>(s.size());
            const float inc = hz / src_->rootHz * src_->rate / sr_; // source samples per output sample
            until_next_ -= 1.0f;
            if (until_next_ <= 0.0f)
            {
                spawn(n, inc);
                until_next_ += sr_ / density_ * (0.85f + 0.3f * rnd()); // a little jitter: no buzz at the density's rate
            }
            float out = 0.0f;
            for (auto &g : grains_)
            {
                if (!g.active)
                    continue;
                const float w = window(g.age * g.ageStep);
                size_t i = static_cast<size_t>(g.pos);
                const float f = static_cast<float>(g.pos - static_cast<double>(i));
                const float a = s[i % s.size()], b = s[(i + 1) % s.size()];
                out += (a + (b - a) * f) * w;
                g.pos += static_cast<double>(g.inc);
                if (g.pos >= static_cast<double>(n))
                    g.pos -= static_cast<double>(n);
                g.age += 1.0f;
                if (g.age >= g.len)
                    g.active = false;
            }
            // the overlap: about density x size grains at once; x2: a window's mean power is under half
            const float overlap = std::max(1.0f, density_ * size_);
            return 2.0f * out / std::sqrt(overlap);
        }

    private:
        struct Grain
        {
            bool active = false;
            double pos = 0.0;
            float inc = 1.0f, age = 0.0f, len = 1.0f, ageStep = 0.0f;
        };

        void spawn(float n, float inc)
        {
            Grain *g = nullptr;
            for (auto &x : grains_)
                if (!x.active)
                {
                    g = &x;
                    break;
                }
            if (!g) // all playing: this grain is skipped (cutting one mid-window buzzed at the density's rate)
                return;
            const float len = std::max(16.0f, size_ * sr_);
            const float centre = std::clamp(position_ + position_mod_, 0.0f, 1.0f) + (rnd() * 2.0f - 1.0f) * spray_ * 0.5f;
            float start = centre - 0.5f * len * inc / n; // the position is the grain's middle
            start -= std::floor(start);
            g->active = true;
            g->pos = static_cast<double>(start * n);
            g->inc = inc;
            g->age = 0.0f;
            g->len = len;
            g->ageStep = static_cast<float>(kWindow - 1) / len;
        }

        float window(float x) const // x: 0..kWindow-1 through the grain
        {
            const int i = std::min(static_cast<int>(x), kWindow - 2);
            const float f = x - static_cast<float>(i);
            return table()[static_cast<size_t>(i)] + (table()[static_cast<size_t>(i + 1)] - table()[static_cast<size_t>(i)]) * f;
        }
        static constexpr int kWindow = 1024;
        static const std::array<float, kWindow> &table()
        {
            static const std::array<float, kWindow> t = []
            {
                std::array<float, kWindow> w{};
                for (int i = 0; i < kWindow; ++i)
                    w[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(6.2831853f * static_cast<float>(i) / static_cast<float>(kWindow - 1));
                return w;
            }();
            return t;
        }
        float rnd()
        {
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            return static_cast<float>(rng_ >> 8) / 16777216.0f;
        }

        std::shared_ptr<const GrainSource> src_;
        std::array<Grain, MAX_GRAINS> grains_{};
        float sr_ = 48000.0f;
        float position_ = 0.3f, position_mod_ = 0.0f, size_ = 0.08f, density_ = 25.0f, spray_ = 0.1f;
        float until_next_ = 0.0f;
        uint32_t rng_ = 0x2545F491u;
    };
} // namespace synth
