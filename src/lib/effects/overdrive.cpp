#include "overdrive.hpp"
#include <cstddef>
#include <algorithm>

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define SYNTH_HAS_SSE2 1
#endif

namespace synth
{

    Overdrive::Overdrive(float drive, float output_level, float mix)
        : drive_(drive), output_level_(output_level), mix_(mix)
    {
        // Clamp parameters to valid ranges
        set_drive(drive);
        set_output_level(output_level);
        set_mix(mix);
    }

    void Overdrive::set_drive(float drive)
    {
        // Clamp drive to reasonable range (1.0f to 20.0f)
        drive_ = std::max(1.0f, std::min(20.0f, drive));
    }

    void Overdrive::set_output_level(float output_level)
    {
        // Clamp output level to 0.0f - 1.0f range
        output_level_ = std::max(0.0f, std::min(1.0f, output_level));
    }

    void Overdrive::set_mix(float mix)
    {
        // Clamp mix to 0.0f - 1.0f range
        mix_ = std::max(0.0f, std::min(1.0f, mix));
    }

    void Overdrive::reset()
    {
        // No internal state to reset in this implementation
    }

    void Overdrive::process(const float *input, float *output, size_t sample_count)
    {
#ifdef SYNTH_HAS_SSE2
        // SIMD optimized version for bulk processing
        const size_t simd_samples = sample_count & ~3; // Process 4 samples at a time
        const __m128 drive_vec = _mm_set1_ps(drive_);
        const __m128 output_level_vec = _mm_set1_ps(output_level_);
        const __m128 mix_vec = _mm_set1_ps(mix_);
        const __m128 one_minus_mix_vec = _mm_set1_ps(1.0f - mix_);

        for (size_t i = 0; i < simd_samples; i += 4)
        {
            __m128 dry_vec = _mm_loadu_ps(&input[i]); // Load 4 input samples

            // Apply drive (input gain)
            __m128 driven_vec = _mm_mul_ps(dry_vec, drive_vec);

            // Apply soft clipping using scalar fallback for tanhf
            // Note: SSE doesn't have native tanh, so we process individually
            float driven_samples[4];
            _mm_storeu_ps(driven_samples, driven_vec);

            float wet_samples[4];
            for (int j = 0; j < 4; ++j)
            {
                wet_samples[j] = soft_clip(driven_samples[j]);
            }

            __m128 wet_vec = _mm_loadu_ps(wet_samples);

            // Apply output level compensation
            wet_vec = _mm_mul_ps(wet_vec, output_level_vec);

            // Mix dry and wet signals: (1-mix)*dry + mix*wet
            __m128 dry_part = _mm_mul_ps(one_minus_mix_vec, dry_vec);
            __m128 wet_part = _mm_mul_ps(mix_vec, wet_vec);
            __m128 output_vec = _mm_add_ps(dry_part, wet_part);

            _mm_storeu_ps(&output[i], output_vec); // Store 4 output samples
        }

        // Handle remaining samples (scalar fallback)
        for (size_t i = simd_samples; i < sample_count; ++i)
        {
            float dry_sample = input[i];
            float driven_sample = dry_sample * drive_;
            float wet_sample = soft_clip(driven_sample);
            wet_sample *= output_level_;
            output[i] = (1.0f - mix_) * dry_sample + mix_ * wet_sample;
        }
#else
        // Scalar fallback for non-SSE2 platforms
        for (size_t i = 0; i < sample_count; ++i)
        {
            // Get input sample
            float dry_sample = input[i];

            // Apply drive (input gain)
            float driven_sample = dry_sample * drive_;

            // Apply soft clipping
            float wet_sample = soft_clip(driven_sample);

            // Apply output level compensation
            wet_sample *= output_level_;

            // Mix dry and wet signals
            output[i] = (1.0f - mix_) * dry_sample + mix_ * wet_sample;
        }
#endif
    }

    float Overdrive::soft_clip(float sample) const
    {
        // Use hyperbolic tangent for smooth, musical saturation
        // tanhf provides natural soft clipping that sounds musical
        return tanhf(sample);
    }

} // namespace synth
