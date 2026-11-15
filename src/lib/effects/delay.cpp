#include "delay.hpp"
#include <algorithm>
#include <cstring>

namespace synth
{

    StereoDelay::StereoDelay(float sample_rate)
        : sample_rate_(sample_rate),
          max_delay_samples_(static_cast<int>(sample_rate * MAX_DELAY_SECONDS) + 1),
          write_pos_(0),
          delay_samples_l_(static_cast<int>(0.375f * sample_rate)),
          delay_samples_r_(static_cast<int>(0.5f * sample_rate)),
          feedback_(0.4f),
          mix_(0.3f),
          damping_(0.3f),
          ping_pong_(true),
          bypassed_(false),
          damp_state_l_(0.0f),
          damp_state_r_(0.0f)
    {
        buffer_l_ = std::make_unique<float[]>(max_delay_samples_);
        buffer_r_ = std::make_unique<float[]>(max_delay_samples_);
        std::memset(buffer_l_.get(), 0, max_delay_samples_ * sizeof(float));
        std::memset(buffer_r_.get(), 0, max_delay_samples_ * sizeof(float));
    }

    StereoDelay::~StereoDelay() = default;

    void StereoDelay::setDelayTimeL(float seconds)
    {
        seconds = std::clamp(seconds, 0.01f, MAX_DELAY_SECONDS);
        delay_samples_l_ = static_cast<int>(seconds * sample_rate_);
    }

    void StereoDelay::setDelayTimeR(float seconds)
    {
        seconds = std::clamp(seconds, 0.01f, MAX_DELAY_SECONDS);
        delay_samples_r_ = static_cast<int>(seconds * sample_rate_);
    }

    void StereoDelay::setFeedback(float feedback)
    {
        feedback_ = std::clamp(feedback, 0.0f, 0.95f);
    }

    void StereoDelay::setMix(float mix)
    {
        mix_ = std::clamp(mix, 0.0f, 1.0f);
    }

    void StereoDelay::setPingPong(bool enabled)
    {
        ping_pong_ = enabled;
    }

    void StereoDelay::setDamping(float amount)
    {
        damping_ = std::clamp(amount, 0.0f, 1.0f);
    }

    void StereoDelay::processStereo(float *buffer, int frames)
    {
        if (bypassed_)
            return; // pass-through: buffer unchanged
        for (int i = 0; i < frames; ++i)
        {
            float in_l = buffer[i * 2];
            float in_r = buffer[i * 2 + 1];

            // Read from delay buffers
            int read_pos_l = (write_pos_ - delay_samples_l_ + max_delay_samples_) % max_delay_samples_;
            int read_pos_r = (write_pos_ - delay_samples_r_ + max_delay_samples_) % max_delay_samples_;

            float delayed_l = buffer_l_[read_pos_l];
            float delayed_r = buffer_r_[read_pos_r];

            // Apply damping (simple one-pole lowpass on feedback)
            damp_state_l_ = delayed_l * (1.0f - damping_) + damp_state_l_ * damping_;
            damp_state_r_ = delayed_r * (1.0f - damping_) + damp_state_r_ * damping_;

            float fb_l, fb_r;
            if (ping_pong_)
            {
                // Cross-feedback: left feeds right, right feeds left
                fb_l = in_l + damp_state_r_ * feedback_;
                fb_r = in_r + damp_state_l_ * feedback_;
            }
            else
            {
                // Standard feedback: each channel feeds itself
                fb_l = in_l + damp_state_l_ * feedback_;
                fb_r = in_r + damp_state_r_ * feedback_;
            }

            // Write to delay buffers
            buffer_l_[write_pos_] = fb_l;
            buffer_r_[write_pos_] = fb_r;

            // Output: dry + wet
            buffer[i * 2] = in_l * (1.0f - mix_) + delayed_l * mix_;
            buffer[i * 2 + 1] = in_r * (1.0f - mix_) + delayed_r * mix_;

            // Advance write position
            write_pos_ = (write_pos_ + 1) % max_delay_samples_;
        }
    }

    void StereoDelay::reset()
    {
        std::memset(buffer_l_.get(), 0, max_delay_samples_ * sizeof(float));
        std::memset(buffer_r_.get(), 0, max_delay_samples_ * sizeof(float));
        write_pos_ = 0;
        damp_state_l_ = 0.0f;
        damp_state_r_ = 0.0f;
    }

    void StereoDelay::setSampleRate(float sample_rate)
    {
        sample_rate_ = sample_rate;
        max_delay_samples_ = static_cast<int>(sample_rate * MAX_DELAY_SECONDS) + 1;
        buffer_l_ = std::make_unique<float[]>(max_delay_samples_);
        buffer_r_ = std::make_unique<float[]>(max_delay_samples_);
        reset();
        // Recalculate delay times with new sample rate (keep proportions)
        delay_samples_l_ = std::min(delay_samples_l_, max_delay_samples_ - 1);
        delay_samples_r_ = std::min(delay_samples_r_, max_delay_samples_ - 1);
    }

} // namespace synth
