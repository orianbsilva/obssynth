#include "gain.hpp"
#include <cstddef>

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define SYNTH_HAS_SSE2 1
#endif

namespace synth
{

    Gain::Gain(float gain)
        : gain_(gain)
    {
    }

    void Gain::set_gain(float gain)
    {
        gain_ = gain;
    }

    void Gain::reset()
    {
        // No state to reset in a simple gain effect
    }

    void Gain::process(const float *input, float *output, size_t sample_count)
    {
#ifdef SYNTH_HAS_SSE2
        // SIMD optimized version for bulk processing
        const size_t simd_samples = sample_count & ~3; // Process 4 samples at a time
        const __m128 gain_vec = _mm_set1_ps(gain_);    // Broadcast gain to all 4 elements

        for (size_t i = 0; i < simd_samples; i += 4)
        {
            __m128 input_vec = _mm_loadu_ps(&input[i]);          // Load 4 input samples
            __m128 output_vec = _mm_mul_ps(input_vec, gain_vec); // Multiply by gain
            _mm_storeu_ps(&output[i], output_vec);               // Store 4 output samples
        }

        // Handle remaining samples (scalar fallback)
        for (size_t i = simd_samples; i < sample_count; ++i)
        {
            output[i] = input[i] * gain_;
        }
#else
        // Scalar fallback for non-SSE2 platforms
        for (size_t i = 0; i < sample_count; ++i)
        {
            output[i] = input[i] * gain_;
        }
#endif
    }

} // namespace synth
