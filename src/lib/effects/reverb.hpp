/**
 * @file reverb.hpp
 * @brief Reverb effect implementation
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
     * @brief Simple delay line for reverb implementation
     */
    class DelayLine
    {
    private:
        std::unique_ptr<float[]> buffer_;
        int size_;
        int write_pos_;

    public:
        /**
         * @brief Construct a new DelayLine
         * @param delay_samples Delay length in samples
         */
        DelayLine(int delay_samples);

        /**
         * @brief Destructor
         */
        ~DelayLine() = default;

        /**
         * @brief Process a sample through the delay line
         * @param input Input sample
         * @return Delayed sample from the buffer
         */
        float process(float input);

        /**
         * @brief Read delayed sample without advancing position
         * @return Current delayed sample
         */
        float read() const;

        /**
         * @brief Clear the delay line buffer
         */
        void clear();
    };

    /**
     * @brief Comb filter for reverb (feedback delay line)
     */
    class CombFilter
    {
    private:
        DelayLine delay_;
        float feedback_;
        float damping_;
        float filter_state_;

    public:
        /**
         * @brief Construct a new CombFilter
         * @param delay_samples Delay length in samples
         * @param feedback Feedback amount (0.0f to 1.0f)
         * @param damping Damping amount for high frequency roll-off (default 0.5f)
         */
        CombFilter(int delay_samples, float feedback, float damping = 0.5f);

        /**
         * @brief Process a sample through the comb filter
         * @param input Input sample
         * @return Filtered sample
         */
        float process(float input);

        /**
         * @brief Set the feedback amount
         * @param feedback Feedback amount (0.0f to 1.0f)
         */
        void setFeedback(float feedback);

        /**
         * @brief Set the damping amount
         * @param damping Damping amount (0.0f to 1.0f)
         */
        void setDamping(float damping);

        /**
         * @brief Clear the filter state
         */
        void clear();
    };

    /**
     * @brief All-pass filter for reverb (creates dense reflections)
     */
    class AllPassFilter
    {
    private:
        DelayLine delay_;
        float gain_;

    public:
        /**
         * @brief Construct a new AllPassFilter
         * @param delay_samples Delay length in samples
         * @param gain Filter gain amount
         */
        AllPassFilter(int delay_samples, float gain);

        /**
         * @brief Process a sample through the all-pass filter
         * @param input Input sample
         * @return Filtered sample
         */
        float process(float input);

        /**
         * @brief Set the filter gain
         * @param gain Gain amount
         */
        void setGain(float gain);

        /**
         * @brief Clear the filter state
         */
        void clear();
    };

    /**
     * @brief Schroeder Reverb Implementation
     *
     * Based on the classic Schroeder reverb algorithm from 1961.
     * Uses parallel comb filters followed by series all-pass filters.
     */
    class SchroederReverb
    {
    public:
        /**
         * @brief Preset structure for reverb settings
         */
        struct Preset
        {
            float room_size; ///< Room size (0.0f to 1.0f)
            float damping;   ///< High frequency damping (0.0f to 1.0f)
            float wet_level; ///< Wet signal level (0.0f to 1.0f)
            float dry_level; ///< Dry signal level (0.0f to 1.0f)
        };

    private:
        static constexpr int NUM_COMBS = 4;
        static constexpr int NUM_ALLPASS = 2;

        // Comb filter delays (in samples at 44.1kHz)
        // These are tuned to avoid obvious periodic artifacts
        static constexpr std::array<int, NUM_COMBS> COMB_DELAYS = {1116, 1188, 1277, 1356};

        // All-pass filter delays
        static constexpr std::array<int, NUM_ALLPASS> ALLPASS_DELAYS = {556, 441};

        [[maybe_unused]] float sample_rate_;
        float wet_level_; // Reverb signal level (0.0 - 1.0)
        float dry_level_; // Direct signal level (0.0 - 1.0)
        float room_size_; // Controls feedback amount (0.0 - 1.0)
        float damping_;   // High frequency damping (0.0 - 1.0)

        std::array<CombFilter, NUM_COMBS> comb_filters_;
        std::array<AllPassFilter, NUM_ALLPASS> allpass_filters_;

        /**
         * @brief Scale delays based on sample rate
         * @param base_delay Base delay in samples at 44.1kHz
         * @param sample_rate Target sample rate
         * @return Scaled delay in samples
         */
        int scaleDelay(int base_delay, float sample_rate) const;

    public:
        /**
         * @brief Construct a new SchroederReverb
         * @param sample_rate Sample rate in Hz
         */
        SchroederReverb(float sample_rate);

        /**
         * @brief Process a single sample through the reverb
         * @param input Input sample
         * @return Processed sample with reverb effect
         */
        float process(float input);

        /**
         * @brief Process a buffer of samples in-place
         * @param buffer Audio buffer to process
         * @param size Number of samples in buffer
         */
        void processBuffer(float *buffer, int size);

        /**
         * @brief Process input buffer to output buffer
         * @param input Input audio buffer
         * @param output Output audio buffer
         * @param size Number of samples to process
         */
        void processBuffer(const float *input, float *output, int size);

        /**
         * @brief Set the wet signal level
         * @param wet Wet level (0.0f to 1.0f)
         */
        void setWetLevel(float wet);

        /**
         * @brief Set the dry signal level
         * @param dry Dry level (0.0f to 1.0f)
         */
        void setDryLevel(float dry);

        /**
         * @brief Set the dry/wet mix ratio
         * @param mix Mix ratio (0.0f = fully dry, 1.0f = fully wet)
         */
        void setMix(float mix);

        /**
         * @brief Set the room size
         * @param size Room size (0.0f to 1.0f)
         */
        void setRoomSize(float size);

        /**
         * @brief Set the high frequency damping
         * @param damping Damping amount (0.0f to 1.0f)
         */
        void setDamping(float damping);

        /**
         * @brief Get the current wet level
         * @return Current wet level
         */
        float getWetLevel() const { return wet_level_; }

        /**
         * @brief Get the current dry level
         * @return Current dry level
         */
        float getDryLevel() const { return dry_level_; }

        /**
         * @brief Get the current mix ratio
         * @return Current mix ratio (wet level)
         */
        float getMix() const { return wet_level_; }

        /**
         * @brief Get the current room size
         * @return Current room size
         */
        float getRoomSize() const { return room_size_; }

        /**
         * @brief Get the current damping amount
         * @return Current damping amount
         */
        float getDamping() const { return damping_; }

        /**
         * @brief Clear all internal buffers
         */
        void clear();

        /**
         * @brief Create a small room preset
         * @return Preset for small room reverb
         */
        static Preset createSmallRoomPreset();

        /**
         * @brief Create a medium room preset
         * @return Preset for medium room reverb
         */
        static Preset createMediumRoomPreset();

        /**
         * @brief Create a large room preset
         * @return Preset for large room reverb
         */
        static Preset createLargeRoomPreset();

        /**
         * @brief Create a hall preset
         * @return Preset for hall reverb
         */
        static Preset createHallPreset();

        /**
         * @brief Apply a reverb preset
         * @param preset Preset to apply
         */
        void applyPreset(const Preset &preset);
    };

} // namespace synth
