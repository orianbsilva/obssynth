#pragma once
// CIRCUIT snare: two struck resonators (the drum's body: its two lowest modes, about an octave and a bit apart,
// each with its own small pitch drop and its own short decay) and noise through a high-pass (the snares: wires on
// the bottom head), mixed. TONE weighs the two body modes, SNAPPY the wires; harder hits are brighter, wirier and
// throw the body's pitch further. The library's parts: Oscillator (the modes), ADSREnvelope (each decay),
// SvfFilter (the wires' high-pass and a gentle low-pass over them). "bent": the modes detuned off their ratio and
// ringing long, the wires resonant. FLOAT-ONLY.
#include "drum_model.hpp"
#include "../core/svf_filter.hpp"
#include "../../oscillators/adsr_envelope.hpp"
#include "../../oscillators/oscillator.hpp"

#include <algorithm>
#include <cmath>

namespace synth
{
    class CircuitSnare : public DrumModel
    {
    public:
        struct Params
        {
            float tune = 185.0f;  // Hz, the low mode (120..320)
            float decay = 0.22f;  // s, the wires' (the body is shorter)
            float tone = 0.45f;   // 0: the low mode, 1: the high
            float snappy = 0.6f;  // the wires, 0..1
            bool bent = false;
        };

        explicit CircuitSnare(float sampleRate)
            : sr_(sampleRate), lo_(185.0f, WaveType::SINE, sampleRate), hi_(300.0f, WaveType::SINE, sampleRate), bodyEnv_(sampleRate),
              hiEnv_(sampleRate), pitch_(sampleRate), wiresEnv_(sampleRate), wiresHp_(sampleRate, BiquadType::HIGHPASS, 1800.0f, 0.1f),
              wiresLp_(sampleRate, BiquadType::LOWPASS, 9000.0f, 0.0f)
        {
            lo_.setAmplitude(1.0f); // (the Oscillator's default is 0.5)
            hi_.setAmplitude(1.0f);
            setPhysical(Params{});
        }

        // TUNE 120..320 Hz, DECAY 0.05..1 s (the wires), A TONE, B SNAPPY
        void set(const DrumParams &n) override
        {
            Params p;
            p.tune = drumExp(n.tune, 120.0f, 320.0f);
            p.decay = drumExp(n.decay, 0.05f, 1.0f);
            p.tone = n.a;
            p.snappy = n.b;
            p.bent = n.bent;
            setPhysical(p);
        }
        const char *nameA() const override { return "TONE"; }
        const char *nameB() const override { return "SNAPPY"; }

        void setPhysical(const Params &p)
        {
            p_ = p;
            p_.tune = std::clamp(p.tune, 80.0f, 500.0f);
            p_.decay = std::clamp(p.decay, 0.03f, 2.0f);
            ratio_ = p_.bent ? 1.47f : 1.62f;
            wiresHp_.setResonance(p_.bent ? 0.7f : 0.1f);
        }
        const Params &params() const { return p_; }

        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.55f + 0.45f * s;
            throw_ = 0.35f + 0.5f * s; // octaves: up to ~0.85 at the strike, falling in ~15 ms
            if (bodyEnv_.getCurrentAmplitude() < 1e-3f)
            {
                lo_.resetPhase();
                hi_.resetPhase();
            }
            const float body = p_.bent ? 0.5f : 0.09f + 0.05f * s;
            bodyEnv_.setADSR(0.0003f, body, 0.0f, body);
            hiEnv_.setADSR(0.0003f, body * 0.7f, 0.0f, body * 0.7f);
            pitch_.setADSR(0.0f, 0.015f, 0.0f, 0.015f);
            wiresEnv_.setADSR(0.0008f, p_.decay * (0.8f + 0.35f * s), 0.0f, p_.decay);
            // harder: brighter wires
            wiresHp_.setCutoff(1300.0f + 2200.0f * p_.tone + 800.0f * s);
            wiresLp_.setCutoff(6000.0f + 7000.0f * s);
            wiresAmt_ = p_.snappy * (0.6f + 0.4f * s);
            bodyEnv_.noteOn();
            hiEnv_.noteOn();
            pitch_.noteOn();
            wiresEnv_.noteOn();
        }

        bool active() const override { return bodyEnv_.isActive() || wiresEnv_.isActive(); }

        float process() override
        {
            if (!active())
                return 0.0f;
            const float pe = std::exp2(throw_ * pitch_.getNextSample());
            lo_.setFrequency(p_.tune * pe);
            hi_.setFrequency(p_.tune * ratio_ * pe);
            const float body = lo_.getSample() * (1.0f - p_.tone) * bodyEnv_.getNextSample() + hi_.getSample() * p_.tone * hiEnv_.getNextSample();
            const float wires = wiresLp_.process(wiresHp_.process(noise_.next())) * wiresEnv_.getNextSample() * wiresAmt_;
            return (0.5f * body + 0.55f * wires) * level_; // about 0.7 at the loudest: headroom for the kit's mix
        }

    private:
        [[maybe_unused]] float sr_;
        Params p_;
        Oscillator lo_, hi_;
        ADSREnvelope bodyEnv_, hiEnv_, pitch_, wiresEnv_;
        SvfFilter wiresHp_, wiresLp_;
        DrumNoise noise_{0x2B7E1516u};
        float level_ = 1.0f, throw_ = 0.0f, wiresAmt_ = 0.0f, ratio_ = 1.62f;
    };
} // namespace synth
