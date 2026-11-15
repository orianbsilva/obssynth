/**
 * @file delay.hpp
 * @brief Stereo ping-pong delay / echo effect
 *
 * EFFECTS MODULE CODING STANDARD:
 * - Headers (.hpp) contain ONLY declarations - no implementations
 * - All implementations must be in corresponding .cpp files
 * - ALL public methods must have Doxygen documentation (@brief, @param, @return)
 * - Use float-only precision for all audio processing
 * - Private members end with underscore (_)
 * - All code in lowercase 'synth' namespace
 * - Follow pattern: declaration in .hpp, implementation in .cpp
 */

#pragma once

#include <memory>

namespace synth
{

    /**
     * @brief Stereo delay effect with feedback and ping-pong mode
     *
     * Processes interleaved stereo buffers (L,R,L,R...).
     * Supports simple echo and ping-pong (cross-feedback) modes.
     */
    class StereoDelay
    {
    public:
        /**
         * @brief Construct a new StereoDelay
         * @param sample_rate Audio sample rate in Hz
         */
        explicit StereoDelay(float sample_rate);

        /**
         * @brief Destructor
         */
        ~StereoDelay();

        /**
         * @brief Set delay time for left channel
         * @param seconds Delay time in seconds (0.01 to 2.0)
         */
        void setDelayTimeL(float seconds);

        /**
         * @brief Set delay time for right channel
         * @param seconds Delay time in seconds (0.01 to 2.0)
         */
        void setDelayTimeR(float seconds);

        /**
         * @brief Set feedback amount
         * @param feedback Feedback level (0.0 to 0.95)
         */
        void setFeedback(float feedback);

        /**
         * @brief Set dry/wet mix
         * @param mix Mix amount (0.0 = dry, 1.0 = wet)
         */
        void setMix(float mix);

        /**
         * @brief Enable or disable ping-pong mode
         * @param enabled true for ping-pong cross-feedback
         */
        void setPingPong(bool enabled);

        /**
         * @brief Set high-frequency damping on feedback path
         * @param amount Damping amount (0.0 = none, 1.0 = heavy)
         */
        void setDamping(float amount);

        float getMix() const { return mix_; }
        float getFeedback() const { return feedback_; }
        float getDamping() const { return damping_; }
        float getDelayTimeLSec() const { return (float)delay_samples_l_ / sample_rate_; }
        float getDelayTimeRSec() const { return (float)delay_samples_r_ / sample_rate_; }
        bool getPingPong() const { return ping_pong_; }
        void setBypassed(bool b) { bypassed_ = b; }
        bool isBypassed() const { return bypassed_; }

        /**
         * @brief Process an interleaved stereo buffer in-place
         * @param buffer Interleaved stereo buffer (L,R,L,R...)
         * @param frames Number of stereo frames
         */
        void processStereo(float *buffer, int frames);

        /**
         * @brief Reset delay buffers (clear all echoes)
         */
        void reset();

        /**
         * @brief Set sample rate
         * @param sample_rate New sample rate in Hz
         */
        void setSampleRate(float sample_rate);

    private:
        static constexpr float MAX_DELAY_SECONDS = 2.0f;

        float sample_rate_;
        int max_delay_samples_;

        // Delay buffers
        std::unique_ptr<float[]> buffer_l_;
        std::unique_ptr<float[]> buffer_r_;
        int write_pos_;

        // Parameters
        int delay_samples_l_;
        int delay_samples_r_;
        float feedback_;
        float mix_;
        float damping_;
        bool ping_pong_;
        bool bypassed_;

        // Damping filter state
        float damp_state_l_;
        float damp_state_r_;
    };

} // namespace synth
