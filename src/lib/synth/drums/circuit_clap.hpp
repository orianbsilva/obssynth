#pragma once
// CIRCUIT clap: many hands are noise struck a few times in quick succession, then a room's tail. The classic circuits
// retrigger one noise envelope three or four times about ten milliseconds apart (a sawtooth-like envelope) through a
// band-pass, and add a longer, filtered decay. TUNE: the band's centre, DECAY: the tail, A SPREAD: the time between
// the hands, B TONE: how bright (a high-pass under the band). Bent: irregular hands, more of them, a ringing band.
// The library's parts: SvfFilter (band and high-pass), ADSREnvelope (each burst, the tail). FLOAT-ONLY.
#include "drum_model.hpp"
#include "../core/svf_filter.hpp"
#include "../../oscillators/adsr_envelope.hpp"

#include <algorithm>

namespace synth
{
    class CircuitClap : public DrumModel
    {
    public:
        explicit CircuitClap(float sampleRate)
            : sr_(sampleRate), burst_(sampleRate), tail_(sampleRate), band_(sampleRate, BiquadType::BANDPASS, 1200.0f, 0.35f),
              hp_(sampleRate, BiquadType::HIGHPASS, 500.0f, 0.0f)
        {
            set(DrumParams{});
        }

        void set(const DrumParams &n) override
        {
            p_ = n;
            band_.setCutoff(drumExp(n.tune, 700.0f, 2600.0f));
            band_.setResonance(n.bent ? 0.8f : 0.35f);
            hp_.setCutoff(drumExp(n.b, 200.0f, 2500.0f));
            tailTime_ = drumExp(n.decay, 0.08f, 1.2f);
            spread_ = drumExp(n.a, 0.004f, 0.025f);
            bursts_ = n.bent ? 6 : 4;
        }
        const char *nameA() const override { return "SPREAD"; }
        const char *nameB() const override { return "TONE"; }

        void trigger(const DrumHit &hit) override
        {
            const float s = hit.strength();
            level_ = 0.5f + 0.5f * s;
            left_ = bursts_;
            next_ = 0.0f;
            t_ = 0;
            tail_.setADSR(0.002f, tailTime_ * (0.8f + 0.4f * s), 0.0f, tailTime_);
            burst_.setADSR(0.0002f, 0.007f, 0.0f, 0.007f);
        }

        bool active() const override { return left_ > 0 || burst_.isActive() || tail_.isActive(); }

        float process() override
        {
            if (!active())
                return 0.0f;
            if (left_ > 0 && static_cast<float>(t_) >= next_)
            {
                burst_.noteOn();
                --left_;
                float gap = spread_ * sr_;
                if (p_.bent) // irregular hands
                    gap *= 0.5f + noise_.next() * 0.45f + 0.5f;
                next_ = static_cast<float>(t_) + gap;
                if (left_ == 0)
                    tail_.noteOn(); // the room after the last hand
            }
            ++t_;
            const float x = hp_.process(band_.process(noise_.next()));
            return 0.7f * x * (burst_.getNextSample() + 0.55f * tail_.getNextSample()) * level_ * 1.6f;
        }

    private:
        float sr_;
        DrumParams p_;
        ADSREnvelope burst_, tail_;
        SvfFilter band_, hp_;
        DrumNoise noise_{0x3C6EF372u};
        float level_ = 1.0f, tailTime_ = 0.3f, spread_ = 0.01f, next_ = 0.0f;
        int bursts_ = 4, left_ = 0;
        long t_ = 0;
    };
} // namespace synth
