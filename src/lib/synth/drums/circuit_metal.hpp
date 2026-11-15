#pragma once
// CIRCUIT metal: the hats, cymbals and cowbell of the classic circuits come from a bank of square waves at frequencies
// that make no chord (inharmonic ratios), summed: a "metal" that band-pass and high-pass filters shape. Six squares
// for the hats and cymbals, two for the cowbell. TUNE scales the whole bank, DECAY its envelope (each kind its own
// range), A TONE the filters, B METAL how far the bank's ratios spread (the colour of the metal). A ride adds a
// ringing partial (the bell). Bent: the ratios pulled apart, the band resonant. The library's parts: Oscillator
// (SQUARE, band-limited), SvfFilter, ADSREnvelope. FLOAT-ONLY.
#include "drum_model.hpp"
#include "../core/svf_filter.hpp"
#include "../../oscillators/adsr_envelope.hpp"
#include "../../oscillators/oscillator.hpp"

#include <cstddef>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace synth
{
    class CircuitMetal : public DrumModel
    {
    public:
        enum class Kind
        {
            ClosedHat,
            OpenHat,
            Crash,
            Ride,
            Cowbell
        };

        CircuitMetal(float sampleRate, Kind kind)
            : sr_(sampleRate), kind_(kind), amp_(sampleRate), bell_(800.0f, WaveType::SINE, sampleRate), bellEnv_(sampleRate),
              band_(sampleRate, BiquadType::BANDPASS, 8000.0f, 0.2f), hp_(sampleRate, BiquadType::HIGHPASS, 6000.0f, 0.0f)
        {
            for (auto &o : osc_)
            {
                o = std::make_unique<Oscillator>(400.0f, WaveType::SQUARE, sampleRate);
                o->setAmplitude(1.0f);
            }
            bell_.setAmplitude(1.0f);
            set(DrumParams{});
        }

        void set(const DrumParams &n) override
        {
            p_ = n;
            // the classic bank's frequencies (Hz) at TUNE 0.5, spread by METAL around their mean, all scaled by TUNE
            static constexpr float bank[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};
            const float scale = drumExp(n.tune, 0.6f, 1.7f), spread = drumLin(n.b, 0.6f, 1.4f) * (n.bent ? 1.8f : 1.0f);
            const float mean = 457.0f;
            for (int i = 0; i < 6; ++i)
                hz_[static_cast<size_t>(i)] = (mean + (bank[i] - mean) * spread) * scale;
            if (kind_ == Kind::Cowbell) // two squares, the classic pair, and only them
            {
                hz_[0] = 540.0f * scale;
                hz_[1] = 800.0f * scale * (0.9f + 0.2f * n.b);
            }
            for (int i = 0; i < 6; ++i)
                osc_[static_cast<size_t>(i)]->setFrequency(std::max(20.0f, hz_[static_cast<size_t>(i)]));
            const float tone = n.a;
            switch (kind_)
            {
            case Kind::ClosedHat:
            case Kind::OpenHat:
                band_.setCutoff(drumExp(tone, 5000.0f, 12000.0f));
                hp_.setCutoff(drumExp(tone, 4000.0f, 9000.0f));
                decay_ = kind_ == Kind::ClosedHat ? drumExp(n.decay, 0.02f, 0.25f) : drumExp(n.decay, 0.15f, 1.6f);
                break;
            case Kind::Crash:
                band_.setCutoff(drumExp(tone, 3000.0f, 9000.0f));
                hp_.setCutoff(drumExp(tone, 1500.0f, 5000.0f));
                decay_ = drumExp(n.decay, 0.6f, 5.0f);
                break;
            case Kind::Ride:
                band_.setCutoff(drumExp(tone, 4000.0f, 10000.0f));
                hp_.setCutoff(drumExp(tone, 2500.0f, 6000.0f));
                decay_ = drumExp(n.decay, 0.8f, 6.0f);
                bell_.setFrequency(hz_[5] * 3.1f);
                break;
            case Kind::Cowbell:
                band_.setCutoff(drumExp(tone, 1200.0f, 4000.0f));
                hp_.setCutoff(300.0f);
                decay_ = drumExp(n.decay, 0.08f, 0.8f);
                break;
            }
            band_.setResonance(n.bent ? 0.8f : (kind_ == Kind::Cowbell ? 0.45f : 0.2f));
        }
        const char *nameA() const override { return "TONE"; }
        const char *nameB() const override { return "METAL"; }

        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.5f + 0.5f * s;
            const float d = decay_ * (0.85f + 0.3f * s);
            if (kind_ == Kind::Cowbell) // a quick drop and a short ring
                amp_.setADSR(0.0003f, d, 0.0f, d);
            else
                amp_.setADSR(kind_ == Kind::Crash ? 0.002f : 0.0004f, d, 0.0f, d);
            bellEnv_.setADSR(0.0005f, d * 0.6f, 0.0f, d * 0.6f);
            amp_.noteOn();
            if (kind_ == Kind::Ride)
                bellEnv_.noteOn();
        }

        bool active() const override { return amp_.isActive(); }

        float process() override
        {
            if (!amp_.isActive())
                return 0.0f;
            const int n = kind_ == Kind::Cowbell ? 2 : 6;
            float x = 0.0f;
            for (int i = 0; i < n; ++i)
                x += osc_[static_cast<size_t>(i)]->getSample();
            x /= static_cast<float>(n);
            // the cowbell keeps its body (the band only); the rest lose their lows to the high-pass
            float y = kind_ == Kind::Cowbell ? band_.process(x) * 1.6f : hp_.process(band_.process(x)) * 3.0f;
            y *= amp_.getNextSample();
            if (kind_ == Kind::Ride)
                y += 0.18f * bell_.getSample() * bellEnv_.getNextSample() * (1.0f - 0.5f * p_.a);
            return 0.7f * y * level_;
        }

    private:
        [[maybe_unused]] float sr_;
        Kind kind_;
        DrumParams p_;
        std::array<std::unique_ptr<Oscillator>, 6> osc_;
        std::array<float, 6> hz_{};
        ADSREnvelope amp_;
        Oscillator bell_;
        ADSREnvelope bellEnv_;
        SvfFilter band_, hp_;
        float decay_ = 0.1f, level_ = 1.0f;
    };
} // namespace synth
