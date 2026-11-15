/**
 * @file tube_reverb.cpp
 * @brief Physical model tube reverb - implementation
 */

#include "tube_reverb.hpp"
#include <algorithm>
#include <cmath>

namespace synth
{
    TubeReverb::TubeReverb(float sample_rate)
        : sample_rate_(sample_rate), tube_length_(1.2f), reflection_(0.92f), mix_(0.5f), write_pos_(0)
    {
        // Allocate once for the maximum possible tube length
        max_delay_samples_ = static_cast<int>(MAX_TUBE_LENGTH * sample_rate_ / SPEED_OF_SOUND) + 2;
        buffer_ = std::make_unique<float[]>(max_delay_samples_);
        rebuildDelay();
    }

    // ── P1 ────────────────────────────────────────────────────────────────────

    void TubeReverb::setTubeLength(float length)
    {
        tube_length_ = std::max(MIN_TUBE_LENGTH, std::min(MAX_TUBE_LENGTH, length));
        rebuildDelay();
    }

    float TubeReverb::getTubeLength() const
    {
        return tube_length_;
    }

    // ── P2 ────────────────────────────────────────────────────────────────────

    void TubeReverb::setReflection(float reflection)
    {
        reflection_ = std::max(0.5f, std::min(0.99f, reflection));
    }

    float TubeReverb::getReflection() const
    {
        return reflection_;
    }

    // ── P3 ────────────────────────────────────────────────────────────────────

    void TubeReverb::setMix(float mix)
    {
        mix_ = std::max(0.0f, std::min(1.0f, mix));
    }

    float TubeReverb::getMix() const
    {
        return mix_;
    }

    // ── Processing ────────────────────────────────────────────────────────────

    float TubeReverb::process(float input)
    {
        // Oldest sample in the buffer = pressure wave arriving at the far-end (mic)
        float delayed = buffer_[write_pos_];

        // Open-end reflection: pressure node → phase inversion + attenuation
        float reflected = -delayed * reflection_ * DAMPING_PER_SAMPLE;

        // Speaker end: injected signal + returning reflected wave
        float current = input + reflected;

        // Propagate through the tube (push current, overwrite oldest slot)
        buffer_[write_pos_] = current;
        write_pos_ = (write_pos_ + 1) % delay_samples_;

        // Mic reads the far-end pressure; blend with dry signal
        return (1.0f - mix_) * input + mix_ * delayed;
    }

    void TubeReverb::processBuffer(float *buffer, int size)
    {
        for (int i = 0; i < size; ++i)
            buffer[i] = process(buffer[i]);
    }

    void TubeReverb::processBuffer(const float *input, float *output, int size)
    {
        for (int i = 0; i < size; ++i)
            output[i] = process(input[i]);
    }

    void TubeReverb::clear()
    {
        std::fill(buffer_.get(), buffer_.get() + max_delay_samples_, 0.0f);
        write_pos_ = 0;
    }

    float TubeReverb::fundamentalHz() const
    {
        return SPEED_OF_SOUND / (2.0f * tube_length_);
    }

    // ── Private ───────────────────────────────────────────────────────────────

    void TubeReverb::rebuildDelay()
    {
        delay_samples_ = static_cast<int>(tube_length_ * sample_rate_ / SPEED_OF_SOUND);
        // Guard: must be at least 1 and fit within allocated buffer
        if (delay_samples_ < 1)
            delay_samples_ = 1;
        if (delay_samples_ > max_delay_samples_)
            delay_samples_ = max_delay_samples_;
        // Clear stale data to prevent transients after a length change
        std::fill(buffer_.get(), buffer_.get() + max_delay_samples_, 0.0f);
        write_pos_ = 0;
    }

} // namespace synth
