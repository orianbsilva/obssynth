#include "chorus.hpp"
#include "../math/synt_math.hpp"
#include <algorithm>

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define SYNTH_HAS_SSE2 1
#endif

namespace synth
{
    // ChorusLFO implementation
    ChorusLFO::ChorusLFO(float sample_rate, float frequency)
        : sample_rate_(sample_rate), frequency_(frequency), phase_(0.0f)
    {
        updatePhaseIncrement();
    }

    void ChorusLFO::setFrequency(float frequency)
    {
        frequency_ = frequency;
        updatePhaseIncrement();
    }

    float ChorusLFO::getSine()
    {
        float output = math::sin(phase_);
        phase_ += phase_increment_;

        // Wrap phase to avoid accumulation errors
        phase_ = math::fmod(phase_, math::TWO_PI);

        return output;
    }

    void ChorusLFO::reset()
    {
        phase_ = 0.0f;
    }

    void ChorusLFO::updatePhaseIncrement()
    {
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;
    }

    // ModulatedDelayLine implementation
    ModulatedDelayLine::ModulatedDelayLine(int max_delay_samples, float sample_rate)
        : max_delay_samples_(max_delay_samples), write_pos_(0), sample_rate_(sample_rate)
    {
        buffer_ = std::make_unique<float[]>(max_delay_samples_);
        std::fill(buffer_.get(), buffer_.get() + max_delay_samples_, 0.0f);
    }

    float ModulatedDelayLine::process(float input, float delay_samples)
    {
        // Clamp delay to valid range
        delay_samples = std::max(1.0f, std::min(delay_samples, static_cast<float>(max_delay_samples_ - 1)));

        // Write input to buffer
        buffer_[write_pos_] = input;

        // Calculate read position with fractional delay
        float read_pos_float = write_pos_ - delay_samples;
        if (read_pos_float < 0.0f)
            read_pos_float += max_delay_samples_;

        // Linear interpolation for smooth modulation
        int read_pos_int = static_cast<int>(read_pos_float);
        float fraction = read_pos_float - read_pos_int;

        int next_pos = (read_pos_int + 1) % max_delay_samples_;

        float sample1 = buffer_[read_pos_int];
        float sample2 = buffer_[next_pos];
        float output = sample1 + fraction * (sample2 - sample1);

        // Advance write position
        write_pos_ = (write_pos_ + 1) % max_delay_samples_;

        return output;
    }

    void ModulatedDelayLine::clear()
    {
        std::fill(buffer_.get(), buffer_.get() + max_delay_samples_, 0.0f);
        write_pos_ = 0;
    }

    // Chorus implementation
    Chorus::Chorus(float sample_rate)
        : sample_rate_(sample_rate),
          wet_level_(0.5f), dry_level_(0.5f), depth_(0.5f), rate_(0.5f), feedback_(0.2f),
          delay_lines_{
              ModulatedDelayLine(static_cast<int>(msToSamples(MAX_DELAY_MS)), sample_rate),
              ModulatedDelayLine(static_cast<int>(msToSamples(MAX_DELAY_MS)), sample_rate),
              ModulatedDelayLine(static_cast<int>(msToSamples(MAX_DELAY_MS)), sample_rate)},
          lfos_{
              ChorusLFO(sample_rate, rate_),
              ChorusLFO(sample_rate, rate_ * 1.1f), // Slightly different rates
              ChorusLFO(sample_rate, rate_ * 0.9f)  // for richer modulation
          }
    {
        max_delay_samples_ = static_cast<int>(msToSamples(MAX_DELAY_MS));
        min_delay_samples_ = msToSamples(MIN_DELAY_MS);
        max_delay_range_ = msToSamples(MAX_DELAY_MS - MIN_DELAY_MS);

        // Initialize LFOs with different phase offsets for stereo width
        lfos_[1].reset(); // Start at different phases
        lfos_[2].reset();
    }

    float Chorus::msToSamples(float ms) const
    {
        return ms * sample_rate_ / 1000.0f;
    }

    float Chorus::process(float input)
    {
        float wet_sum = 0.0f;

        // Process each chorus voice
        for (int i = 0; i < NUM_VOICES; ++i)
        {
            // Get LFO modulation (-1 to +1)
            float lfo_value = lfos_[i].getSine();

            // Calculate modulated delay time
            float delay_modulation = depth_ * lfo_value;
            float delay_samples = min_delay_samples_ +
                                  (max_delay_range_ * 0.5f) +
                                  (delay_modulation * max_delay_range_ * 0.5f);

            // Process through delay line
            float delayed = delay_lines_[i].process(input + wet_sum * feedback_, delay_samples);
            wet_sum += delayed;
        }

        // Mix dry and wet signals
        wet_sum /= NUM_VOICES; // Normalize
        return input * dry_level_ + wet_sum * wet_level_;
    }

    void Chorus::processBuffer(float *buffer, int buffer_size)
    {
#ifdef SYNTH_HAS_SSE2
        // Note: Chorus processing has complex state dependencies with delay lines and LFOs
        // that make SIMD optimization difficult. The modulated delay lines need to be
        // processed sequentially due to buffer write/read dependencies.
        // Using scalar processing for now.
        for (int i = 0; i < buffer_size; ++i)
        {
            buffer[i] = process(buffer[i]);
        }
#else
        // Scalar version
        for (int i = 0; i < buffer_size; ++i)
        {
            buffer[i] = process(buffer[i]);
        }
#endif
    }

    void Chorus::setMix(float mix)
    {
        mix = std::max(0.0f, std::min(1.0f, mix));
        wet_level_ = mix;
        dry_level_ = 1.0f - mix;
    }

    void Chorus::setDepth(float depth)
    {
        depth_ = std::max(0.0f, std::min(1.0f, depth));
    }

    void Chorus::setRate(float rate)
    {
        rate_ = std::max(0.1f, std::min(10.0f, rate));
        // Update all LFO rates with slight variations
        lfos_[0].setFrequency(rate_);
        lfos_[1].setFrequency(rate_ * 1.1f);
        lfos_[2].setFrequency(rate_ * 0.9f);
    }

    void Chorus::setFeedback(float feedback)
    {
        feedback_ = std::max(0.0f, std::min(0.7f, feedback));
    }

    void Chorus::clear()
    {
        for (auto &delay_line : delay_lines_)
        {
            delay_line.clear();
        }
    }

} // namespace synth
