#pragma once
// A product's last stage, at the native rate: the DC out (a saturator's asymmetry leaves some), the volume and a
// balance ramped across the block from the last one's (no stairs), and a limiter that holds the peaks under -1
// dBFS (instant down, 150 ms back up, one gain for both sides so the image stays), then a soft edge above 0.95
// for what gets through. Out of ObsSynth4D, for every product. FLOAT-ONLY.
#include <algorithm>
#include <cmath>

namespace synth
{
    class OutputStage
    {
    public:
        explicit OutputStage(float sampleRate, float volume = 0.8f) : sr_(sampleRate), lastVolume_(volume) {}

        // buffer: interleaved stereo; volume 0..1; pan -1..1
        void process(float *buffer, int frames, float volume, float pan)
        {
            const float vol0 = lastVolume_, pan0 = lastPan_;
            lastVolume_ = volume;
            lastPan_ = pan;
            const float release = 1.0f - std::exp(-1.0f / (0.15f * sr_));
            for (int i = 0; i < frames; ++i)
            {
                float lr[2];
                const float f = static_cast<float>(i + 1) / static_cast<float>(frames);
                const float v = vol0 + (volume - vol0) * f, p = pan0 + (pan - pan0) * f;
                for (int c = 0; c < 2; ++c)
                {
                    const float x = buffer[i * 2 + c];
                    const float y = x - dcIn_[c] + 0.9995f * dcOut_[c]; // ~4 Hz high-pass
                    dcIn_[c] = x;
                    dcOut_[c] = y;
                    lr[c] = y * v * (c == 0 ? std::min(1.0f, 1.0f - p) : std::min(1.0f, 1.0f + p));
                }
                const float peak = std::max(std::fabs(lr[0]), std::fabs(lr[1]));
                const float want = peak > kCeiling ? kCeiling / peak : 1.0f;
                limGain_ = want < limGain_ ? want : limGain_ + (want - limGain_) * release;
                limLowest_ = std::min(limLowest_, limGain_);
                for (int c = 0; c < 2; ++c)
                {
                    float y = lr[c] * limGain_;
                    if (std::fabs(y) > 0.95f) // the soft edge: 0.95 to 1.0
                        y = std::copysign(0.95f + 0.05f * std::tanh((std::fabs(y) - 0.95f) / 0.05f), y);
                    buffer[i * 2 + c] = y;
                }
            }
        }

        // the limiter's deepest gain since the last call (1: it did nothing)
        float takeLimiterLowest()
        {
            const float g = limLowest_;
            limLowest_ = 1.0f;
            return g;
        }

    private:
        static constexpr float kCeiling = 0.891f; // -1 dBFS
        float sr_;
        float lastVolume_, lastPan_ = 0.0f;
        float limGain_ = 1.0f, limLowest_ = 1.0f;
        float dcIn_[2]{}, dcOut_[2]{};
    };
} // namespace synth
