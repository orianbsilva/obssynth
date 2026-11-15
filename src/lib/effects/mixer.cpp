#include "mixer.hpp"
#include <cstddef>
#include <algorithm>

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define SYNTH_HAS_SSE2 1
#endif

namespace synth
{

    Mixer::Mixer(size_t input_count, float default_gain)
        : input_count_(input_count)
    {
        gains_.resize(input_count, default_gain);
    }

    void Mixer::set_input_count(size_t count)
    {
        input_count_ = count;
        gains_.resize(count, 1.0f);
    }

    void Mixer::set_gain(size_t channel, float gain)
    {
        if (channel < gains_.size())
        {
            gains_[channel] = gain;
        }
    }

    float Mixer::get_gain(size_t channel) const
    {
        if (channel < gains_.size())
        {
            return gains_[channel];
        }
        return 1.0f;
    }

    void Mixer::reset()
    {
        // No state to reset in a simple mixer
    }

    void Mixer::process(const std::vector<const float *> &inputs, float *output, size_t sample_count)
    {
#ifdef SYNTH_HAS_SSE2
        // SIMD optimized version for bulk processing
        const size_t simd_samples = sample_count & ~3; // Process 4 samples at a time

        // Clear output buffer first using SIMD
        const __m128 zero_vec = _mm_setzero_ps();
        for (size_t i = 0; i < simd_samples; i += 4)
        {
            _mm_storeu_ps(&output[i], zero_vec);
        }
        // Clear remaining samples
        for (size_t i = simd_samples; i < sample_count; ++i)
        {
            output[i] = 0.0f;
        }

        // Mix all inputs with their respective gains using SIMD
        for (size_t i = 0; i < inputs.size() && i < gains_.size(); ++i)
        {
            if (inputs[i])
            {
                const __m128 gain_vec = _mm_set1_ps(gains_[i]);

                // Process 4 samples at a time
                for (size_t j = 0; j < simd_samples; j += 4)
                {
                    __m128 input_vec = _mm_loadu_ps(&inputs[i][j]);
                    __m128 output_vec = _mm_loadu_ps(&output[j]);
                    __m128 scaled_input = _mm_mul_ps(input_vec, gain_vec);
                    __m128 mixed_output = _mm_add_ps(output_vec, scaled_input);
                    _mm_storeu_ps(&output[j], mixed_output);
                }

                // Handle remaining samples (scalar fallback)
                const float gain = gains_[i];
                for (size_t j = simd_samples; j < sample_count; ++j)
                {
                    output[j] += inputs[i][j] * gain;
                }
            }
        }
#else
        // Scalar fallback for non-SSE2 platforms
        // Clear output buffer first
        std::fill(output, output + sample_count, 0.0f);

        // Mix all inputs with their respective gains
        for (size_t i = 0; i < inputs.size() && i < gains_.size(); ++i)
        {
            if (inputs[i])
            {
                const float gain = gains_[i];
                for (size_t j = 0; j < sample_count; ++j)
                {
                    output[j] += inputs[i][j] * gain;
                }
            }
        }
#endif
    }

} // namespace synth
