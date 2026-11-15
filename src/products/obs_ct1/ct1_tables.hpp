#pragma once
// ObsSynthCT1's tables: a classic shape (one band-limited frame) or a wavetable bank (pcm/bank_*.wav), each frame
// kept per octave (a mip: only the harmonics that fit under Nyquist at the oversampled rate for the frequency read),
// read at a position across the frames and a phase, linear in both. Built off the audio thread, read-only after.
#include "../../lib/synth/core/grain_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <complex>
#include <filesystem>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace synth::ct1
{
    constexpr double kPi = 3.14159265358979323846; // M_PI is not standard (MSVC has none without _USE_MATH_DEFINES)

    constexpr int TABLE_N = 1024; // samples a frame; harmonics up to TABLE_N / 2
    constexpr int MIPS = 10;      // mip m keeps the harmonics up to (TABLE_N / 2) >> m

    // ── tables ──────────────────────────────────────────────────────────────────────────────────
    namespace detail
    {
        // in place, radix 2 (TABLE_N is a power of two); built once per table, off the audio thread
        inline void fft(std::vector<std::complex<double>> &a, bool inverse)
        {
            const size_t n = a.size();
            for (size_t i = 1, j = 0; i < n; ++i)
            {
                size_t bit = n >> 1;
                for (; j & bit; bit >>= 1)
                    j ^= bit;
                j ^= bit;
                if (i < j)
                    std::swap(a[i], a[j]);
            }
            for (size_t len = 2; len <= n; len <<= 1)
            {
                const double ang = 2 * kPi / static_cast<double>(len) * (inverse ? 1 : -1);
                const std::complex<double> wl(std::cos(ang), std::sin(ang));
                for (size_t i = 0; i < n; i += len)
                {
                    std::complex<double> w(1);
                    for (size_t k = 0; k < len / 2; ++k)
                    {
                        const auto u = a[i + k], v = a[i + k + len / 2] * w;
                        a[i + k] = u + v;
                        a[i + k + len / 2] = u - v;
                        w *= wl;
                    }
                }
            }
            if (inverse)
                for (auto &x : a)
                    x /= static_cast<double>(n);
        }
    } // namespace detail

    class Table
    {
    public:
        enum Shape { Sine, Saw, Square, Triangle };

        // a bank: frames in a row (pcm/bank_*_<spw>samples_*.wav: 1024 or 2048 a frame; read as TABLE_N)
        static std::shared_ptr<Table> fromBank(const std::string &path, const std::string &name)
        {
            auto src = GrainSource::loadWav(path);
            const int spw = path.find("_2048samples") != std::string::npos ? 2048 : TABLE_N;
            if (!src || src->samples.size() < static_cast<size_t>(spw))
                return nullptr;
            int frames = static_cast<int>(src->samples.size() / static_cast<size_t>(spw));
            // the banks end in silent frames (pcm/: the upper half of some): not frames to read, nor to scale up
            auto silent = [&](int f)
            {
                float peak = 0;
                for (int i = 0; i < spw; ++i)
                    peak = std::max(peak, std::fabs(src->samples[static_cast<size_t>(f * spw + i)]));
                return peak < 1e-4f;
            };
            while (frames > 1 && silent(frames - 1))
                --frames;
            auto t = std::make_shared<Table>();
            t->frames_ = frames;
            t->name_ = name;
            t->data_.assign(static_cast<size_t>(MIPS * frames * TABLE_N), 0.0f);
            std::vector<std::complex<double>> big(static_cast<size_t>(spw)), spec(TABLE_N);
            for (int f = 0; f < frames; ++f)
            {
                for (int i = 0; i < spw; ++i)
                    big[static_cast<size_t>(i)] = static_cast<double>(src->samples[static_cast<size_t>(f * spw + i)]);
                detail::fft(big, false);
                // its harmonics into a TABLE_N spectrum (a 2048 frame keeps the first TABLE_N / 2, as a 1024 one)
                const double scale = static_cast<double>(TABLE_N) / static_cast<double>(spw);
                std::fill(spec.begin(), spec.end(), 0.0);
                for (int k = 1; k < TABLE_N / 2; ++k)
                {
                    spec[static_cast<size_t>(k)] = big[static_cast<size_t>(k)] * scale;
                    spec[static_cast<size_t>(TABLE_N - k)] = std::conj(spec[static_cast<size_t>(k)]);
                }
                t->fill(f, spec); // no DC: a modulator's offset would detune its carrier
            }
            return t;
        }

        // a classic shape, band-limited, as a one-frame table
        static std::shared_ptr<Table> fromShape(Shape s)
        {
            auto t = std::make_shared<Table>();
            t->frames_ = 1;
            t->name_ = s == Sine ? "SINE" : s == Saw ? "SAW" : s == Square ? "SQUARE" : "TRIANGLE";
            t->data_.assign(static_cast<size_t>(MIPS * TABLE_N), 0.0f);
            std::vector<std::complex<double>> spec(TABLE_N, 0.0);
            for (int k = 1; k < TABLE_N / 2; ++k)
            {
                double a = 0; // the sine amplitude of harmonic k
                if (s == Sine)
                    a = k == 1 ? 1 : 0;
                else if (s == Saw)
                    a = 2 / kPi / k * (k % 2 ? 1 : -1);
                else if (s == Square)
                    a = k % 2 ? 4 / kPi / k : 0;
                else
                    a = k % 2 ? 8 / (kPi * kPi) / (k * k) * ((k / 2) % 2 ? -1 : 1) : 0;
                // a sine of amplitude a at bin k: -i a N/2 there, its mirror conjugate
                spec[static_cast<size_t>(k)] = std::complex<double>(0, -a * TABLE_N / 2);
                spec[static_cast<size_t>(TABLE_N - k)] = std::conj(spec[static_cast<size_t>(k)]);
            }
            t->fill(0, spec);
            return t;
        }

        int frames() const { return frames_; }
        const std::string &name() const { return name_; }
        // frame f's sample i at full band (the panel's picture)
        float sample(int frame, int i) const { return at(0, std::clamp(frame, 0, frames_ - 1))[i & (TABLE_N - 1)]; }

        // pos 0..1 across the frames, phase 0..1, the mip for the frequency; linear in both
        float read(int mip, float pos, float phase) const
        {
            const float fp = std::clamp(pos, 0.0f, 1.0f) * static_cast<float>(frames_ - 1);
            const int f0 = static_cast<int>(fp), f1 = std::min(f0 + 1, frames_ - 1);
            const float ff = fp - static_cast<float>(f0);
            const float x = phase * static_cast<float>(TABLE_N);
            const int i0 = static_cast<int>(x) & (TABLE_N - 1), i1 = (i0 + 1) & (TABLE_N - 1);
            const float fx = x - std::floor(x);
            const float *a = at(mip, f0), *b = at(mip, f1);
            const float va = a[i0] + (a[i1] - a[i0]) * fx, vb = b[i0] + (b[i1] - b[i0]) * fx;
            return va + (vb - va) * ff;
        }

        // the mip whose harmonics stay under Nyquist (90%) at this frequency and rate
        static int mipFor(float hz, float rate)
        {
            const float fit = 0.45f * rate / std::max(hz, 1.0f); // harmonics that fit
            int m = 0;
            while (m < MIPS - 1 && static_cast<float>((TABLE_N / 2) >> m) > fit)
                ++m;
            return m;
        }

    private:
        int frames_ = 0;
        std::string name_;
        std::vector<float> data_; // [mip][frame][TABLE_N]

        const float *at(int mip, int frame) const { return data_.data() + (static_cast<size_t>(mip) * static_cast<size_t>(frames_) + static_cast<size_t>(frame)) * TABLE_N; }

        void fill(int frame, const std::vector<std::complex<double>> &spec)
        {
            std::vector<std::complex<double>> s(TABLE_N);
            double peak = 0;
            std::vector<std::vector<double>> mips(MIPS, std::vector<double>(TABLE_N));
            for (int m = 0; m < MIPS; ++m)
            {
                const int keep = (TABLE_N / 2) >> m;
                for (int k = 0; k < TABLE_N; ++k)
                {
                    const int h = k <= TABLE_N / 2 ? k : TABLE_N - k;
                    s[static_cast<size_t>(k)] = h <= keep && h < TABLE_N / 2 ? spec[static_cast<size_t>(k)] : 0.0;
                }
                detail::fft(s, true);
                for (int i = 0; i < TABLE_N; ++i)
                {
                    mips[static_cast<size_t>(m)][static_cast<size_t>(i)] = s[static_cast<size_t>(i)].real();
                    if (m == 0)
                        peak = std::max(peak, std::fabs(s[static_cast<size_t>(i)].real()));
                }
            }
            const double g = peak > 1e-4 ? 1 / peak : 0; // every frame at full scale (its mips at the same gain); silence stays silent
            for (int m = 0; m < MIPS; ++m)
                for (int i = 0; i < TABLE_N; ++i)
                    data_[(static_cast<size_t>(m) * static_cast<size_t>(frames_) + static_cast<size_t>(frame)) * TABLE_N + static_cast<size_t>(i)] =
                        static_cast<float>(mips[static_cast<size_t>(m)][static_cast<size_t>(i)] * g);
        }
    };


    // a bank's name from its file: bank_all_waves_32waves_1024samples_32float.wav -> "ALL WAVES"
    inline std::string bankName(const std::string &file)
    {
        std::string n = std::filesystem::path(file).stem().string();
        if (n.rfind("bank_", 0) == 0)
            n = n.substr(5);
        // up to "_<count>waves" (all_waves_32waves: "all_waves")
        for (size_t i = 0; i + 1 < n.size(); ++i)
            if (n[i] == '_' && std::isdigit(static_cast<unsigned char>(n[i + 1])))
            {
                size_t j = i + 1;
                while (j < n.size() && std::isdigit(static_cast<unsigned char>(n[j])))
                    ++j;
                if (n.compare(j, 5, "waves") == 0)
                {
                    n = n.substr(0, i);
                    break;
                }
            }
        for (auto &c : n)
            c = c == '_' ? ' ' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return n;
    }

    // the wavetable banks of a folder (pcm/bank_*.wav), by name, each name once (a bank in two sizes: the first)
    inline std::vector<std::shared_ptr<const Table>> loadBanks(const std::string &dir)
    {
        std::vector<std::string> files;
        std::error_code ec;
        for (const auto &e : std::filesystem::directory_iterator(dir, ec))
            if (e.path().extension() == ".wav" && e.path().filename().string().rfind("bank_", 0) == 0)
                files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
        std::vector<std::shared_ptr<const Table>> v;
        for (const std::string &f : files)
        {
            const std::string name = bankName(f);
            if (std::any_of(v.begin(), v.end(), [&](const auto &t) { return t->name() == name; }))
                continue;
            if (auto t = Table::fromBank(f, name))
                v.push_back(t);
        }
        return v;
    }
} // namespace synth::ct1
