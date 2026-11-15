/**
 * @file gain.hpp
 * @brief Simple gain/amplifier effect
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
#include <cstddef>

namespace synth
{

    /**
     * @brief Simple gain/amplifier effect
     *
     * Provides basic amplitude control by multiplying input signals
     * by a configurable gain factor. Supports real-time gain changes.
     */
    class Gain
    {
    private:
        float gain_;

    public:
        /**
         * @brief Construct a new Gain effect
         * @param gain Initial gain value (default 1.0f = unity gain)
         */
        Gain(float gain = 1.0f);

        /**
         * @brief Set the gain level
         * @param gain Gain multiplier value
         */
        void set_gain(float gain);

        /**
         * @brief Get the current gain level
         * @return Current gain multiplier value
         */
        float get_gain() const { return gain_; }

        /**
         * @brief Reset the effect state
         */
        void reset();

        /**
         * @brief Process audio samples through the gain effect
         * @param input Input audio buffer
         * @param output Output audio buffer
         * @param sample_count Number of samples to process
         */
        void process(const float *input, float *output, size_t sample_count);
    };

} // namespace synth
