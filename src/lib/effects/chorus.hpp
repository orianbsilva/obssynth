/**
 * @file chorus.hpp
 * @brief Chorus effect implementation
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

#include "../math/synt_math.hpp"
#include <array>
#include <memory>

namespace synth
{
    /**
     * @brief Low Frequency Oscillator for modulation
     */
    class ChorusLFO
    {
    private:
        float sample_rate_;
        float frequency_;
        float phase_;
        float phase_increment_;

        /**
         * @brief Update the phase increment based on frequency
         */
        void updatePhaseIncrement();

    public:
        /**
         * @brief Construct a new ChorusLFO
         * @param sample_rate Sample rate in Hz
         * @param frequency Initial frequency in Hz (default 0.5f)
         */
        ChorusLFO(float sample_rate, float frequency = 0.5f);

        /**
         * @brief Set the LFO frequency
         * @param frequency Frequency in Hz
         */
        void setFrequency(float frequency);

        /**
         * @brief Get the next sine wave sample
         * @return Sine wave value (-1.0f to 1.0f)
         */
        float getSine();

        /**
         * @brief Reset the LFO phase to zero
         */
        void reset();
    };

    /**
     * @brief Variable delay line for chorus effect
     */
    class ModulatedDelayLine
    {
    private:
        std::unique_ptr<float[]> buffer_;
        int max_delay_samples_;
        int write_pos_;
        [[maybe_unused]] float sample_rate_;

    public:
        /**
         * @brief Construct a new ModulatedDelayLine
         * @param max_delay_samples Maximum delay in samples
         * @param sample_rate Sample rate in Hz
         */
        ModulatedDelayLine(int max_delay_samples, float sample_rate);

        /**
         * @brief Destructor
         */
        ~ModulatedDelayLine() = default;

        /**
         * @brief Process a sample with variable delay
         * @param input Input sample
         * @param delay_samples Delay amount in samples (with fractional support)
         * @return Delayed sample
         */
        float process(float input, float delay_samples);

        /**
         * @brief Clear the delay line buffer
         */
        void clear();
    };

    /**
     * @brief Multi-voice Chorus Effect
     *
     * The classic chorus: one or more copies of the input, each with a slight time delay,
     * those delays changed slowly (or quickly) by an LFO.
     */
    class Chorus
    {
    private:
        static constexpr int NUM_VOICES = 3;         // Number of chorus voices
        static constexpr float MAX_DELAY_MS = 30.0f; // Maximum delay in milliseconds
        static constexpr float MIN_DELAY_MS = 5.0f;  // Minimum delay in milliseconds

        float sample_rate_;
        float wet_level_; // Chorus signal level (0.0 - 1.0)
        float dry_level_; // Direct signal level (0.0 - 1.0)
        float depth_;     // Modulation depth (0.0 - 1.0)
        float rate_;      // LFO rate in Hz (0.1 - 10.0)
        float feedback_;  // Feedback amount (0.0 - 0.7)

        int max_delay_samples_;
        float min_delay_samples_;
        float max_delay_range_;

        std::array<ModulatedDelayLine, NUM_VOICES> delay_lines_;
        std::array<ChorusLFO, NUM_VOICES> lfos_;

        /**
         * @brief Convert milliseconds to samples
         * @param ms Time in milliseconds
         * @return Equivalent time in samples
         */
        float msToSamples(float ms) const;

    public:
        /**
         * @brief Construct a new Chorus effect
         * @param sample_rate Sample rate in Hz
         */
        Chorus(float sample_rate);

        /**
         * @brief Process a single sample
         * @param input Input sample
         * @return Processed sample with chorus effect
         */
        float process(float input);

        /**
         * @brief Process buffer of samples
         * @param buffer Audio buffer to process in-place
         * @param buffer_size Number of samples in buffer
         */
        void processBuffer(float *buffer, int buffer_size);

        /**
         * @brief Set the dry/wet mix ratio
         * @param mix Mix ratio (0.0f = dry, 1.0f = wet)
         */
        void setMix(float mix);

        /**
         * @brief Set the modulation depth
         * @param depth Depth amount (0.0f to 1.0f)
         */
        void setDepth(float depth);

        /**
         * @brief Set the LFO rate
         * @param rate Rate in Hz (0.1f to 10.0f)
         */
        void setRate(float rate);

        /**
         * @brief Set the feedback amount
         * @param feedback Feedback amount (0.0f to 0.7f)
         */
        void setFeedback(float feedback);

        /**
         * @brief Get the current mix ratio
         * @return Current mix ratio
         */
        float getMix() const { return wet_level_; }

        /**
         * @brief Get the current modulation depth
         * @return Current depth value
         */
        float getDepth() const { return depth_; }

        /**
         * @brief Get the current LFO rate
         * @return Current rate in Hz
         */
        float getRate() const { return rate_; }

        /**
         * @brief Get the current feedback amount
         * @return Current feedback amount
         */
        float getFeedback() const { return feedback_; }

        /**
         * @brief Clear all delay lines
         */
        void clear();
    };

} // namespace synth
