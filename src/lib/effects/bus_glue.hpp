#pragma once
// A bus compressor for "glue": the mix's peaks pull the whole mix down a little, both sides together, so the hits
// sit as one kit. A peak follower (10 ms attack, 150 ms release) above -18 dBFS, a ratio from 1:1 to 4:1 by the
// amount, half the reduction made back up. FLOAT-ONLY.
#include <algorithm>
#include <cmath>

namespace synth
{
    class BusGlue
    {
    public:
        explicit BusGlue(float sampleRate)
            : attack_(1.0f - std::exp(-1.0f / (0.010f * sampleRate))), release_(1.0f - std::exp(-1.0f / (0.150f * sampleRate)))
        {
        }
        void setAmount(float a) { amount_ = std::clamp(a, 0.0f, 1.0f); }

        // interleaved stereo, in place
        void process(float *lr, int frames)
        {
            if (amount_ <= 0.001f)
                return;
            const float ratio = 1.0f + 3.0f * amount_, threshold = 0.126f; // -18 dBFS
            for (int i = 0; i < frames; ++i)
            {
                const float peak = std::max(std::fabs(lr[i * 2]), std::fabs(lr[i * 2 + 1]));
                env_ += (peak - env_) * (peak > env_ ? attack_ : release_);
                float gain = 1.0f;
                if (env_ > threshold)
                {
                    const float over = env_ / threshold;                           // above the threshold, as a ratio
                    gain = std::pow(over, 1.0f / ratio - 1.0f);                  // what is above, divided by the ratio
                    gain *= std::pow(1.0f / gain, 0.5f);                         // half of it made back up
                }
                lr[i * 2] *= gain;
                lr[i * 2 + 1] *= gain;
            }
        }

    private:
        float attack_, release_, amount_ = 0.0f, env_ = 0.0f;
    };
} // namespace synth
