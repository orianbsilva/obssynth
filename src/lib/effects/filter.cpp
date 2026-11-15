#include "filter.hpp"
#include <cstddef>
#include <cmath>

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define SYNTH_HAS_SSE2 1
#endif

namespace synth
{

    Filter::Filter(float cutoff_frequency, float resonance, bool low_pass)
        : filter_type_(low_pass ? FilterType::LOW_PASS : FilterType::HIGH_PASS), cutoff_frequency_(cutoff_frequency), resonance_(resonance), sample_rate_(48000.0f), prev_input_(0.0f), prev_output_(0.0f)
    {
    }

    void Filter::set_sample_rate(float sample_rate)
    {
        sample_rate_ = sample_rate;
        reset();
    }

    void Filter::set_cutoff_frequency(float cutoff)
    {
        cutoff_frequency_ = cutoff;
    }

    void Filter::set_resonance(float resonance)
    {
        resonance_ = resonance;
    }

    void Filter::set_filter_type(FilterType type)
    {
        filter_type_ = type;
        reset();
    }

    void Filter::reset()
    {
        prev_input_ = 0.0f;
        prev_output_ = 0.0f;
    }

    void Filter::process(const float *input, float *output, size_t sample_count)
    {
        // Simple one-pole filter implementation
        // Calculate filter coefficient based on cutoff frequency
        float dt = 1.0f / sample_rate_;
        float rc = 1.0f / (2.0f * 3.14159f * cutoff_frequency_);
        float alpha = dt / (rc + dt);

        // Apply resonance by increasing coefficient (simplified approach)
        alpha *= resonance_;

        // Note: Filter processing has state dependencies that make SIMD optimization
        // complex due to recursive nature. For now, using scalar processing but
        // with optimized coefficient calculation.
        // TODO: Consider implementing parallel biquad sections for SIMD optimization

        if (filter_type_ == FilterType::LOW_PASS)
        {
#ifdef SYNTH_HAS_SSE2
            // Process samples with state dependency (must be sequential)
            // Note: SIMD variables prepared for future optimization
            for (size_t i = 0; i < sample_count; ++i)
            {
                prev_output_ = prev_output_ + alpha * (input[i] - prev_output_);
                output[i] = prev_output_;
            }
#else
            // Scalar version
            for (size_t i = 0; i < sample_count; ++i)
            {
                prev_output_ = prev_output_ + alpha * (input[i] - prev_output_);
                output[i] = prev_output_;
            }
#endif
        }
        else
        { // HIGH_PASS
            // High-pass filter with state dependency
            for (size_t i = 0; i < sample_count; ++i)
            {
                output[i] = resonance_ * (prev_output_ + input[i] - prev_input_);
                prev_output_ = output[i];
                prev_input_ = input[i];
            }
        }
    }

} // namespace synth
