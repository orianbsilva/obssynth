/**
 * @file mixer.hpp
 * @brief Multi-channel audio mixer
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
#include <vector>

namespace synth
{

    /**
     * @brief Multi-channel audio mixer
     *
     * Provides mixing of multiple audio channels with individual gain control.
     * Supports configurable number of input channels and efficient mixing.
     */
    class Mixer
    {
    private:
        std::vector<float> gains_;
        size_t input_count_;

    public:
        /**
         * @brief Construct a new Mixer
         * @param input_count Number of input channels (default 2)
         * @param default_gain Default gain for all channels (default 1.0f)
         */
        Mixer(size_t input_count = 2, float default_gain = 1.0f);

        /**
         * @brief Set the number of input channels
         * @param count Number of input channels
         */
        void set_input_count(size_t count);

        /**
         * @brief Set the gain for a specific channel
         * @param channel Channel index (0-based)
         * @param gain Gain value for the channel
         */
        void set_gain(size_t channel, float gain);

        /**
         * @brief Get the gain for a specific channel
         * @param channel Channel index (0-based)
         * @return Current gain value for the channel
         */
        float get_gain(size_t channel) const;

        /**
         * @brief Get the number of input channels
         * @return Current number of input channels
         */
        size_t get_input_count() const { return input_count_; }

        /**
         * @brief Reset the mixer state
         */
        void reset();

        /**
         * @brief Process multiple input channels into a single output
         * @param inputs Vector of input audio buffers
         * @param output Output audio buffer
         * @param sample_count Number of samples to process
         */
        void process(const std::vector<const float *> &inputs, float *output, size_t sample_count);
    };

} // namespace synth
