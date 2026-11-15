/**
 * @file biquad_filter.hpp
 * @brief Biquad filter implementation for synth signal processing
 *
 * Implements resonant low-pass, high-pass, band-pass, and notch filters
 * using biquad topology. Supports real-time parameter modulation.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cassert>
#include <cmath>
#include <algorithm>

namespace synth
{

    /**
     * @brief Filter type enumeration
     */
    enum class BiquadType
    {
        LOWPASS,  ///< Low-pass filter (removes high frequencies)
        HIGHPASS, ///< High-pass filter (removes low frequencies)
        BANDPASS, ///< Band-pass filter (passes frequencies around cutoff)
        NOTCH,    ///< Notch filter (removes frequencies around cutoff)
        ALLPASS   ///< All-pass filter (phase shifting)
    };

    /**
     * @brief Biquad filter with resonance control
     *
     * Implements a standard biquad filter topology supporting multiple
     * filter types with real-time parameter modulation capability.
     */
    class BiquadFilter
    {
    private:
        // Filter coefficients
        float b0_, b1_, b2_; ///< Feedforward coefficients
        float a1_, a2_;      ///< Feedback coefficients (a0 normalized to 1)

        // Filter state (delay line)
        float x1_, x2_; ///< Input delay samples
        float y1_, y2_; ///< Output delay samples

        // Parameters
        BiquadType type_;
        float cutoff_;    ///< Cutoff frequency in Hz
        float resonance_; ///< Resonance (Q factor), 0.0 to 1.0
        float sample_rate_;

        // Smoothing for parameter changes
        float target_cutoff_;
        float target_resonance_;
        static constexpr float SMOOTHING_FACTOR = 0.01f;

        // Constants
        static constexpr float PI = 3.14159265358979f;
        static constexpr float TWO_PI = 6.28318530717959f;
        static constexpr float DENORMAL_PREVENTION = 1e-15f;

        /**
         * @brief Calculate filter coefficients based on current parameters
         */
        void calculateCoefficients()
        {
            // Smooth parameter changes
            cutoff_ += (target_cutoff_ - cutoff_) * SMOOTHING_FACTOR;
            resonance_ += (target_resonance_ - resonance_) * SMOOTHING_FACTOR;

            // Clamp cutoff to valid range (20 Hz to Nyquist)
            float nyquist = sample_rate_ * 0.5f;
            float freq = std::clamp(cutoff_, 20.0f, nyquist * 0.99f);

            // Convert resonance (0-1) to Q factor (0.5 to 20)
            float Q = 0.5f + resonance_ * 19.5f;

            // Calculate intermediate values
            float omega = TWO_PI * freq / sample_rate_;
            float sin_omega = std::sin(omega);
            float cos_omega = std::cos(omega);
            float alpha = sin_omega / (2.0f * Q);

            float a0 = 1.0f + alpha;

            switch (type_)
            {
            case BiquadType::LOWPASS:
                b0_ = ((1.0f - cos_omega) / 2.0f) / a0;
                b1_ = (1.0f - cos_omega) / a0;
                b2_ = ((1.0f - cos_omega) / 2.0f) / a0;
                a1_ = (-2.0f * cos_omega) / a0;
                a2_ = (1.0f - alpha) / a0;
                break;

            case BiquadType::HIGHPASS:
                b0_ = ((1.0f + cos_omega) / 2.0f) / a0;
                b1_ = (-(1.0f + cos_omega)) / a0;
                b2_ = ((1.0f + cos_omega) / 2.0f) / a0;
                a1_ = (-2.0f * cos_omega) / a0;
                a2_ = (1.0f - alpha) / a0;
                break;

            case BiquadType::BANDPASS:
                b0_ = alpha / a0;
                b1_ = 0.0f;
                b2_ = -alpha / a0;
                a1_ = (-2.0f * cos_omega) / a0;
                a2_ = (1.0f - alpha) / a0;
                break;

            case BiquadType::NOTCH:
                b0_ = 1.0f / a0;
                b1_ = (-2.0f * cos_omega) / a0;
                b2_ = 1.0f / a0;
                a1_ = (-2.0f * cos_omega) / a0;
                a2_ = (1.0f - alpha) / a0;
                break;

            case BiquadType::ALLPASS:
                b0_ = (1.0f - alpha) / a0;
                b1_ = (-2.0f * cos_omega) / a0;
                b2_ = (1.0f + alpha) / a0;
                a1_ = (-2.0f * cos_omega) / a0;
                a2_ = (1.0f - alpha) / a0;
                break;
            }
        }

    public:
        /**
         * @brief Construct a new BiquadFilter
         * @param sample_rate Audio sample rate in Hz
         * @param type Filter type (default: LOWPASS)
         * @param cutoff Initial cutoff frequency (default: 1000 Hz)
         * @param resonance Initial resonance 0-1 (default: 0.0)
         */
        BiquadFilter(float sample_rate,
                     BiquadType type = BiquadType::LOWPASS,
                     float cutoff = 1000.0f,
                     float resonance = 0.0f)
            : b0_(1.0f), b1_(0.0f), b2_(0.0f), a1_(0.0f), a2_(0.0f), x1_(0.0f), x2_(0.0f), y1_(0.0f), y2_(0.0f), type_(type), cutoff_(cutoff), resonance_(resonance), sample_rate_(sample_rate), target_cutoff_(cutoff), target_resonance_(resonance)
        {
            assert(sample_rate > 0.0f && "BiquadFilter: sample_rate must be > 0");
            calculateCoefficients();
        }

        /**
         * @brief Reset filter state (clear delay line)
         */
        void reset()
        {
            x1_ = x2_ = y1_ = y2_ = 0.0f;
        }

        /**
         * @brief Set sample rate
         * @param sample_rate Sample rate in Hz
         */
        void setSampleRate(float sample_rate)
        {
            sample_rate_ = sample_rate;
            calculateCoefficients();
        }

        /**
         * @brief Set filter type
         * @param type New filter type
         */
        void setType(BiquadType type)
        {
            if (type_ != type)
            {
                type_ = type;
                calculateCoefficients();
            }
        }

        /**
         * @brief Set cutoff frequency (smoothed)
         * @param cutoff_hz Cutoff frequency in Hz (20 to Nyquist)
         */
        void setCutoff(float cutoff_hz)
        {
            target_cutoff_ = cutoff_hz;
        }

        /**
         * @brief Set cutoff frequency immediately (no smoothing)
         * @param cutoff_hz Cutoff frequency in Hz
         */
        void setCutoffImmediate(float cutoff_hz)
        {
            cutoff_ = target_cutoff_ = cutoff_hz;
            calculateCoefficients();
        }

        /**
         * @brief Set resonance (smoothed)
         * @param resonance Resonance value 0.0 to 1.0
         */
        void setResonance(float resonance)
        {
            target_resonance_ = std::clamp(resonance, 0.0f, 1.0f);
        }

        /**
         * @brief Set resonance immediately (no smoothing)
         * @param resonance Resonance value 0.0 to 1.0
         */
        void setResonanceImmediate(float resonance)
        {
            resonance_ = target_resonance_ = std::clamp(resonance, 0.0f, 1.0f);
            calculateCoefficients();
        }

        /**
         * @brief Get current cutoff frequency
         * @return Cutoff frequency in Hz
         */
        float getCutoff() const { return cutoff_; }

        /**
         * @brief Get current resonance
         * @return Resonance value 0.0 to 1.0
         */
        float getResonance() const { return resonance_; }

        /**
         * @brief Get filter type
         * @return Current filter type
         */
        BiquadType getType() const { return type_; }

        /**
         * @brief Process a single sample through the filter
         * @param input Input sample
         * @return Filtered output sample
         */
        float process(float input)
        {
            // Recalculate coefficients if parameters are changing
            if (std::abs(cutoff_ - target_cutoff_) > 0.1f ||
                std::abs(resonance_ - target_resonance_) > 0.001f)
            {
                calculateCoefficients();
            }

            // Biquad difference equation:
            // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
            float output = b0_ * input + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;

            // Update delay line
            x2_ = x1_;
            x1_ = input;
            y2_ = y1_;
            y1_ = output;

            // Prevent denormals
            if (std::abs(y1_) < DENORMAL_PREVENTION)
                y1_ = 0.0f;
            if (std::abs(y2_) < DENORMAL_PREVENTION)
                y2_ = 0.0f;

            return output;
        }

        /**
         * @brief Process a buffer of samples in-place
         * @param buffer Input/output buffer
         * @param num_samples Number of samples
         */
        void processBuffer(float *buffer, int num_samples)
        {
            for (int i = 0; i < num_samples; ++i)
            {
                buffer[i] = process(buffer[i]);
            }
        }

        /**
         * @brief Get filter type as string
         * @param type Filter type
         * @return String representation
         */
        static const char *typeToString(BiquadType type)
        {
            switch (type)
            {
            case BiquadType::LOWPASS:
                return "Low Pass";
            case BiquadType::HIGHPASS:
                return "High Pass";
            case BiquadType::BANDPASS:
                return "Band Pass";
            case BiquadType::NOTCH:
                return "Notch";
            case BiquadType::ALLPASS:
                return "All Pass";
            default:
                return "Unknown";
            }
        }
    };

} // namespace synth
