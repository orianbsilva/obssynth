/**
 * @file oscillator.hpp
 * @brief Core oscillator implementation for waveform generation
 *
 * Provides the fundamental oscillator class for generating sine, sawtooth,
 * and square waves. Supports frequency modulation for aftertouch effects
 * and maintains phase continuity for audio streaming applications.
 * Optimized for real-time audio with float-only precision.
 * Supports both mono and stereo output modes.
 */

#pragma once

#define SYNTH_USE_FLOAT_MATH
#include "../math/synt_math.hpp"
#include "pcm_waveform.hpp"
#include <functional>
#include <iostream>
#include <vector>
#include <memory>

namespace synth
{
    // Stereo sample structure
    struct StereoSample
    {
        float left;
        float right;

        StereoSample() : left(0.0f), right(0.0f) {}
        StereoSample(float l, float r) : left(l), right(r) {}
        StereoSample(float mono) : left(mono), right(mono) {}

        StereoSample operator*(float scalar) const
        {
            return StereoSample(left * scalar, right * scalar);
        }

        StereoSample operator+(const StereoSample &other) const
        {
            return StereoSample(left + other.left, right + other.right);
        }
    };

    // Wave types
    enum class WaveType
    {
        SINE,
        SAW,
        SQUARE,
        TRIANGLE,
        PULSE,
        NOISE,
        RAMP_UP,
        RAMP_DOWN,
        PCM_CUSTOM,  // Custom waveform from PCM file
        FM_SYNTHESIS // FM synthesis with multiple algorithms
    };

    // FM synthesis algorithms
    enum class FMAlgorithm
    {
        TWO_OP_SIMPLE,    // Classic 2-operator: Carrier = sin(C + I * sin(M))
        TWO_OP_FEEDBACK,  // 2-operator with feedback: includes self-modulation
        FOUR_OP_SERIAL,   // 4-operator serial: Op4->Op3->Op2->Op1->Output
        FOUR_OP_PARALLEL, // 4-operator parallel: (Op3->Op1) + (Op4->Op2)->Output
        FOUR_OP_MIXED     // 4-operator mixed: (Op4->Op3->Op1) + (Op2)->Output
    };

    // Forward declaration for wave generator function pointer
    using WaveGeneratorFunc = std::function<float(float)>;

    /**
     * @brief Core oscillator class for waveform generation
     *
     * Generates sine, sawtooth, square, triangle, pulse, noise, and ramp waveforms
     * with configurable frequency, amplitude, and wave type. Supports frequency modulation
     * for aftertouch effects. Maintains phase continuity for seamless
     * audio streaming. Optimized for real-time performance with lookup
     * tables and float-only arithmetic.
     */
    class Oscillator
    {
    private:
        float frequency_;
        float phase_;
        float sample_rate_;
        WaveType wave_type_;
        float amplitude_;
        float phase_increment_;
        WaveGeneratorFunc wave_generator_; // Function pointer set at initialization
        // the wave of wave_type_, chosen by a switch at each sample (a std::function call cost ~2x the PolyBLEP saw)
        float wave(float normalized_phase)
        {
            switch (wave_type_)
            {
            case WaveType::SINE: return sine_wave(normalized_phase);
            case WaveType::SAW: return saw_wave(normalized_phase);
            case WaveType::SQUARE: return square_wave(normalized_phase);
            case WaveType::TRIANGLE: return triangle_wave(normalized_phase);
            case WaveType::PULSE: return pulse_wave(normalized_phase);
            case WaveType::NOISE: return noise_wave(normalized_phase);
            case WaveType::RAMP_UP: return ramp_up_wave(normalized_phase);
            case WaveType::RAMP_DOWN: return ramp_down_wave(normalized_phase);
            case WaveType::PCM_CUSTOM: return pcm_custom_wave(normalized_phase);
            case WaveType::FM_SYNTHESIS: return fm_synthesis_wave(normalized_phase);
            default: return sine_wave(normalized_phase);
            }
        }
        int samples_per_buffer_;           // Number of samples per buffer

        // Frequency modulation for aftertouch
        float base_frequency_;       // Base frequency without modulation
        float frequency_mod_amount_; // Current frequency modulation amount (-1.0 to 1.0)

        // Stereo panning (-1.0 = full left, 0.0 = center, 1.0 = full right)
        float pan_position_;

        // Custom PCM waveform
        std::shared_ptr<PCMWaveform> pcm_waveform_;

        // FM Synthesis parameters
        FMAlgorithm fm_algorithm_;           // FM algorithm selection
        float fm_modulator_frequency_ratio_; // Modulator frequency as ratio to carrier (default 1.0f)
        float fm_modulation_index_;          // Modulation depth (0.0f to 20.0f)
        float fm_modulator_phase_;           // Modulator phase
        float fm_modulator_phase_increment_; // Modulator phase increment

        // Additional operators for 4-operator FM
        float fm_operator2_ratio_;           // Operator 2 frequency ratio
        float fm_operator3_ratio_;           // Operator 3 frequency ratio
        float fm_operator4_ratio_;           // Operator 4 frequency ratio
        float fm_operator2_level_;           // Operator 2 amplitude level (0.0f to 1.0f)
        float fm_operator3_level_;           // Operator 3 amplitude level (0.0f to 1.0f)
        float fm_operator4_level_;           // Operator 4 amplitude level (0.0f to 1.0f)
        float fm_operator2_phase_;           // Operator 2 phase
        float fm_operator3_phase_;           // Operator 3 phase
        float fm_operator4_phase_;           // Operator 4 phase
        float fm_operator2_phase_increment_; // Operator 2 phase increment
        float fm_operator3_phase_increment_; // Operator 3 phase increment
        float fm_operator4_phase_increment_; // Operator 4 phase increment
        float fm_feedback_amount_;           // Feedback amount for feedback algorithms (0.0f to 1.0f)
        float fm_previous_output_;           // Previous carrier output for TWO_OP_FEEDBACK (per-instance, not static!)

        // Wave generator functions
        static float sine_wave(float normalized_phase);
        float saw_wave(float normalized_phase);    // Non-static: needs phase_increment_ for PolyBLEP
        float square_wave(float normalized_phase); // Non-static: needs phase_increment_ for PolyBLEP
        static float triangle_wave(float normalized_phase);
        float pulse_wave(float normalized_phase); // Non-static: needs phase_increment_ for PolyBLEP
        static float noise_wave(float normalized_phase);
        static float ramp_up_wave(float normalized_phase);
        static float ramp_down_wave(float normalized_phase);
        float pcm_custom_wave(float normalized_phase) const; // Non-static for access to pcm_waveform_
        float fm_synthesis_wave(float normalized_phase);     // Non-static for access to FM parameters

        /// @brief PolyBLEP correction for discontinuities (band-limits saw/square/pulse)
        /// @param t Normalized phase position of discontinuity (0..1)
        /// @param dt Phase increment normalized to 1 cycle (phase_increment_ / TWO_PI)
        static float polyblep(float t, float dt);

    public:
        /**
         * @brief Construct a new Oscillator object
         * @param frequency Initial frequency in Hz
         * @param wave_type Initial waveform type (SINE, SAW, SQUARE, TRIANGLE, PULSE, NOISE, RAMP_UP, or RAMP_DOWN)
         * @param sample_rate Sample rate in Hz
         */
        Oscillator(float frequency, WaveType wave_type, float sample_rate);

        /**
         * @brief Destroy the Oscillator object
         */
        ~Oscillator() = default;

        /**
         * @brief Set the oscillator frequency
         * @param frequency Frequency in Hz (typically 20.0f to 20000.0f)
         */
        void setFrequency(float frequency);

        /**
         * @brief Set the sample rate and recalculate phase increment
         * @param sample_rate New sample rate in Hz
         */
        void setSampleRate(float sample_rate);

        /**
         * @brief Set the waveform type
         * @param wave_type Waveform type to generate (SINE, SAW, SQUARE, TRIANGLE, PULSE, NOISE, RAMP_UP, or RAMP_DOWN)
         */
        void setWaveType(WaveType wave_type);

        /**
         * @brief Set the output amplitude
         * @param amplitude Amplitude multiplier (typically 0.0f to 1.0f)
         */
        void setAmplitude(float amplitude);

        /**
         * @brief Set the stereo pan position
         * @param pan Pan position (-1.0f = full left, 0.0f = center, 1.0f = full right)
         */
        void setPan(float pan);

        /**
         * @brief Reset the oscillator phase to zero
         */
        void resetPhase();
        void advancePhase(); // the phase moves on, no sample made (a wavetable reads its own)

        /**
         * @brief Set frequency modulation amount for aftertouch effects
         * @param mod_amount Modulation amount (-1.0f to 1.0f, where 0.0f = no modulation)
         */
        void setFrequencyModulation(float mod_amount);

        /**
         * @brief Generate a single audio sample
         * @return Audio sample value
         */
        float getSample();

        /**
         * @brief Generate a single stereo audio sample
         * @return Stereo sample with left and right channels
         */
        StereoSample getStereoSample();

        /**
         * @brief Generate a buffer of audio samples (mono)
         * @param buffer Output buffer to fill with samples
         * @param size Number of samples to generate
         */
        void generateBuffer(float *buffer, int size);

        /**
         * @brief Generate a buffer of stereo audio samples (interleaved)
         * @param buffer Output buffer to fill with interleaved L,R,L,R samples
         * @param frames Number of stereo frames to generate
         */
        void generateStereoBuffer(float *buffer, int frames);

        /**
         * @brief Peek at a future sample without advancing phase
         * @param offset Number of samples ahead to peek
         * @return Audio sample value at the specified offset
         */
        float peekSampleAtOffset(int offset) const;

        /**
         * @brief Get the current frequency
         * @return Current frequency in Hz
         */
        float getFrequency() const { return frequency_; }

        /**
         * @brief Get the current phase
         * @return Current phase value
         */
        float getPhase() const { return phase_; }

        /**
         * @brief Get the sample rate
         * @return Sample rate in Hz
         */
        float getSampleRate() const { return sample_rate_; }

        /**
         * @brief Get the current waveform type
         * @return Current WaveType
         */
        WaveType getWaveType() const { return wave_type_; }

        /**
         * @brief Get the current amplitude
         * @return Current amplitude multiplier
         */
        float getAmplitude() const { return amplitude_; }

        /**
         * @brief Get the current pan position
         * @return Current pan position (-1.0f to 1.0f)
         */
        float getPan() const { return pan_position_; }

        /**
         * @brief Get the number of samples per buffer
         * @return Samples per buffer
         */
        int getSamplesPerBuffer() const { return samples_per_buffer_; }

        /**
         * @brief Set the stereo panning position
         * @param pan Panning position (-1.0f = full left, 0.0f = center, 1.0f = full right)
         */
        void set_pan(float pan);

        /**
         * @brief Set a custom waveform from PCM data
         * @param pcm_data Vector of floating-point PCM samples for one complete cycle
         */
        void set_custom_waveform(const std::vector<float> &pcm_data);

        /**
         * @brief Set a custom waveform from a PCMWaveform object
         * @param waveform Shared pointer to PCMWaveform object
         */
        void set_custom_waveform(std::shared_ptr<PCMWaveform> waveform);

        /**
         * @brief Load a custom waveform from a WAV file
         * @param filename Path to WAV file containing single-cycle waveform
         * @return True if loaded successfully, false otherwise
         */
        bool load_custom_waveform(const std::string &filename);

        /**
         * @brief Check if a custom waveform is loaded
         * @return True if custom waveform is available, false otherwise
         */
        bool has_custom_waveform() const;

        /**
         * @brief Set wav bank position for custom waveforms
         * @param position Bank position (0.0 to 1.0)
         */
        void set_bank_position(float position);

        /**
         * @brief Get current wav bank position
         * @return Current bank position (0.0 to 1.0)
         */
        float get_bank_position() const;

        /**
         * @brief Check if current custom waveform is a wav bank
         * @return True if custom waveform is a wav bank
         */
        bool is_wav_bank() const;

        // === FM Synthesis Control ===

        /**
         * @brief Set FM modulator frequency ratio
         * @param ratio Frequency ratio of modulator to carrier (0.1f to 16.0f)
         */
        void setFMModulatorRatio(float ratio);

        /**
         * @brief Set FM modulation index (depth)
         * @param index Modulation index (0.0f to 10.0f)
         */
        void setFMModulationIndex(float index);

        /**
         * @brief Get current FM modulator frequency ratio
         * @return Current modulator ratio
         */
        float getFMModulatorRatio() const;

        /**
         * @brief Get current FM modulation index
         * @return Current modulation index
         */
        float getFMModulationIndex() const;

        /**
         * @brief Reset FM modulator phase
         */
        void resetFMModulatorPhase();

        /**
         * @brief Set FM synthesis algorithm
         * @param algorithm FM algorithm to use
         */
        void setFMAlgorithm(FMAlgorithm algorithm);

        /**
         * @brief Get current FM algorithm
         * @return Current FM algorithm
         */
        FMAlgorithm getFMAlgorithm() const;

        /**
         * @brief Set FM operator ratio
         * @param operator_num Operator number (2, 3, or 4)
         * @param ratio Frequency ratio for the operator
         */
        void setFMOperatorRatio(int operator_num, float ratio);

        /**
         * @brief Set FM operator level
         * @param operator_num Operator number (2, 3, or 4)
         * @param level Amplitude level for the operator (0.0f to 1.0f)
         */
        void setFMOperatorLevel(int operator_num, float level);

        /**
         * @brief Set FM feedback amount
         * @param feedback Feedback amount (0.0f to 1.0f)
         */
        void setFMFeedback(float feedback);

        /**
         * @brief Get FM operator ratio
         * @param operator_num Operator number (2, 3, or 4)
         * @return Operator frequency ratio
         */
        float getFMOperatorRatio(int operator_num) const;

        /**
         * @brief Get FM operator level
         * @param operator_num Operator number (2, 3, or 4)
         * @return Operator amplitude level
         */
        float getFMOperatorLevel(int operator_num) const;

        /**
         * @brief Get FM feedback amount
         * @return Current feedback amount
         */
        float getFMFeedback() const;
    };

} // namespace synth
