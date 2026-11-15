#pragma once
// A state-variable filter in the topology-preserving form (Zavalishin; Simper's equations): it stays stable
// and in tune while its cutoff moves every sample, which the chains do (envelope, matrix). Its band state is
// saturated, softly: resonance swells and bites as it rises and self-oscillation stays bounded, where a
// biquad only rang cleaner and louder. LP, HP, BP and notch, as BiquadType names them. FLOAT-ONLY.
#include "biquad_filter.hpp"

#include <algorithm>
#include <cmath>

namespace synth
{
    class SvfFilter
    {
    public:
        SvfFilter(float sample_rate, BiquadType type = BiquadType::LOWPASS, float cutoff = 10000.0f, float resonance = 0.0f)
            : sr_(sample_rate), type_(type), cutoff_(cutoff), res_(resonance)
        {
            update();
        }

        void setSampleRate(float sample_rate)
        {
            sr_ = sample_rate;
            update();
        }
        void setType(BiquadType type) { type_ = type; }
        void setCutoff(float hz)
        {
            if (hz == cutoff_)
                return;
            cutoff_ = hz;
            updateG();
        }
        // 0: flat (Q 0.707); 1: at the edge of self-oscillation
        void setResonance(float r)
        {
            res_ = std::clamp(r, 0.0f, 1.0f);
            k_ = 1.414f - 1.37f * res_;
            updateA();
        }
        void reset() { ic1_ = ic2_ = 0.0f; }

        float getCutoff() const { return cutoff_; }
        float getResonance() const { return res_; }
        BiquadType getType() const { return type_; }

        /// The cutoff reached over n samples: the coefficients glide there linearly (a control rate: the tan and the
        /// division once per step, not every sample). setCutoff jumps, and stops a glide.
        void setCutoffRamp(float hz, int n)
        {
            if (n <= 1)
            {
                setCutoff(hz);
                return;
            }
            cutoff_ = hz;
            const float g = gFor(hz);
            const float t1 = 1.0f / (1.0f + g * (g + k_)), t2 = g * t1, t3 = g * t2;
            const float inv = 1.0f / static_cast<float>(n);
            d1_ = (t1 - a1_) * inv;
            d2_ = (t2 - a2_) * inv;
            d3_ = (t3 - a3_) * inv;
            g_ = g;
            ramp_left_ = n;
        }

        float process(float v0)
        {
            if (ramp_left_ > 0)
            {
                a1_ += d1_;
                a2_ += d2_;
                a3_ += d3_;
                if (--ramp_left_ == 0)
                    updateA(); // at the step's end exactly where it was going
            }
            const float v3 = v0 - ic2_;
            const float v1 = a1_ * ic1_ + a2_ * v3;
            const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
            ic1_ = soft(2.0f * v1 - ic1_);
            ic2_ = 2.0f * v2 - ic2_;
            switch (type_)
            {
            case BiquadType::HIGHPASS: return v0 - k_ * v1 - v2;
            case BiquadType::BANDPASS: return v1 * k_; // unity at the centre
            case BiquadType::NOTCH: return v0 - k_ * v1;
            default: return v2;
            }
        }

    private:
        // tanh-like, 4 of headroom: the band state rounds off instead of growing without end (at an oscillator's
        // full level a resonance of 0.9 peaks +3.6 dB, quietly +14: it squashes as it is driven)
        static float soft(float x)
        {
            const float y = std::clamp(x * (1.0f / 4.0f), -3.0f, 3.0f);
            return 4.0f * y * (27.0f + y * y) / (27.0f + 9.0f * y * y);
        }
        // tan by its series for 0..0.75 rad (cutoffs up to 0.24 of the rate: 20 kHz at 2x oversampling), std::tan above
        static float fastTan(float x)
        {
            const float x2 = x * x;
            return x * (1.0f + x2 * (0.333333f + x2 * (0.133333f + x2 * 0.053968f)));
        }
        float gFor(float hz) const
        {
            const float fc = std::clamp(hz, std::min(20.0f, 0.2f * sr_), std::max(0.45f * sr_, std::min(20.0f, 0.2f * sr_))); // (a rate of 1 gave hi < lo)
            const float w = 3.14159265f * fc / sr_;
            return w < 0.75f ? fastTan(w) : std::tan(w);
        }
        void updateG()
        {
            g_ = gFor(cutoff_);
            ramp_left_ = 0;
            updateA();
        }
        void updateA()
        {
            a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
            a2_ = g_ * a1_;
            a3_ = g_ * a2_;
        }
        void update()
        {
            setResonance(res_);
            updateG();
        }

        float sr_;
        BiquadType type_;
        float cutoff_, res_;
        float g_ = 0.1f, k_ = 1.414f, a1_ = 1.0f, a2_ = 0.0f, a3_ = 0.0f;
        float ic1_ = 0.0f, ic2_ = 0.0f;
        float d1_ = 0.0f, d2_ = 0.0f, d3_ = 0.0f; // a glide's step per sample (setCutoffRamp)
        int ramp_left_ = 0;
    };
} // namespace synth
