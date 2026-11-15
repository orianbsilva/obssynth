#pragma once
// CIRCUIT tom and rim. A tom is the kick's resonator tuned higher, its pitch thrown less and longer, with a breath
// of filtered noise (the head); a rim shot is a high, very short resonator with a sharp click. TUNE, DECAY, and A,
// B: for the tom SWEEP (how far the pitch falls) and NOISE; for the rim TONE (a second, inharmonic mode) and CLICK.
// The library's parts: Oscillator, ADSREnvelope, SvfFilter. FLOAT-ONLY.
#include "drum_model.hpp"
#include "../core/svf_filter.hpp"
#include "../../oscillators/adsr_envelope.hpp"
#include "../../oscillators/oscillator.hpp"

#include <algorithm>
#include <cmath>

namespace synth
{
    class CircuitTom : public DrumModel
    {
    public:
        // low, mid, high: where TUNE's range sits
        CircuitTom(float sampleRate, float lowHz, float highHz)
            : sr_(sampleRate), lo_(lowHz), hi_(highHz), osc_(lowHz, WaveType::SINE, sampleRate), amp_(sampleRate), pitch_(sampleRate),
              noiseEnv_(sampleRate), head_(sampleRate, BiquadType::BANDPASS, 900.0f, 0.2f)
        {
            osc_.setAmplitude(1.0f);
            set(DrumParams{});
        }

        void set(const DrumParams &n) override
        {
            p_ = n;
            hz_ = drumExp(n.tune, lo_, hi_);
            decay_ = drumExp(n.decay, 0.08f, 2.0f);
            head_.setCutoff(hz_ * 4.0f);
            head_.setResonance(n.bent ? 0.7f : 0.2f);
        }
        const char *nameA() const override { return "SWEEP"; }
        const char *nameB() const override { return "NOISE"; }

        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.55f + 0.45f * s;
            throw_ = p_.a * (p_.bent ? 3.0f : 1.2f) * (0.6f + 0.4f * s);
            if (amp_.getCurrentAmplitude() < 1e-3f)
                osc_.resetPhase();
            amp_.setADSR(0.0005f, decay_ * (0.85f + 0.3f * s), 0.0f, decay_);
            pitch_.setADSR(0.0f, p_.bent ? 0.25f : 0.08f, 0.0f, 0.05f);
            noiseEnv_.setADSR(0.0005f, 0.05f + 0.1f * p_.b, 0.0f, 0.05f);
            amp_.noteOn();
            pitch_.noteOn();
            noiseEnv_.noteOn();
        }

        bool active() const override { return amp_.isActive(); }

        float process() override
        {
            if (!amp_.isActive())
                return 0.0f;
            osc_.setFrequency(hz_ * std::exp2(throw_ * pitch_.getNextSample()));
            const float body = osc_.getSample() * amp_.getNextSample();
            const float head = head_.process(noise_.next()) * noiseEnv_.getNextSample() * p_.b * 0.6f;
            return 0.7f * (body + head) * level_;
        }

    private:
        [[maybe_unused]] float sr_;
        float lo_, hi_;
        DrumParams p_;
        Oscillator osc_;
        ADSREnvelope amp_, pitch_, noiseEnv_;
        SvfFilter head_;
        DrumNoise noise_{0x9E3779B9u};
        float hz_ = 100.0f, decay_ = 0.4f, level_ = 1.0f, throw_ = 0.0f;
    };

    class CircuitRim : public DrumModel
    {
    public:
        explicit CircuitRim(float sampleRate)
            : sr_(sampleRate), a_(1700.0f, WaveType::TRIANGLE, sampleRate), b_(2700.0f, WaveType::SINE, sampleRate), amp_(sampleRate),
              click_(sampleRate), hp_(sampleRate, BiquadType::HIGHPASS, 3000.0f, 0.3f)
        {
            a_.setAmplitude(1.0f);
            b_.setAmplitude(1.0f);
            set(DrumParams{});
        }

        void set(const DrumParams &n) override
        {
            p_ = n;
            hz_ = drumExp(n.tune, 700.0f, 2600.0f);
            decay_ = drumExp(n.decay, 0.01f, 0.25f);
            a_.setFrequency(hz_);
            b_.setFrequency(hz_ * (n.bent ? 2.37f : 1.58f));
            hp_.setResonance(n.bent ? 0.75f : 0.3f);
        }
        const char *nameA() const override { return "TONE"; }
        const char *nameB() const override { return "CLICK"; }

        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.5f + 0.5f * s;
            a_.resetPhase();
            b_.resetPhase();
            amp_.setADSR(0.0001f, decay_, 0.0f, decay_);
            click_.setADSR(0.0f, 0.002f, 0.0f, 0.002f);
            amp_.noteOn();
            click_.noteOn();
        }

        bool active() const override { return amp_.isActive(); }

        float process() override
        {
            if (!amp_.isActive())
                return 0.0f;
            const float body = (a_.getSample() * (1.0f - p_.a) + b_.getSample() * p_.a) * amp_.getNextSample();
            const float click = hp_.process(noise_.next()) * click_.getNextSample() * p_.b;
            return 0.6f * (body + click) * level_;
        }

    private:
        [[maybe_unused]] float sr_;
        DrumParams p_;
        Oscillator a_, b_;
        ADSREnvelope amp_, click_;
        SvfFilter hp_;
        DrumNoise noise_{0x7F4A7C15u};
        float hz_ = 1700.0f, decay_ = 0.05f, level_ = 1.0f;
    };
} // namespace synth
