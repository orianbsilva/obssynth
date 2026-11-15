/**
 * @file adsr_envelope.hpp
 * @brief ADSR envelope implementation for amplitude shaping
 *
 * Provides Attack, Decay, Sustain, Release envelope functionality
 * for shaping audio amplitude over time. Thread-safe implementation
 * suitable for real-time audio processing with precise timing control
 * and state management.
 */

#pragma once

#include <mutex>

namespace synth
{
    // ADSR envelope states
    enum class EnvelopeState
    {
        IDLE,    // Envelope is not active
        ATTACK,  // Attack phase - rising from 0 to peak
        DECAY,   // Decay phase - falling from peak to sustain level
        SUSTAIN, // Sustain phase - holding at sustain level
        RELEASE  // Release phase - falling from current level to 0
    };

    /**
     * @brief ADSR envelope for shaping amplitude over time
     *
     * Implements a classic ADSR (Attack, Decay, Sustain, Release) envelope
     * generator for controlling amplitude changes in response to note events.
     * Features thread-safe operation, precise timing control, and efficient
     * sample-by-sample processing for real-time audio applications.
     * Supports peeking functionality for UI visualization without state changes.
     */
    class ADSREnvelope
    {
    private:
        // ADSR parameters (in seconds)
        float attack_time_;   // Time to reach peak amplitude
        float decay_time_;    // Time to decay from peak to sustain level
        float sustain_level_; // Sustain amplitude level (0.0-1.0)
        float release_time_;  // Time to decay from sustain to 0

        // Current state
        EnvelopeState state_;
        float current_amplitude_;
        float sample_rate_;

        // Internal calculation variables
        float attack_increment_;
        float decay_increment_;
        float release_increment_;
        float target_amplitude_; // Target amplitude for current phase

        // Timing variables
        int samples_in_current_phase_;
        int total_samples_in_phase_;

        // (a mutex was here: only the audio thread runs an envelope, and getNextSample never took it)

        // Internal helper methods
        void calculateIncrements();
        void updatePhase();

    public:
        /**
         * @brief Construct a new ADSREnvelope object
         * @param sample_rate Sample rate in Hz for timing calculations
         */
        ADSREnvelope(float sample_rate);

        /**
         * @brief Destroy the ADSREnvelope object
         */
        ~ADSREnvelope() = default;

        /**
         * @brief Set the attack time
         * @param attack_seconds Time in seconds to reach peak amplitude from zero
         */
        void setAttackTime(float attack_seconds);

        /**
         * @brief Set the decay time
         * @param decay_seconds Time in seconds to decay from peak to sustain level
         */
        void setDecayTime(float decay_seconds);

        /**
         * @brief Set the sustain level
         * @param sustain_level Sustain amplitude level (0.0f to 1.0f)
         */
        void setSustainLevel(float sustain_level);

        /**
         * @brief Set the release time
         * @param release_seconds Time in seconds to decay from current level to zero
         */
        void setReleaseTime(float release_seconds);

        /**
         * @brief Set all ADSR parameters at once
         * @param attack Attack time in seconds
         * @param decay Decay time in seconds
         * @param sustain Sustain level (0.0f to 1.0f)
         * @param release Release time in seconds
         */
        void setADSR(float attack, float decay, float sustain, float release);

        /**
         * @brief Start the envelope (begin attack phase)
         */
        void noteOn();

        /**
         * @brief Start the release phase
         */
        void noteOff();

        /**
         * @brief Reset envelope to idle state
         */
        void reset();

        /**
         * @brief Get the next envelope sample and advance internal state
         * @return Current envelope amplitude value
         */
        float getNextSample();

        /// @brief The rate the times count in: an envelope made before its rate was known (a voice's, built by
        /// the default constructor at 1 Hz) gets it here
        void setSampleRate(float sample_rate);

        /**
         * @brief Get current envelope amplitude without advancing state
         * @return Current envelope amplitude value
         */
        float getCurrentAmplitude() const;

        /**
         * @brief Peek at envelope amplitude at a future sample offset
         * @param sampleOffset Sample offset from hypothetical note-on start
         * @param releaseSampleOffset Sample offset where release begins (-1 for no release)
         * @return Envelope amplitude at the specified offset
         */
        float peekSampleAt(int sampleOffset, int releaseSampleOffset = -1) const;

        /**
         * @brief Check if envelope is currently active
         * @return true if envelope is not idle, false otherwise
         */
        bool isActive() const;

        /**
         * @brief Check if envelope is in release phase
         * @return true if in release phase, false otherwise
         */
        bool isInRelease() const;

        /**
         * @brief Get the current envelope state
         * @return Current EnvelopeState
         */
        EnvelopeState getState() const;

        /**
         * @brief Get the attack time
         * @return Attack time in seconds
         */
        float getAttackTime() const { return attack_time_; }

        /**
         * @brief Get the decay time
         * @return Decay time in seconds
         */
        float getDecayTime() const { return decay_time_; }

        /**
         * @brief Get the sustain level
         * @return Sustain level (0.0f to 1.0f)
         */
        float getSustainLevel() const { return sustain_level_; }

        /**
         * @brief Get the release time
         * @return Release time in seconds
         */
        float getReleaseTime() const { return release_time_; }

        /**
         * @brief Get the sample rate
         * @return Sample rate in Hz
         */
        float getSampleRate() const { return sample_rate_; }
    };

} // namespace synth
