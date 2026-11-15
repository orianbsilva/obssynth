/**
 * @file overdrive.hpp
 * @brief Overdrive distortion effect
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
     * @brief Overdrive distortion effect
     *
     * Provides soft clipping distortion with adjustable drive and output level.
     * Uses a hyperbolic tangent function for smooth, musical saturation.
     * Includes input gain (drive) and output gain controls for flexible tone shaping.
     */
    class Overdrive
    {
    private:
        float drive_;        // Input gain/drive amount (1.0f = no drive, higher = more distortion)
        float output_level_; // Output level compensation (0.0f to 1.0f)
        float mix_;          // Dry/wet mix (0.0f = dry, 1.0f = wet)

    public:
        /**
         * @brief Construct a new Overdrive effect
         * @param drive Input drive amount (1.0f to 10.0f recommended)
         * @param output_level Output level compensation (0.0f to 1.0f)
         * @param mix Dry/wet mix ratio (0.0f to 1.0f)
         */
        Overdrive(float drive = 2.0f, float output_level = 0.5f, float mix = 1.0f);

        /**
         * @brief Set the drive amount
         * @param drive Drive level (1.0f = clean, higher = more distortion)
         */
        void set_drive(float drive);

        /**
         * @brief Set the output level
         * @param output_level Output compensation level (0.0f to 1.0f)
         */
        void set_output_level(float output_level);

        /**
         * @brief Set the dry/wet mix
         * @param mix Mix ratio (0.0f = dry signal only, 1.0f = wet signal only)
         */
        void set_mix(float mix);

        /**
         * @brief Get the current drive amount
         * @return Current drive level
         */
        float get_drive() const { return drive_; }

        /**
         * @brief Get the current output level
         * @return Current output level compensation
         */
        float get_output_level() const { return output_level_; }

        /**
         * @brief Get the current dry/wet mix ratio
         * @return Current mix ratio (0.0f = dry, 1.0f = wet)
         */
        float get_mix() const { return mix_; }

        /**
         * @brief Reset the effect state
         */
        void reset();

        /**
         * @brief Process audio samples through the overdrive effect
         * @param input Input audio buffer
         * @param output Output audio buffer
         * @param sample_count Number of samples to process
         */
        void process(const float *input, float *output, size_t sample_count);

    private:
        /**
         * @brief Apply soft clipping using hyperbolic tangent
         * @param sample Input sample
         * @return Clipped sample
         */
        float soft_clip(float sample) const;
    };

} // namespace synth
