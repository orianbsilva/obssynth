/**
 * @file tube_reverb.hpp
 * @brief Physical model tube reverb based on 1D digital waveguide
 *
 * Simulates acoustic resonance of a cylindrical tube open at both ends
 * (open-open boundary conditions). The speaker drives one end; the
 * microphone reads the other end.
 *
 * Resonant modes:
 *   f_n = n * v / (2L),   n = 1, 2, 3 ...
 * where v = 343 m/s (speed of sound) and L = tube length in metres.
 *
 * Parameters:
 * - P1 (tube_length): physical length in metres [0.3 – 6.0]
 *     → f0 range: ~29 Hz (6 m) to ~572 Hz (0.3 m)
 * - P2 (reflection): reflection coefficient at each open end [0.5 – 0.99]
 *     → controls resonance decay time (higher = longer ring)
 * - P3 (mix): dry/wet blend [0.0 = dry only, 1.0 = wet only]
 *
 * EFFECTS MODULE CODING STANDARD:
 * - Headers (.hpp) contain ONLY declarations – no implementations
 * - All implementations must be in the corresponding .cpp file
 * - ALL public methods must have Doxygen documentation
 * - float-only precision for all audio processing
 * - Private members end with underscore (_)
 * - All code in lowercase 'synth' namespace
 */

#pragma once

#include <memory>

namespace synth
{

    /**
     * @brief Physical model tube reverb using a 1D digital waveguide.
     *
     * Models a cylindrical air column (open at both ends) as a resonant
     * reverb body. Phase-inverting reflections at the open ends produce
     * harmonic resonance modes spaced at f0 = v / (2L).
     *
     * The three exposed parameters map directly to physical properties:
     *   P1 → tube length   (sets resonant pitch)
     *   P2 → reflection    (sets decay time / brightness)
     *   P3 → mix           (dry/wet blend)
     */
    class TubeReverb
    {
    public:
        /**
         * @brief Construct a new TubeReverb
         * @param sample_rate Audio sample rate in Hz
         */
        explicit TubeReverb(float sample_rate);

        /**
         * @brief Destructor
         */
        ~TubeReverb() = default;

        /// Move semantics (needed because of unique_ptr member)
        TubeReverb(TubeReverb &&) = default;
        TubeReverb &operator=(TubeReverb &&) = default;

        // ── P1 ────────────────────────────────────────────────────────────

        /**
         * @brief Set the tube length (P1)
         *
         * Changes the fundamental resonant frequency: f0 = 343 / (2 * length).
         * Clears the internal delay buffer to avoid transients on change.
         *
         * @param length Tube length in metres [0.3 – 6.0]
         */
        void setTubeLength(float length);

        /**
         * @brief Get the current tube length
         * @return Tube length in metres
         */
        float getTubeLength() const;

        // ── P2 ────────────────────────────────────────────────────────────

        /**
         * @brief Set the reflection coefficient (P2)
         *
         * Controls how much energy is retained at each open-end reflection.
         * Higher values produce longer decay / stronger resonance.
         *
         * @param reflection Reflection coefficient [0.5 – 0.99]
         */
        void setReflection(float reflection);

        /**
         * @brief Get the current reflection coefficient
         * @return Reflection coefficient
         */
        float getReflection() const;

        // ── P3 ────────────────────────────────────────────────────────────

        /**
         * @brief Set the dry/wet mix (P3)
         * @param mix 0.0 = fully dry, 1.0 = fully wet
         */
        void setMix(float mix);

        /**
         * @brief Get the current dry/wet mix
         * @return Mix value [0.0 – 1.0]
         */
        float getMix() const;

        // ── Processing ────────────────────────────────────────────────────

        /**
         * @brief Process a single sample through the tube reverb
         * @param input Dry input sample
         * @return Processed sample (dry + wet according to mix)
         */
        float process(float input);

        /**
         * @brief Process a buffer of samples in-place
         * @param buffer Audio buffer to process
         * @param size   Number of samples in buffer
         */
        void processBuffer(float *buffer, int size);

        /**
         * @brief Process input buffer to output buffer
         * @param input  Read-only input buffer
         * @param output Output buffer (may alias input)
         * @param size   Number of samples to process
         */
        void processBuffer(const float *input, float *output, int size);

        /**
         * @brief Clear the internal delay buffer and reset state
         */
        void clear();

        /**
         * @brief Compute the fundamental resonant frequency for the current length
         * @return f0 in Hz
         */
        float fundamentalHz() const;

    private:
        static constexpr float SPEED_OF_SOUND = 343.0f;     ///< m/s
        static constexpr float DAMPING_PER_SAMPLE = 0.995f; ///< wall/air absorption per sample
        static constexpr float MAX_TUBE_LENGTH = 6.0f;      ///< metres (sets allocation)
        static constexpr float MIN_TUBE_LENGTH = 0.3f;      ///< metres (clamp lower bound)

        float sample_rate_;
        float tube_length_;
        float reflection_;
        float mix_;

        int max_delay_samples_; ///< buffer capacity (for MAX_TUBE_LENGTH at sample_rate)
        int delay_samples_;     ///< active delay length for current tube_length
        int write_pos_;         ///< circular buffer write position

        std::unique_ptr<float[]> buffer_; ///< circular delay buffer

        /**
         * @brief Recompute delay_samples_ from tube_length_ and clear the buffer
         */
        void rebuildDelay();
    };

} // namespace synth
