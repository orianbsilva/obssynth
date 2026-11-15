/**
 * @file synth_engine.hpp
 * @brief High-level synthesizer engine wrapping VoiceAllocator
 *
 * SynthEngine provides a PolyphonicOscillator-compatible API while using
 * the new Voice/VoiceAllocator architecture internally. This enables:
 * - Per-voice filtering with velocity modulation
 * - 4 oscillator chains per voice (for ObsSynth4D compatibility)
 * - ModMatrix integration (future)
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>
#include "../synth/chains/osc_chain.hpp"
#include "../synth/chains/voice.hpp"
#include "../synth/chains/voice_allocator.hpp"
#include "../synth/modulation/mod_matrix.hpp"
#include "../oscillators/oscillator.hpp"
#include <algorithm>
#include <cmath>
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace synth
{

    // Forward declaration
    class PCMWaveform;

    /**
     * @brief High-level synthesizer engine with per-voice filtering
     *
     * Drop-in replacement for PolyphonicOscillator with additional features:
     * - Per-voice filter with velocity modulation
     * - 4 oscillator chains per voice
     * - Configurable voice count
     *
     * Example usage:
     * @code
     * SynthEngine synth(48000.0f);
     * synth.setWaveType(WaveType::SAW);
     * synth.setCutoff(2000.0f);
     * synth.setFilterVelocitySensitivity(0.5f);  // Velocity opens filter
     *
     * synth.noteOn(60, 0.8f);  // C4 with velocity affecting filter
     * synth.generateStereoBuffer(buffer, frames);
     * @endcode
     */
    class SynthEngine
    {
    public:
        static constexpr int MAX_VOICES = 32;
        static constexpr int CHAINS_PER_VOICE = 4;

    private:
        // Voice allocator with 4 chains per voice
        VoiceAllocator<SynthVoice<CHAINS_PER_VOICE>, MAX_VOICES> allocator_;

        // Modulation matrix for flexible routing
        ModMatrix mod_matrix_;

        // Global parameters
        float sample_rate_;
        WaveType wave_type_;

        // Filter parameters
        float base_cutoff_;
        float resonance_;
        float filter_env_amount_;
        float filter_velocity_sensitivity_;
        float hpf_cutoff_;
        float key_track_amount_;

        // ADSR parameters
        float attack_time_;
        float decay_time_;
        float sustain_level_;
        float release_time_;

        // Global modulation sources
        float mod_wheel_;        // CC1 (0-1)
        float channel_pressure_; // Channel aftertouch (0-1)
        float pitch_bend_;       // Pitch bend (-1 to +1)

        // Configurable source ID mappings (set by product via setSourceIds)
        int aftertouch_source_id_ = -1;

    public:
        // ── Vector synthesis: the four oscillators at the corners of a square (1 top left, 2 top right, 3 bottom
        // left, 4 bottom right); each voice walks a path in it from its note-on (time in seconds, x and y 0..1),
        // moved by the pad (its centre leaves the path as recorded), and hears each oscillator by its nearness
        // (equal power). Off: every oscillator whole.
        struct VectorPoint
        {
            float t, x, y;
        };
        static constexpr int VECTOR_POINTS = 16;
        void setVectorEnabled(bool on) { vector_on_ = on; }
        void setVectorLoop(bool loop) { vector_loop_ = loop; }
        void setVectorPad(float x, float y)
        {
            vector_pad_x_ = std::clamp(x, 0.0f, 1.0f);
            vector_pad_y_ = std::clamp(y, 0.0f, 1.0f);
        }
        void setVectorPoint(int i, float t, float x, float y)
        {
            if (i >= 0 && i < VECTOR_POINTS)
                vector_path_[static_cast<size_t>(i)] = {std::max(0.0f, t), std::clamp(x, 0.0f, 1.0f), std::clamp(y, 0.0f, 1.0f)};
        }
        void setVectorCount(int n) { vector_count_ = std::clamp(n, 0, VECTOR_POINTS); }
        bool vectorEnabled() const { return vector_on_; }
        // where the newest voice is in the square now (for a display)
        float vectorX() const { return vector_now_x_; }
        float vectorY() const { return vector_now_y_; }

    private:
        bool vector_on_ = false, vector_loop_ = false;
        float vector_pad_x_ = 0.5f, vector_pad_y_ = 0.5f, vector_now_x_ = 0.5f, vector_now_y_ = 0.5f;
        std::array<VectorPoint, VECTOR_POINTS> vector_path_{};
        int vector_count_ = 0;

        // the path at t seconds (the pad's position when there is no path)
        void vectorAt(float t, float &x, float &y) const
        {
            if (vector_count_ == 0)
            {
                x = vector_pad_x_;
                y = vector_pad_y_;
                return;
            }
            const float end = vector_path_[static_cast<size_t>(vector_count_ - 1)].t;
            if (vector_loop_ && end > 0.0f)
                t = std::fmod(t, end);
            x = vector_path_[static_cast<size_t>(vector_count_ - 1)].x;
            y = vector_path_[static_cast<size_t>(vector_count_ - 1)].y;
            if (t <= vector_path_[0].t)
            {
                x = vector_path_[0].x;
                y = vector_path_[0].y;
            }
            else
                for (int i = 1; i < vector_count_; ++i)
                {
                    const VectorPoint &a = vector_path_[static_cast<size_t>(i - 1)], &b = vector_path_[static_cast<size_t>(i)];
                    if (t <= b.t)
                    {
                        const float f = b.t > a.t ? (t - a.t) / (b.t - a.t) : 1.0f;
                        x = a.x + (b.x - a.x) * f;
                        y = a.y + (b.y - a.y) * f;
                        break;
                    }
                }
            x = std::clamp(x + vector_pad_x_ - 0.5f, 0.0f, 1.0f);
            y = std::clamp(y + vector_pad_y_ - 0.5f, 0.0f, 1.0f);
        }

    public:
        // the matrix's sources each voice has of its own (ids in the product's source list; -1: none). The
        // envelopes' ids are their first: chain c at id + c, mod envelope e at id + e.
        struct VoiceSourceIds
        {
            int velocity = -1, keytrack = -1, random = -1, filterEnv = -1, ampEnv = -1, modEnv = -1;
        };
        void setVoiceSourceIds(const VoiceSourceIds &ids) { voice_ids_ = ids; }

    private:
        static constexpr int VOICE_DESTS = 72;
        VoiceSourceIds voice_ids_;
        std::array<float, MAX_MOD_SOURCES> voice_src_{};
        std::array<float, VOICE_DESTS> voice_dst_{};
        const void *last_voice_ = nullptr;
        int mod_wheel_source_id_ = -1;
        int pitch_bend_source_id_ = -1;

        // Dry/wet mix

        // Per-sample callback (for advancing LFOs etc)
        std::function<void()> per_sample_callback_;
        int control_rate_ = 1;  // the modulation (callback, matrix, chains) once per this many samples
        int control_count_ = 0;

        // Thread safety
        mutable std::mutex params_mutex_;

        // Apply current parameters to all voices/chains (defined at end of class)

    public:
        /**
         * @brief Construct a new SynthEngine
         * @param sample_rate Audio sample rate in Hz
         * @param wave_type Initial waveform type
         */
        explicit SynthEngine(float sample_rate, WaveType wave_type = WaveType::SAW)
            : allocator_(sample_rate, VoiceAllocationStrategy::OLDEST), sample_rate_(sample_rate), wave_type_(wave_type), base_cutoff_(10000.0f), resonance_(0.0f), filter_env_amount_(0.0f), filter_velocity_sensitivity_(0.0f), hpf_cutoff_(20.0f), key_track_amount_(0.0f), attack_time_(0.01f), decay_time_(0.1f), sustain_level_(0.7f), release_time_(0.3f), mod_wheel_(0.0f), channel_pressure_(0.0f), pitch_bend_(0.0f)
        {
            // Configure all chains with initial parameters
            applyParametersToAllChains();

            // Setup default modulation routes
            setupDefaultModRoutes();
        }

        ~SynthEngine() = default;

        // ============ ModMatrix Access ============

        /**
         * @brief Get reference to the ModMatrix
         */
        ModMatrix &getModMatrix() { return mod_matrix_; }
        const ModMatrix &getModMatrix() const { return mod_matrix_; }

        /**
         * @brief Setup default modulation routes (no-op in generic engine)
         *
         * Products should configure their own routes via getModMatrix().
         */
        void setupDefaultModRoutes()
        {
            mod_matrix_.clear();
        }

        /**
         * @brief Configure source IDs for common controllers
         *
         * Products call this to map aftertouch/modwheel/pitchbend to
         * their product-specific source IDs in the ModMatrix.
         */
        void setSourceIds(int aftertouch_id, int mod_wheel_id, int pitch_bend_id)
        {
            aftertouch_source_id_ = aftertouch_id;
            mod_wheel_source_id_ = mod_wheel_id;
            pitch_bend_source_id_ = pitch_bend_id;
        }

        /**
         * @brief Add a modulation route
         * @param slot Slot index (0-31)
         * @param source Modulation source ID
         * @param dest Modulation destination ID
         * @param amount Amount (-2.0 to +2.0)
         * @return true if successful
         */
        bool setModRoute(int slot, int source, int dest, float amount)
        {
            return mod_matrix_.setSlot(slot, source, dest, amount);
        }

        /**
         * @brief Set mod route amount
         */
        void setModRouteAmount(int slot, float amount)
        {
            mod_matrix_.setSlotAmount(slot, amount);
        }

        // ============ Voice Management (PolyphonicOscillator compatible) ============

        /**
         * @brief Start playing a MIDI note
         * @param note_number MIDI note number (0-127)
         * @param velocity Note velocity (0.0-1.0)
         * @param pan Stereo pan position (-1.0 to 1.0, 0.0 = center)
         */
        void noteOn(uint8_t note_number, float velocity = 1.0f, float pan = 0.0f)
        {
            auto *voice = allocator_.noteOn(note_number, velocity);
            if (voice)
            {
                // Apply velocity to filter cutoff modulation
                float cutoff_mod = velocity * filter_velocity_sensitivity_;
                voice->setCutoffMod(cutoff_mod);
                // (each note used to set every oscillator's pan to this one: a preset's stereo spread was gone
                // at the first note; the per-voice matrix sets the pans now)
                (void)pan;
                last_voice_ = voice;
            }
        }

        /**
         * @brief Stop playing a MIDI note
         * @param note_number MIDI note number to stop
         */
        void noteOff(uint8_t note_number)
        {
            allocator_.noteOff(note_number);
        }

        /**
         * @brief Stop all currently playing notes
         */
        void allNotesOff()
        {
            allocator_.allNotesOff();
        }

        /**
         * @brief Panic - force stop all voices immediately
         */
        void panic()
        {
            allocator_.panic();
        }

        // ============ Waveform Control ============

        /**
         * @brief Change the waveform type for all voices
         * @param wave_type New waveform type
         */
        void setWaveType(WaveType wave_type)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            wave_type_ = wave_type;

            allocator_.configureChains([wave_type](OscillatorChain &chain, int)
                                       { chain.setWaveType(wave_type); });
        }

        WaveType getWaveType() const { return wave_type_; }

        // ============ Filter Control (NEW!) ============

        /**
         * @brief Set filter cutoff frequency for all voices
         * @param cutoff Cutoff frequency in Hz (20-20000)
         */
        void setCutoff(float cutoff)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            base_cutoff_ = std::clamp(cutoff, 20.0f, 20000.0f);

            allocator_.configureChains([cutoff = base_cutoff_](OscillatorChain &chain, int)
                                       { chain.setCutoff(cutoff); });
        }

        float getCutoff() const { return base_cutoff_; }

        /**
         * @brief Set filter resonance for all voices
         * @param resonance Resonance (0.0-1.0)
         */
        void setResonance(float resonance)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            resonance_ = std::clamp(resonance, 0.0f, 1.0f);

            allocator_.configureChains([res = resonance_](OscillatorChain &chain, int)
                                       { chain.setResonance(res); });
        }

        float getResonance() const { return resonance_; }

        /**
         * @brief Set filter type for all voices
         * @param type Filter type (LOWPASS, HIGHPASS, BANDPASS, NOTCH)
         */
        void setFilterType(BiquadType type)
        {
            allocator_.configureChains([type](OscillatorChain &chain, int)
                                       { chain.setFilterType(type); });
        }

        /**
         * @brief Set filter envelope amount
         * @param amount Envelope modulation amount (-1.0 to 1.0)
         */
        void setFilterEnvAmount(float amount)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            filter_env_amount_ = std::clamp(amount, -1.0f, 1.0f);

            allocator_.configureChains([amt = filter_env_amount_](OscillatorChain &chain, int)
                                       { chain.setFilterEnvAmount(amt); });
        }

        float getFilterEnvAmount() const { return filter_env_amount_; }

        /**
         * @brief Set velocity sensitivity for filter cutoff
         * @param sensitivity Sensitivity (0.0-1.0, 0=no effect, 1=full velocity range)
         */
        void setFilterVelocitySensitivity(float sensitivity)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            filter_velocity_sensitivity_ = std::clamp(sensitivity, 0.0f, 1.0f);
        }

        float getFilterVelocitySensitivity() const { return filter_velocity_sensitivity_; }

        void setHPFCutoff(float cutoff)
        {
            hpf_cutoff_ = std::clamp(cutoff, 20.0f, 2000.0f);
            allocator_.configureChains([c = hpf_cutoff_](OscillatorChain &chain, int)
                                       { chain.setHPFCutoff(c); });
        }
        float getHPFCutoff() const { return hpf_cutoff_; }

        void setKeyTrackAmount(float amount)
        {
            key_track_amount_ = std::clamp(amount, 0.0f, 1.0f);
            allocator_.configureChains([a = key_track_amount_](OscillatorChain &chain, int)
                                       { chain.setKeyTrackAmount(a); });
        }
        float getKeyTrackAmount() const { return key_track_amount_; }

        // ============ ADSR Envelope Control ============

        /**
         * @brief Set ADSR parameters for all voices
         */
        void setADSR(float attack, float decay, float sustain, float release)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            attack_time_ = attack;
            decay_time_ = decay;
            sustain_level_ = sustain;
            release_time_ = release;

            allocator_.configureChains([=](OscillatorChain &chain, int)
                                       {
                chain.setAmpAttack(attack);
                chain.setAmpDecay(decay);
                chain.setAmpSustain(sustain);
                chain.setAmpRelease(release); });
        }

        void setAttackTime(float seconds)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            attack_time_ = seconds;
            allocator_.configureChains([seconds](OscillatorChain &chain, int)
                                       { chain.setAmpAttack(seconds); });
        }

        void setDecayTime(float seconds)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            decay_time_ = seconds;
            allocator_.configureChains([seconds](OscillatorChain &chain, int)
                                       { chain.setAmpDecay(seconds); });
        }

        void setSustainLevel(float level)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            sustain_level_ = level;
            allocator_.configureChains([level](OscillatorChain &chain, int)
                                       { chain.setAmpSustain(level); });
        }

        void setReleaseTime(float seconds)
        {
            std::lock_guard<std::mutex> lock(params_mutex_);
            release_time_ = seconds;
            allocator_.configureChains([seconds](OscillatorChain &chain, int)
                                       { chain.setAmpRelease(seconds); });
        }

        float getAttackTime() const { return attack_time_; }
        float getDecayTime() const { return decay_time_; }
        float getSustainLevel() const { return sustain_level_; }
        float getReleaseTime() const { return release_time_; }

        // ============ Filter Envelope Control ============

        void setFilterAttack(float seconds)
        {
            allocator_.configureChains([seconds](OscillatorChain &chain, int)
                                       { chain.setFilterAttack(seconds); });
        }

        void setFilterDecay(float seconds)
        {
            allocator_.configureChains([seconds](OscillatorChain &chain, int)
                                       { chain.setFilterDecay(seconds); });
        }

        void setFilterSustain(float level)
        {
            allocator_.configureChains([level](OscillatorChain &chain, int)
                                       { chain.setFilterSustain(level); });
        }

        void setFilterRelease(float seconds)
        {
            allocator_.configureChains([seconds](OscillatorChain &chain, int)
                                       { chain.setFilterRelease(seconds); });
        }

        // ============ Aftertouch ============

        /**
         * @brief Set polyphonic aftertouch for a specific note
         */
        void setPolyAftertouch(uint8_t note_number, float value)
        {
            allocator_.setPolyAftertouch(note_number, value);
        }

        /**
         * @brief Set channel aftertouch (affects all voices)
         */
        void setChannelAftertouch(float value)
        {
            channel_pressure_ = value;
            if (aftertouch_source_id_ >= 0)
                mod_matrix_.setSourceValue(aftertouch_source_id_, value);
            allocator_.setChannelAftertouch(value);
        }

        /**
         * @brief Set mod wheel value (CC1)
         */
        void setModWheel(float value)
        {
            mod_wheel_ = std::clamp(value, 0.0f, 1.0f);
            if (mod_wheel_source_id_ >= 0)
                mod_matrix_.setSourceValue(mod_wheel_source_id_, mod_wheel_);
        }

        float getModWheel() const { return mod_wheel_; }

        // ============ Pitch Bend ============

        /**
         * @brief Set pitch bend for all voices
         * @param semitones Pitch bend in semitones
         */
        void setPitchBend(float semitones)
        {
            pitch_bend_ = semitones;
            // Normalize to -1..+1 range (assuming ±2 semitone range)
            if (pitch_bend_source_id_ >= 0)
                mod_matrix_.setSourceValue(pitch_bend_source_id_, semitones / 2.0f);
            allocator_.setPitchBend(semitones);
        }

        // ============ Audio Generation ============

        /**
         * @brief Process modulation matrix (call once per block or when sources change)
         */
        void processModulation()
        {
            mod_matrix_.process();
        }

        /**
         * @brief Generate a single mono sample
         *
         * Caller must call processModulation() before if mod sources changed.
         */
        float getSample()
        {
            return allocator_.process();
        }

        /**
         * @brief Set a callback that is invoked once per sample (for LFO advance, etc)
         */
        void setPerSampleCallback(std::function<void()> cb)
        {
            per_sample_callback_ = std::move(cb);
        }

        /**
         * @brief The modulation at a control rate: the per-sample callback, the matrix and the chains' targets once
         * every n samples (1: every sample, as always). The voices' mod envelopes run at the control rate, so their
         * times hold; the caller's LFOs must do the same (the callback is their clock). For machines that cannot
         * afford the matrix at the audio rate: at 96 kHz, n = 32 is a third of a millisecond between steps.
         */
        void setControlRate(int n)
        {
            control_rate_ = std::clamp(n, 1, 256);
            control_count_ = 0;
            for (int v = 0; v < MAX_VOICES; ++v)
            {
                auto &voice = allocator_.getVoice(v);
                voice.setModEnvRate(sample_rate_ / static_cast<float>(control_rate_));
                for (int c = 0; c < CHAINS_PER_VOICE; ++c)
                    voice.getChain(c).setModRamp(control_rate_); // the level and pan glide between the steps
            }
        }
        int getControlRate() const { return control_rate_; }
        /// Oscillators at level 0 are not rendered (their envelopes run): a small machine's profile
        void setSkipSilent(bool on)
        {
            for (int v = 0; v < MAX_VOICES; ++v)
                for (int c = 0; c < CHAINS_PER_VOICE; ++c)
                    allocator_.getVoice(v).getChain(c).setSkipSilent(on);
        }

                /**
         * @brief Generate a buffer of mono samples
         */
        void generateBuffer(float *buffer, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                controlStep();
                buffer[i] = allocator_.process();
            }
        }

        /**
         * @brief Generate a buffer of stereo samples (interleaved L,R,L,R...)
         */
        void generateStereoBuffer(float *buffer, int frames)
        {
            for (int i = 0; i < frames; ++i)
            {
                controlStep();
                float left, right;
                allocator_.processStereo(left, right);
                buffer[i * 2] = left;
                buffer[i * 2 + 1] = right;
            }
        }

        // Optimized versions (same implementation, for API compatibility)
        void generateBufferOptimized(float *buffer, int size)
        {
            generateBuffer(buffer, size);
        }

        void generateStereoBufferOptimized(float *buffer, int frames)
        {
            generateStereoBuffer(buffer, frames);
        }

        // ============ Voice Status ============

        int getActiveVoiceCount() const
        {
            return allocator_.getActiveVoiceCount();
        }

        int getReleasingVoiceCount() const
        {
            return allocator_.getReleasingVoiceCount();
        }

        bool isNoteActive(uint8_t note_number) const
        {
            return allocator_.isNotePlaying(note_number);
        }

        bool isActive() const
        {
            return allocator_.isActive();
        }

        // ============ Utilities ============

        float getSampleRate() const { return sample_rate_; }

        static float noteToFrequency(uint8_t note_number)
        {
            // Delegate to OscillatorChain's lookup table for consistency
            return OscillatorChain::midiNoteToFrequency(static_cast<int>(note_number));
        }

        /**
         * @brief Convert and quantize float buffer to 16-bit integer
         */
        static void convertAndQuantize(const float *floatBuffer, short *shortBuffer, int sampleCount)
        {
            for (int i = 0; i < sampleCount; ++i)
            {
                float sample = floatBuffer[i];
                if (sample > 1.0f)
                    sample = 1.0f;
                else if (sample < -1.0f)
                    sample = -1.0f;
                shortBuffer[i] = static_cast<short>(sample * 32767.0f);
            }
        }

        static short convertSampleToShort(float sample)
        {
            if (sample > 1.0f)
                sample = 1.0f;
            else if (sample < -1.0f)
                sample = -1.0f;
            return static_cast<short>(sample * 32767.0f);
        }

        // ============ Voice Allocation Strategy ============

        void setVoiceAllocationStrategy(VoiceAllocationStrategy strategy)
        {
            allocator_.setStrategy(strategy);
        }

        // ============ Direct Allocator Access (for advanced use) ============

        VoiceAllocator<SynthVoice<CHAINS_PER_VOICE>, MAX_VOICES> &getAllocator()
        {
            return allocator_;
        }

        const VoiceAllocator<SynthVoice<CHAINS_PER_VOICE>, MAX_VOICES> &getAllocator() const
        {
            return allocator_;
        }

    private:
        /**
         * @brief Apply ModMatrix destination values to all active voice chains
         *
         * Maps dest IDs to chain parameters:
         * OSC1 = dests 0-16, OSC2 = 17-33, OSC3 = 34-50, OSC4 = 51-67
         * Offsets: PITCH=0, LEVEL=3, PAN=4, CUTOFF=7, RESO=8
         */
        void controlStep()
        {
            if (control_count_ == 0)
            {
                if (per_sample_callback_)
                    per_sample_callback_();
                mod_matrix_.process();
                applyModMatrixToChains();
            }
            if (++control_count_ >= control_rate_)
                control_count_ = 0;
        }

        void applyModMatrixToChains()
        {
            // the oscillators' destinations (0..71), per voice: the matrix on the sources the voice has of its own
            // (velocity, key, random, its envelopes, its mod envelopes) over the instrument's (LFOs, wheel, XY...).
            // Offsets in each oscillator's block of 17; the wavetable positions at 68..71.
            constexpr int CHAIN_DEST_BASE[4] = {0, 17, 34, 51};
            constexpr int OFF_PITCH = 0, OFF_FINE = 1, OFF_LEVEL = 3, OFF_PAN = 4, OFF_FM_INDEX = 6, OFF_CUTOFF = 7, OFF_RESO = 8, OFF_FENV = 9;
            constexpr int WT_MORPH_BASE = 68;
            const float *global = mod_matrix_.sourceValues();
            const VoiceSourceIds ids = voice_ids_; // a copy: nothing in the loop can change it under the bounds below
            // an envelope's run of ids (chain c at id + c, mod envelope e at id + e) must fit the matrix's sources
            const bool filterEnvOk = ids.filterEnv >= 0 && ids.filterEnv <= MAX_MOD_SOURCES - CHAINS_PER_VOICE;
            const bool ampEnvOk = ids.ampEnv >= 0 && ids.ampEnv <= MAX_MOD_SOURCES - CHAINS_PER_VOICE;
            const bool modEnvOk = ids.modEnv >= 0 && ids.modEnv <= MAX_MOD_SOURCES - 3;
            // the newest voice still playing (by note order: the last note may have ended while a chord holds)
            const void *newest = nullptr;
            uint32_t newestId = 0;
            for (int v = 0; v < MAX_VOICES; ++v)
            {
                const auto &voice = allocator_.getVoice(v);
                if (voice.isActive() && (!newest || voice.getNoteId() - newestId < 0x80000000u))
                {
                    newest = &voice;
                    newestId = voice.getNoteId();
                }
            }
            // the mixer's destinations: every voice's oscillator c, its level and pan (presets route the XY there)
            float mixLevel[4], mixPan[4];
            for (int c = 0; c < 4; ++c)
            {
                mixLevel[c] = mod_matrix_.getModValue(200 + c);
                mixPan[c] = mod_matrix_.getModValue(204 + c);
            }
            for (int v = 0; v < MAX_VOICES; ++v)
            {
                auto &voice = allocator_.getVoice(v);
                if (!voice.isActive())
                    continue;
                voice.advanceModEnvs();
                std::copy(global, global + MAX_MOD_SOURCES, voice_src_.begin());
                if (ids.velocity >= 0)
                    voice_src_[ids.velocity] = voice.getVelocity();
                if (aftertouch_source_id_ >= 0) // a note's own pressure (poly), or the channel's
                    voice_src_[aftertouch_source_id_] = std::max(voice_src_[aftertouch_source_id_], voice.getAftertouch());
                if (ids.keytrack >= 0)
                    voice_src_[ids.keytrack] = static_cast<float>(std::max(voice.getNote(), 0)) / 127.0f;
                if (ids.random >= 0)
                    voice_src_[ids.random] = voice.randomValue();
                for (int c = 0; c < CHAINS_PER_VOICE; ++c)
                {
                    if (filterEnvOk)
                        voice_src_[ids.filterEnv + c] = voice.getChain(c).filterEnvLevel();
                    if (ampEnvOk)
                        voice_src_[ids.ampEnv + c] = voice.getChain(c).ampEnvLevel();
                }
                if (modEnvOk)
                    for (int e = 0; e < 3; ++e)
                        voice_src_[ids.modEnv + e] = voice.modEnvValue(e);
                if (&voice == newest) // the instrument's destinations (tube, volume, LFOs) hear the newest note still playing
                {
                    for (int id : {ids.velocity, ids.keytrack, ids.random})
                        if (id >= 0)
                            mod_matrix_.setSourceValue(id, voice_src_[id]);
                    if (modEnvOk)
                        for (int e = 0; e < 3; ++e)
                            mod_matrix_.setSourceValue(ids.modEnv + e, voice_src_[ids.modEnv + e]);
                    for (int c = 0; c < CHAINS_PER_VOICE; ++c) // and its envelopes (they reached no global destination)
                    {
                        if (filterEnvOk)
                            mod_matrix_.setSourceValue(ids.filterEnv + c, voice_src_[ids.filterEnv + c]);
                        if (ampEnvOk)
                            mod_matrix_.setSourceValue(ids.ampEnv + c, voice_src_[ids.ampEnv + c]);
                    }
                }
                mod_matrix_.evaluate(voice_src_.data(), voice_dst_.data(), VOICE_DESTS);

                // the vector mix: this voice's place in the square, each corner's share (equal power)
                float vg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                if (vector_on_)
                {
                    float x, y;
                    vectorAt(static_cast<float>(voice.getAge()) / sample_rate_, x, y);
                    vg[0] = std::sqrt((1.0f - x) * y);
                    vg[1] = std::sqrt(x * y);
                    vg[2] = std::sqrt((1.0f - x) * (1.0f - y));
                    vg[3] = std::sqrt(x * (1.0f - y));
                    if (&voice == newest)
                    {
                        vector_now_x_ = x;
                        vector_now_y_ = y;
                    }
                }

                for (int c = 0; c < CHAINS_PER_VOICE; ++c)
                {
                    const int base = CHAIN_DEST_BASE[c];
                    auto &chain = voice.getChain(c);
                    // ±12 semitones; fine ±1; the pitch bend (it was set on the chains and overwritten here every sample)
                    chain.setPitchMod(voice_dst_[base + OFF_PITCH] * 12.0f + voice_dst_[base + OFF_FINE] + pitch_bend_);
                    chain.setLevelMod(voice_dst_[base + OFF_LEVEL] + (c < 4 ? mixLevel[c] : 0.0f)); // ±1 direct
                    chain.setPanMod(voice_dst_[base + OFF_PAN] + (c < 4 ? mixPan[c] : 0.0f));       // ±1 direct
                    // ±1 = ±4 octaves; the velocity's own (its sensitivity) on top (it was set at the note and overwritten)
                    chain.setCutoffMod(voice_dst_[base + OFF_CUTOFF] + voice.getVelocity() * filter_velocity_sensitivity_);
                    chain.setResoMod(voice_dst_[base + OFF_RESO]);                                       // ±1 direct
                    chain.setFilterEnvMod(voice_dst_[base + OFF_FENV]);                                  // ±1 direct
                    chain.setFMIndexMod(voice_dst_[base + OFF_FM_INDEX]);                                // ±1 = ±10 on the index
                    chain.setVectorGain(c < 4 ? vg[c] : 1.0f);
                    // WT position: from the base (the chain keeps it), ±1 = the whole bank
                    if (chain.isWtLoaded() || chain.isGranular()) // a granular oscillator's position too
                        chain.setWtMorphMod(voice_dst_[WT_MORPH_BASE + c]);
                }
            }
        }

        void applyParametersToAllChains()
        {
            allocator_.configureChains([this](OscillatorChain &chain, int)
                                       {
                chain.setWaveType(wave_type_);
                chain.setCutoff(base_cutoff_);
                chain.setResonance(resonance_);
                chain.setFilterEnvAmount(filter_env_amount_);
                chain.setAmpAttack(attack_time_);
                chain.setAmpDecay(decay_time_);
                chain.setAmpSustain(sustain_level_);
                chain.setAmpRelease(release_time_); });
        }
    };

} // namespace synth
