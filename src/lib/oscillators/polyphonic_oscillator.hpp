/**
 * @file polyphonic_oscillator.hpp
 * @brief Polyphonic oscillator implementation with ADSR envelopes and voice management
 * @author Synth Project
 * @version 1.0
 * @date 2025-09-13
 *
 * This file implements a polyphonic oscillator capable of playing multiple notes
 * simultaneously with independent ADSR envelopes for each voice. Includes voice
 * management, note stealing, aftertouch support, and real-time parameter control.
 */

#pragma once

#include "oscillator.hpp"
#include "adsr_envelope.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace synth
{
    class PCMWaveform; // Forward declaration

    /**
     * @brief Lock-free voice state structure for optimized block processing
     *
     * This structure is used for high-performance audio generation where
     * minimal locking is required during buffer generation.
     */
    struct VoiceState
    {
        bool is_active_;      ///< Whether this voice is currently playing
        float frequency_;     ///< Current frequency in Hz
        float amplitude_;     ///< Current amplitude (0.0-1.0)
        float pan_position_;  ///< Stereo pan position (-1.0 to 1.0)
        uint8_t note_number_; ///< MIDI note number (0-127)
        float velocity_;      ///< Note velocity (0.0-1.0)

        /**
         * @brief Default constructor initializing all values to safe defaults
         */
        VoiceState() : is_active_(false), frequency_(0.0f), amplitude_(0.0f),
                       pan_position_(0.0f), note_number_(0), velocity_(0.0f) {}
    };

    /**
     * @brief Individual voice structure representing a single note in polyphonic playback
     *
     * Each voice contains its own oscillator, ADSR envelope, and state information.
     * Supports smooth voice stealing through crossfade functionality.
     */
    struct Voice
    {
        std::unique_ptr<Oscillator> oscillator_; ///< Audio oscillator for this voice
        std::unique_ptr<ADSREnvelope> envelope_; ///< ADSR envelope for amplitude control
        bool is_active_;                         ///< Voice activation status
        float frequency_;                        ///< Current frequency in Hz
        float amplitude_;                        ///< Current amplitude level
        uint8_t note_number_;                    ///< Associated MIDI note number
        float velocity_;                         ///< Note velocity (0.0-1.0)
        float pan_position_;                     ///< Stereo pan position (-1.0 to 1.0)

        // Crossfade support for smooth voice stealing and transitions
        bool is_crossfading_;             ///< Whether voice is currently crossfading
        float target_amplitude_;          ///< Target amplitude for crossfade
        float crossfade_increment_;       ///< Per-sample amplitude increment
        int crossfade_samples_remaining_; ///< Samples remaining in crossfade

        /**
         * @brief Default constructor initializing voice to inactive state
         */
        Voice() : is_active_(false), frequency_(0.0f), amplitude_(0.0f), note_number_(0),
                  velocity_(0.0f), pan_position_(0.0f), is_crossfading_(false),
                  target_amplitude_(0.0f), crossfade_increment_(0.0f),
                  crossfade_samples_remaining_(0) {}
    };

    /**
     * @brief High-performance polyphonic oscillator supporting up to 32 simultaneous voices
     *
     * This class provides a complete polyphonic synthesizer engine with the following features:
     * - Up to 32 simultaneous voices with individual ADSR envelopes
     * - Smooth voice stealing with crossfade transitions
     * - Real-time MIDI note on/off control
     * - Configurable waveform types (sine, square, sawtooth)
     * - Stereo panning support per voice
     * - Dry/wet mix control for envelope processing
     * - High-performance block-based audio generation
     * - Thread-safe voice management with mutex protection
     *
     * @note For real-time audio applications, prefer the optimized buffer generation
     *       methods over per-sample methods to minimize mutex overhead.
     *
     * Example usage:
     * @code
     * PolyphonicOscillator synth(WaveType::SINE, 48000.0f);
     * synth.setADSR(0.1f, 0.3f, 0.7f, 0.5f);  // Attack, Decay, Sustain, Release
     * synth.noteOn(60, 0.8f);  // Middle C with 80% velocity
     *
     * float buffer[1024];
     * synth.generateBuffer(buffer, 1024);  // Generate audio samples
     *
     * synth.noteOff(60);  // Release the note
     * @endcode
     */
    class PolyphonicOscillator
    {
    private:
        static constexpr int MAX_VOICES = 32;  ///< Maximum number of simultaneous voices
        std::array<Voice, MAX_VOICES> voices_; ///< Array of all available voices
        float sample_rate_;                    ///< Audio sample rate in Hz
        WaveType wave_type_;                   ///< Current waveform type for all voices
        mutable std::mutex voices_mutex_;      ///< Mutex protecting voice array access

        // ADSR parameters applied to all new voices
        float attack_time_;   ///< Default attack time in seconds
        float decay_time_;    ///< Default decay time in seconds
        float sustain_level_; ///< Default sustain level (0.0-1.0)
        float release_time_;  ///< Default release time in seconds

        float dry_wet_mix_; ///< Dry/wet mix (0.0=dry, 1.0=wet envelope)

        /**
         * @brief Find the first available inactive voice
         * @return Voice index if found, -1 if all voices are active
         */
        int findFreeVoice() const;

        /**
         * @brief Find the oldest active voice for voice stealing
         * @return Voice index of the oldest voice
         */
        int findOldestVoice() const;

        /**
         * @brief Find the voice currently playing a specific MIDI note
         * @param note_number MIDI note number to search for
         * @return Voice index if found, -1 if note is not active
         */
        int findVoiceByNote(uint8_t note_number) const;

        /**
         * @brief Initiate a smooth crossfade transition for a voice
         * @param voice Reference to the voice to crossfade
         * @param target_amplitude Target amplitude to reach
         * @param crossfade_duration_ms Duration of crossfade in milliseconds
         */
        void initiateCrossfade(Voice &voice, float target_amplitude, float crossfade_duration_ms = 5.0f);

        /**
         * @brief Process one sample of crossfade for a voice
         * @param voice Reference to the voice being crossfaded
         */
        void processCrossfade(Voice &voice);

    public:
        /**
         * @brief Construct a new polyphonic oscillator
         * @param wave_type Initial waveform type for all voices
         * @param sample_rate Audio sample rate in Hz
         */
        PolyphonicOscillator(WaveType wave_type, float sample_rate);

        /**
         * @brief Default destructor
         */
        ~PolyphonicOscillator() = default;

        // === Voice Management ===

        /**
         * @brief Start playing a MIDI note
         * @param note_number MIDI note number (0-127)
         * @param velocity Note velocity (0.0-1.0)
         * @param pan Stereo pan position (-1.0 to 1.0, 0.0 = center)
         *
         * If the note is already playing, it will be retriggered.
         * If all voices are busy, the oldest voice will be stolen.
         */
        void noteOn(uint8_t note_number, float velocity = 1.0f, float pan = 0.0f);

        /**
         * @brief Stop playing a MIDI note
         * @param note_number MIDI note number to stop
         *
         * Triggers the ADSR release phase for the specified note.
         */
        void noteOff(uint8_t note_number);

        /**
         * @brief Stop all currently playing notes immediately
         */
        void allNotesOff();

        // === Parameter Control ===

        /**
         * @brief Change the waveform type for all voices
         * @param wave_type New waveform type (SINE, SQUARE, SAWTOOTH)
         */
        void setWaveType(WaveType wave_type);

        /**
         * @brief Set custom PCM waveform for all voices
         * @param pcm_data Vector of floating-point PCM samples for one complete cycle
         */
        void setCustomWaveform(const std::vector<float> &pcm_data);

        /**
         * @brief Set custom waveform for all voices using PCMWaveform object (preserves wav bank metadata)
         * @param waveform Shared pointer to PCMWaveform object
         */
        void setCustomWaveform(std::shared_ptr<PCMWaveform> waveform);

        /**
         * @brief Set wav bank position for all voices (for custom wav banks)
         * @param position Bank position (0.0 to 1.0)
         */
        void setBankPosition(float position);

        /**
         * @brief Get current wav bank position
         * @return Current bank position (0.0 to 1.0)
         */
        float getBankPosition() const;

        /**
         * @brief Check if any voice is using a wav bank
         * @return True if custom waveform is a wav bank
         */
        bool isWavBank() const;

        /**
         * @brief Set master amplitude scaling for all voices
         * @param amplitude Master amplitude multiplier (0.0-1.0)
         */
        void setMasterAmplitude(float amplitude);

        /**
         * @brief Set stereo pan position for a specific voice
         * @param note_number MIDI note number of the voice to pan
         * @param pan Pan position (-1.0=left, 0.0=center, 1.0=right)
         */
        void setVoicePan(uint8_t note_number, float pan);

        // === ADSR Envelope Control ===

        /**
         * @brief Set ADSR parameters for all voices
         * @param attack Attack time in seconds
         * @param decay Decay time in seconds
         * @param sustain Sustain level (0.0-1.0)
         * @param release Release time in seconds
         */
        void setADSR(float attack, float decay, float sustain, float release);

        /**
         * @brief Set attack time for all voices
         * @param attack_seconds Attack time in seconds
         */
        void setAttackTime(float attack_seconds);

        /**
         * @brief Set decay time for all voices
         * @param decay_seconds Decay time in seconds
         */
        void setDecayTime(float decay_seconds);

        /**
         * @brief Set sustain level for all voices
         * @param sustain_level Sustain level (0.0-1.0)
         */
        void setSustainLevel(float sustain_level);

        /**
         * @brief Set release time for all voices
         * @param release_seconds Release time in seconds
         */
        void setReleaseTime(float release_seconds);

        // === Dry/Wet Mix Control ===

        /**
         * @brief Set the dry/wet mix for envelope processing
         * @param mix Mix level (0.0=100% dry, 1.0=100% wet envelope)
         */
        void setDryWetMix(float mix);

        /**
         * @brief Get current dry/wet mix setting
         * @return Current mix level (0.0-1.0)
         */
        float getDryWetMix() const { return dry_wet_mix_; }

        // === ADSR Getters ===

        /**
         * @brief Get current attack time
         * @return Attack time in seconds
         */
        float getAttackTime() const { return attack_time_; }

        /**
         * @brief Get current decay time
         * @return Decay time in seconds
         */
        float getDecayTime() const { return decay_time_; }

        /**
         * @brief Get current sustain level
         * @return Sustain level (0.0-1.0)
         */
        float getSustainLevel() const { return sustain_level_; }

        /**
         * @brief Get current release time
         * @return Release time in seconds
         */
        float getReleaseTime() const { return release_time_; }

        // === Audio Generation ===

        /**
         * @brief Generate a single mono sample (legacy method)
         * @return Mixed audio sample from all active voices
         * @warning This method locks a mutex per sample call - use buffer methods for better performance
         */
        float getSample();

        /**
         * @brief Generate a single stereo sample (legacy method)
         * @return Stereo sample with left and right channels
         * @warning This method locks a mutex per sample call - use buffer methods for better performance
         */
        StereoSample getStereoSample();

        /**
         * @brief Generate a buffer of mono samples (recommended)
         * @param buffer Pointer to output buffer
         * @param size Number of samples to generate
         */
        void generateBuffer(float *buffer, int size);

        /**
         * @brief Generate a buffer of stereo samples (recommended)
         * @param buffer Pointer to interleaved stereo output buffer (L,R,L,R...)
         * @param frames Number of stereo frames to generate
         */
        void generateStereoBuffer(float *buffer, int frames);

        /**
         * @brief Generate mono buffer with optimized locking strategy
         * @param buffer Pointer to output buffer
         * @param size Number of samples to generate
         */
        void generateBufferOptimized(float *buffer, int size);

        /**
         * @brief Generate stereo buffer with optimized locking strategy
         * @param buffer Pointer to interleaved stereo output buffer
         * @param frames Number of stereo frames to generate
         */
        void generateStereoBufferOptimized(float *buffer, int frames);

        // === Voice Status ===

        /**
         * @brief Get the number of currently active voices
         * @return Number of active voices (0-32)
         */
        int getActiveVoiceCount() const;

        /**
         * @brief Get the number of voices in release phase
         * @return Number of releasing voices
         */
        int getReleasingVoiceCount() const;

        /**
         * @brief Check if a specific MIDI note is currently playing
         * @param note_number MIDI note number to check
         * @return True if the note is active, false otherwise
         */
        bool isNoteActive(uint8_t note_number) const;

        /**
         * @brief Check if any voices are currently active
         * @return True if any voice is active, false if all are silent
         */
        bool isActive() const;

        // === Audio Conversion Utilities ===

        /**
         * @brief Convert and quantize float buffer to 16-bit integer with SIMD optimization
         * @param floatBuffer Input buffer of float samples
         * @param shortBuffer Output buffer for 16-bit samples
         * @param sampleCount Number of samples to convert
         */
        static void convertAndQuantize(const float *floatBuffer, short *shortBuffer, int sampleCount);

        /**
         * @brief Convert single float sample to 16-bit integer with dithering
         * @param sample Input float sample (-1.0 to 1.0)
         * @return 16-bit integer sample
         */
        static short convertSampleToShort(float sample);

        // === Getters ===

        /**
         * @brief Get current waveform type
         * @return Current wave type
         */
        WaveType getWaveType() const { return wave_type_; }

        /**
         * @brief Get sample rate
         * @return Sample rate in Hz
         */
        float getSampleRate() const { return sample_rate_; }

        /**
         * @brief Convert MIDI note number to frequency in Hz
         * @param note_number MIDI note number (0-127)
         * @return Frequency in Hz (A4 = 440 Hz is note 69)
         */
        static float noteToFrequency(uint8_t note_number);

        /**
         * @brief Reset phase for all active oscillators
         */
        void reset() const;

        // === FM Synthesis Control ===

        /**
         * @brief Set FM modulator ratio for all voices
         * @param ratio Frequency ratio of modulator to carrier (0.1-10.0)
         */
        void setFMModulatorRatio(float ratio);

        /**
         * @brief Set FM modulation index for all voices
         * @param index Modulation depth (0.0-20.0)
         */
        void setFMModulationIndex(float index);

        /**
         * @brief Set FM algorithm for all voices
         * @param algorithm FM algorithm type
         */
        void setFMAlgorithm(FMAlgorithm algorithm);

        /**
         * @brief Set FM operator ratio for all voices
         * @param operator_num Operator number (2, 3, or 4)
         * @param ratio Frequency ratio of operator (0.1-10.0)
         */
        void setFMOperatorRatio(int operator_num, float ratio);

        /**
         * @brief Set FM operator level for all voices
         * @param operator_num Operator number (2, 3, or 4)
         * @param level Output level of operator (0.0-1.0)
         */
        void setFMOperatorLevel(int operator_num, float level);

        /**
         * @brief Set FM feedback amount for all voices
         * @param feedback Feedback amount (0.0-1.0)
         */
        void setFMFeedback(float feedback);

        /**
         * @brief Get current FM algorithm for voices
         * @return Current FM algorithm type
         */
        FMAlgorithm getFMAlgorithm() const;
    };

} // namespace synth
