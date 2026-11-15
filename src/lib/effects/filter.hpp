/**
 * @file filter.hpp
 * @brief Simple one-pole filter implementation
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

#include <cstddef>

#include "../math/synt_math.hpp"

namespace synth
{

    /**
     * @brief Simple one-pole filter implementation
     *
     * Provides low-pass and high-pass filtering with adjustable cutoff frequency
     * and resonance. Uses a simple one-pole design for efficient real-time processing.
     */
    class Filter
    {
    public:
        enum class FilterType
        {
            LOW_PASS,
            HIGH_PASS
        };

    private:
        FilterType filter_type_;
        float cutoff_frequency_;
        float resonance_;
        float sample_rate_;

        // Filter state variables
        float prev_input_;
        float prev_output_;

    public:
        /**
         * @brief Construct a new Filter
         * @param cutoff_frequency Initial cutoff frequency in Hz (default 1000.0f)
         * @param resonance Initial resonance value (default 0.7f)
         * @param low_pass True for low-pass, false for high-pass (default true)
         */
        Filter(float cutoff_frequency = 1000.0f, float resonance = 0.7f, bool low_pass = true);

        /**
         * @brief Set the sample rate for the filter
         * @param sample_rate Sample rate in Hz
         */
        void set_sample_rate(float sample_rate);

        /**
         * @brief Set the cutoff frequency
         * @param cutoff Cutoff frequency in Hz
         */
        void set_cutoff_frequency(float cutoff);

        /**
         * @brief Set the resonance value
         * @param resonance Resonance amount (typically 0.0f to 1.0f)
         */
        void set_resonance(float resonance);

        /**
         * @brief Set the filter type
         * @param type FilterType (LOW_PASS or HIGH_PASS)
         */
        void set_filter_type(FilterType type);

        /**
         * @brief Get the current cutoff frequency
         * @return Current cutoff frequency in Hz
         */
        float get_cutoff_frequency() const { return cutoff_frequency_; }

        /**
         * @brief Get the current resonance value
         * @return Current resonance value
         */
        float get_resonance() const { return resonance_; }

        /**
         * @brief Get the current filter type
         * @return Current FilterType
         */
        FilterType get_filter_type() const { return filter_type_; }

        /**
         * @brief Reset the filter state
         */
        void reset();

        /**
         * @brief Process audio samples through the filter
         * @param input Input audio buffer
         * @param output Output audio buffer
         * @param sample_count Number of samples to process
         */
        void process(const float *input, float *output, size_t sample_count);
    };

} // namespace synth
