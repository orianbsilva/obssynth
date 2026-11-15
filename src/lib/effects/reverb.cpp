#include "reverb.hpp"
#include <algorithm>

namespace synth
{
    // DelayLine implementation
    DelayLine::DelayLine(int delay_samples) : size_(delay_samples), write_pos_(0)
    {
        buffer_ = std::make_unique<float[]>(size_);
        std::fill(buffer_.get(), buffer_.get() + size_, 0.0f);
    }

    float DelayLine::process(float input)
    {
        // Read the delayed sample
        float output = buffer_[write_pos_];

        // Write the new input
        buffer_[write_pos_] = input;

        // Advance write position with wrap-around
        write_pos_ = (write_pos_ + 1) % size_;

        return output;
    }

    float DelayLine::read() const
    {
        return buffer_[write_pos_];
    }

    void DelayLine::clear()
    {
        std::fill(buffer_.get(), buffer_.get() + size_, 0.0f);
    }

    // CombFilter implementation
    CombFilter::CombFilter(int delay_samples, float feedback, float damping)
        : delay_(delay_samples), feedback_(feedback), damping_(damping), filter_state_(0.0f)
    {
    }

    float CombFilter::process(float input)
    {
        float output = delay_.read();

        // Apply simple low-pass filter for damping
        filter_state_ = output * (1.0f - damping_) + filter_state_ * damping_;

        // Feed back the filtered signal
        delay_.process(input + filter_state_ * feedback_);

        return output;
    }

    void CombFilter::setFeedback(float feedback)
    {
        feedback_ = feedback;
    }

    void CombFilter::setDamping(float damping)
    {
        damping_ = damping;
    }

    void CombFilter::clear()
    {
        delay_.clear();
        filter_state_ = 0.0f;
    }

    // AllPassFilter implementation
    AllPassFilter::AllPassFilter(int delay_samples, float gain)
        : delay_(delay_samples), gain_(gain)
    {
    }

    float AllPassFilter::process(float input)
    {
        float delayed = delay_.read();
        float output = -gain_ * input + delayed;
        delay_.process(input + gain_ * delayed);
        return output;
    }

    void AllPassFilter::setGain(float gain)
    {
        gain_ = gain;
    }

    void AllPassFilter::clear()
    {
        delay_.clear();
    }

    // SchroederReverb implementation
    SchroederReverb::SchroederReverb(float sample_rate)
        : sample_rate_(sample_rate),
          wet_level_(0.3f), dry_level_(0.7f), room_size_(0.5f), damping_(0.5f),
          comb_filters_{
              CombFilter(scaleDelay(COMB_DELAYS[0], sample_rate), 0.84f, 0.5f),
              CombFilter(scaleDelay(COMB_DELAYS[1], sample_rate), 0.83f, 0.5f),
              CombFilter(scaleDelay(COMB_DELAYS[2], sample_rate), 0.82f, 0.5f),
              CombFilter(scaleDelay(COMB_DELAYS[3], sample_rate), 0.81f, 0.5f)},
          allpass_filters_{
              AllPassFilter(scaleDelay(ALLPASS_DELAYS[0], sample_rate), 0.7f),
              AllPassFilter(scaleDelay(ALLPASS_DELAYS[1], sample_rate), 0.7f)}
    {
    }

    int SchroederReverb::scaleDelay(int base_delay, float sample_rate) const
    {
        return static_cast<int>(base_delay * sample_rate / 48000.0f);
    }

    float SchroederReverb::process(float input)
    {
        // Sum the output of all parallel comb filters
        float comb_sum = 0.0f;
        for (auto &comb : comb_filters_)
        {
            comb_sum += comb.process(input);
        }

        // Pass through series all-pass filters
        float output = comb_sum;
        for (auto &allpass : allpass_filters_)
        {
            output = allpass.process(output);
        }

        // Mix dry and wet signals
        return dry_level_ * input + wet_level_ * output;
    }

    void SchroederReverb::processBuffer(float *buffer, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            buffer[i] = process(buffer[i]);
        }
    }

    void SchroederReverb::processBuffer(const float *input, float *output, int size)
    {
        for (int i = 0; i < size; ++i)
        {
            output[i] = process(input[i]);
        }
    }

    void SchroederReverb::setWetLevel(float wet)
    {
        wet_level_ = std::clamp(wet, 0.0f, 1.0f);
    }

    void SchroederReverb::setDryLevel(float dry)
    {
        dry_level_ = std::clamp(dry, 0.0f, 1.0f);
    }

    void SchroederReverb::setMix(float mix)
    {
        // Mix: 0.0 = fully dry, 1.0 = fully wet
        mix = std::clamp(mix, 0.0f, 1.0f);
        wet_level_ = mix;
        dry_level_ = 1.0f - mix;
    }

    void SchroederReverb::setRoomSize(float size)
    {
        room_size_ = std::clamp(size, 0.0f, 1.0f);

        // Adjust feedback based on room size
        float base_feedback = 0.7f + room_size_ * 0.28f; // 0.7 to 0.98
        for (int i = 0; i < NUM_COMBS; ++i)
        {
            // Slightly different feedback for each comb to avoid correlation
            float feedback = base_feedback - (i * 0.01f);
            comb_filters_[i].setFeedback(feedback);
        }
    }

    void SchroederReverb::setDamping(float damping)
    {
        damping_ = std::clamp(damping, 0.0f, 1.0f);
        for (auto &comb : comb_filters_)
        {
            comb.setDamping(damping_);
        }
    }

    void SchroederReverb::clear()
    {
        for (auto &comb : comb_filters_)
        {
            comb.clear();
        }
        for (auto &allpass : allpass_filters_)
        {
            allpass.clear();
        }
    }

    SchroederReverb::Preset SchroederReverb::createSmallRoomPreset()
    {
        return {0.3f, 0.7f, 0.2f, 0.8f};
    }

    SchroederReverb::Preset SchroederReverb::createMediumRoomPreset()
    {
        return {0.5f, 0.5f, 0.3f, 0.7f};
    }

    SchroederReverb::Preset SchroederReverb::createLargeRoomPreset()
    {
        return {0.8f, 0.3f, 0.4f, 0.6f};
    }

    SchroederReverb::Preset SchroederReverb::createHallPreset()
    {
        return {0.9f, 0.2f, 0.5f, 0.5f};
    }

    void SchroederReverb::applyPreset(const Preset &preset)
    {
        setRoomSize(preset.room_size);
        setDamping(preset.damping);
        setWetLevel(preset.wet_level);
        setDryLevel(preset.dry_level);
    }

} // namespace synth
