#pragma once
// CIRCUIT kick: the bridged-T resonator of the classic analog kicks, by its behaviour. A pulse strikes a filter on the
// edge of oscillation: a sine rings and decays, its pitch thrown up at the strike and falling back within a few
// tens of milliseconds (harder hits throw it higher: the "boom" that comes in from above). A short burst of
// high-passed noise is the beater's click; a tube drives the whole. The library's parts: Oscillator (the sine),
// ADSREnvelope (amplitude, pitch, click: decays only), SvfFilter (the click's high-pass), TubeSaturation (drive).
// "bent": the sweep up to six octaves and slower, the click resonant, the drive pushed: past what the circuit
// allowed. FLOAT-ONLY.
#include "drum_model.hpp"
#include "../core/svf_filter.hpp"
#include "../../oscillators/adsr_envelope.hpp"
#include "../../oscillators/oscillator.hpp"
#include "../../effects/waveshaper.hpp"

#include <algorithm>
#include <cmath>

namespace synth
{
    class CircuitKick : public DrumModel
    {
    public:
        struct Params
        {
            float tune = 52.0f;  // Hz, where the pitch settles (30..120)
            float decay = 0.5f;  // s, to -60 dB (0.05..3)
            float sweep = 0.5f;  // how far the pitch is thrown: 0..1 = 0..4 octaves (bent: 6)
            float attack = 0.5f; // the click, 0..1
            float drive = 0.3f;  // 0..1
            bool bent = false;
        };

        explicit CircuitKick(float sampleRate)
            : sr_(sampleRate), osc_(52.0f, WaveType::SINE, sampleRate), amp_(sampleRate), pitch_(sampleRate), click_(sampleRate),
              clickHp_(sampleRate, BiquadType::HIGHPASS, 2500.0f, 0.15f), tube_(sampleRate)
        {
            osc_.setAmplitude(1.0f); // (the Oscillator's default is 0.5)
            setPhysical(Params{});
        }

        // the panel's knobs: TUNE 30..120 Hz, DECAY 0.05..3 s, A ATTACK (the click, and the sweep with it), B DRIVE
        void set(const DrumParams &n) override
        {
            Params p;
            p.tune = drumExp(n.tune, 30.0f, 120.0f);
            p.decay = drumExp(n.decay, 0.05f, 3.0f);
            p.attack = n.a;
            p.sweep = 0.3f + 0.5f * n.a;
            p.drive = n.b;
            p.bent = n.bent;
            setPhysical(p);
        }
        const char *nameA() const override { return "ATTACK"; }
        const char *nameB() const override { return "DRIVE"; }

        void setPhysical(const Params &p)
        {
            p_ = p;
            p_.tune = std::clamp(p.tune, 20.0f, 200.0f);
            p_.decay = std::clamp(p.decay, 0.03f, 4.0f);
            tube_.setModel(TubeModel::AX7);
            const float d = 1.0f + p_.drive * (p_.bent ? 9.0f : 4.0f);
            tube_.setDrive(d);
            tube_.setMix(p_.drive > 0.001f ? 1.0f : 0.0f);
            // the tube gives back about 1.5/drive at full level (measured): made up, so drive is colour, not loudness
            makeup_ = std::max(1.0f, d / 1.5f);
            clickHp_.setResonance(p_.bent ? 0.75f : 0.15f);
        }
        const Params &params() const { return p_; }

        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.55f + 0.45f * s;
            throw_ = p_.sweep * (p_.bent ? 6.0f : 4.0f) * (0.55f + 0.45f * s); // octaves at the strike
            if (amp_.getCurrentAmplitude() < 1e-3f)
                osc_.resetPhase(); // from silence: the same strike every time; a roll keeps ringing
            amp_.setADSR(0.0004f, p_.decay * (0.85f + 0.3f * s), 0.0f, p_.decay);
            pitch_.setADSR(0.0f, (p_.bent ? 0.09f : 0.035f) * (0.7f + 0.6f * p_.sweep), 0.0f, 0.03f);
            click_.setADSR(0.0f, 0.004f + 0.004f * p_.attack, 0.0f, 0.004f);
            amp_.noteOn();
            pitch_.noteOn();
            click_.noteOn();
            clickAmt_ = p_.attack * (0.4f + 0.6f * s);
        }

        bool active() const override { return amp_.isActive(); }

        float process() override
        {
            if (!amp_.isActive())
                return 0.0f;
            const float pe = pitch_.getNextSample();
            osc_.setFrequency(p_.tune * std::exp2(throw_ * pe));
            float y = osc_.getSample() * amp_.getNextSample();
            const float c = click_.getNextSample();
            if (c > 0.0f)
                y += clickHp_.process(noise_.next()) * c * clickAmt_ * 0.5f;
            y *= level_;
            if (p_.drive > 0.001f)
                y = tube_.process(y) * makeup_;
            return 0.72f * y; // about 0.7 at the loudest: headroom for the kit's mix
        }

    private:
        [[maybe_unused]] float sr_;
        Params p_;
        Oscillator osc_;
        ADSREnvelope amp_, pitch_, click_;
        SvfFilter clickHp_;
        TubeSaturation tube_;
        DrumNoise noise_{0x1F2E3D4Cu};
        float level_ = 1.0f, throw_ = 0.0f, clickAmt_ = 0.0f, makeup_ = 1.0f;
    };
} // namespace synth
