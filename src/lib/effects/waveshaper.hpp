/**
 * @file waveshaper.hpp
 * @brief Tube-style waveshaper / saturation effect
 *
 * Models vacuum tube (triode/pentode) transfer curves with asymmetric
 * soft clipping. Processes sample-by-sample in the time domain.
 *
 * Parameters:
 * - Drive: input gain before the curve (1.0 = unity, higher = more saturation)
 * - Asymmetry: controls even harmonic content (0 = symmetric, 1 = max 2nd harmonic)
 * - Chaos: physical tube randomness (thermal drift, ripple, microphonics)
 * - Model: selects which tube character curve to use
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

namespace synth
{

    /**
     * @brief Available tube emulation models
     */
    enum class TubeModel
    {
        FLAT, ///< Flat / bypass - linear passthrough, no saturation
        AX7,  ///< 12AX7 - High-gain preamp triode. Warm, strong 2nd harmonic, classic
        AT7,  ///< 12AT7 - Medium-gain preamp triode. Cleaner, balanced harmonics
        AU7,  ///< 12AU7 - Low-gain preamp triode. Subtle warmth, very clean
        EL34, ///< EL34 - Pentode power tube. Crunchy mids, British character
        P6L6  ///< 6L6 - Beam tetrode power tube. Clean headroom, American round
    };

    /**
     * @brief Stereo tube saturation waveshaper
     *
     * Applies an asymmetric soft-clipping transfer curve that emulates
     * vacuum tube nonlinearity. Adds warmth, even harmonics, and body
     * to the signal. Operates entirely in the time domain (sample by sample).
     */
    class TubeSaturation
    {
    public:
        /**
         * @brief Construct a new TubeSaturation
         * @param sample_rate Audio sample rate in Hz
         */
        explicit TubeSaturation(float sample_rate);

        /**
         * @brief Destructor
         */
        ~TubeSaturation() = default;

        /**
         * @brief Set the tube model (changes transfer curve character)
         * @param model Tube model to emulate
         */
        void setModel(TubeModel model);

        /**
         * @brief Get current tube model
         * @return Current TubeModel
         */
        TubeModel getModel() const;

        /**
         * @brief Set drive amount (input gain into the curve)
         * @param drive Drive level (1.0 = clean, 2.0 = moderate, 4.0 = heavy)
         */
        void setDrive(float drive);

        /**
         * @brief Set asymmetry amount (even harmonic emphasis)
         * @param asymmetry Asymmetry level (0.0 = symmetric, 1.0 = max tube character)
         */
        void setAsymmetry(float asymmetry);

        /**
         * @brief Set chaos amount (physical tube randomness)
         * @param chaos Chaos level (0.0 = deterministic, 0.1-0.35 = realistic, >0.5 = unstable)
         */
        void setChaos(float chaos);

        /**
         * @brief Set dry/wet mix
         * @param mix Mix amount (0.0 = fully dry, 1.0 = fully wet)
         */
        void setMix(float mix);

        /**
         * @brief Process a single sample through the tube curve
         * @param input Input sample
         * @return Shaped output sample
         */
        float process(float input);

        /**
         * @brief Process an interleaved stereo buffer in-place
         * @param buffer Interleaved stereo buffer (L,R,L,R...)
         * @param frames Number of stereo frames
         */
        void processStereo(float *buffer, int frames);

        /**
         * @brief Reset effect state
         */
        void reset();

        /**
         * @brief Set sample rate
         * @param sample_rate New sample rate in Hz
         */
        void setSampleRate(float sample_rate);

        /**
         * @brief Get current drive value
         * @return Drive level
         */
        float getDrive() const;

        /**
         * @brief Get current asymmetry value
         * @return Asymmetry level
         */
        float getAsymmetry() const;

        /**
         * @brief Get current chaos value
         * @return Chaos level
         */
        float getChaos() const;

        /**
         * @brief Get current mix value
         * @return Mix level
         */
        float getMix() const;

        /**
         * @brief Apply the tube transfer curve to a single sample
         * @param x Input sample (pre-driven, clamped to -1..1)
         * @return Shaped sample
         */
        float transferCurve(float x);

        /**
         * @brief Compute transfer curve output without mutating state (for GUI plot)
         * @param x Input sample (pre-driven, clamped to -1..1)
         * @return Shaped sample (chaos contribution uses current slow_mod_ snapshot)
         */
        float plotTransferCurve(float x) const;

    private:
        float sample_rate_;
        TubeModel model_;
        float drive_;
        float asymmetry_;
        float chaos_;
        float mix_;
        float slow_mod_; ///< Phase accumulator for physical drift modulation

        /// Function pointer to current transfer curve implementation
        float (TubeSaturation::*curve_fn_)(float);

        // Per-model transfer curve implementations
        float curveFLAT(float x);
        float curveAX7(float x);
        float curveAT7(float x);
        float curveAU7(float x);
        float curveEL34(float x);
        float curveP6L6(float x);
    };

} // namespace synth
