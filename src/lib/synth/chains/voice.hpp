/**
 * @file voice.hpp
 * @brief Polyphonic voice containing multiple oscillator chains
 *
 * A Voice represents one note being played. It contains N OscillatorChains
 * that all play the same note but with different timbres/settings.
 *
 * For ObsSynth: 1 chain per voice (simple synth)
 * For ObsSynth4D: 4 chains per voice (full synth)
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cassert>

#include "osc_chain.hpp"
#include <array>
#include <cstdint>

namespace synth
{

    /**
     * @brief Voice phase enumeration (not to be confused with PolyphonicOscillator's VoiceState struct)
     */
    enum class VoicePhase : uint8_t
    {
        IDLE,      ///< Voice is not playing
        ACTIVE,    ///< Voice is playing (attack/decay/sustain)
        RELEASING, ///< Voice is in release phase
        STEALING   ///< Voice is being stolen (quick fadeout)
    };

    /**
     * @brief A single polyphonic voice with N oscillator chains
     *
     * Template parameter N specifies the number of OscillatorChains:
     * - N=1 for simple synths (ObsSynth)
     * - N=4 for full synths (ObsSynth4D)
     *
     * Each voice handles one MIDI note with per-voice modulation.
     */
    template <int N = 4>
    class SynthVoice
    {
    public:
        static constexpr int NUM_CHAINS = N;

    private:
        // Oscillator chains
        std::array<OscillatorChain, N> chains_;

        // Note state
        int note_;         ///< MIDI note number (-1 if none)
        float velocity_;   ///< Note velocity (0 to 1)
        float aftertouch_; ///< Channel/poly aftertouch (0 to 1)
        VoicePhase state_; ///< Current voice phase

        // Voice management
        uint32_t age_;     ///< Increments each sample for voice stealing
        uint32_t note_id_; ///< Unique ID for this note event

        // Per-voice modulation (from ModMatrix)
        float velocity_mod_;   ///< Velocity-derived modulation
        float keytrack_mod_;   ///< Keytrack-derived modulation
        float aftertouch_mod_; ///< Aftertouch-derived modulation

        // Sample rate
        float sample_rate_;

        // The voice's own modulation: three mod envelopes (they were one set for the whole instrument,
        // restarted by every note: a chord moved as one), a random value drawn per note, and the drift
        static constexpr int MOD_ENVS = 3;
        ADSREnvelope mod_envs_[MOD_ENVS]{ADSREnvelope(1.0f), ADSREnvelope(1.0f), ADSREnvelope(1.0f)}; // rate: setSampleRate
        float mod_env_values_[MOD_ENVS]{};
        float random_ = 0.0f;
        float drift_cents_ = 3.0f;
        uint32_t rng_ = 0x9E3779B9u;
        int holds_ = 0;                // noteOns of this note not yet released
        bool pending_off_ = false;     // a noteOff to apply once the note has sounded (or once the steal is done)
        bool has_pending_ = false;     // a note waiting for the steal's fade
        int pending_note_ = -1;
        float pending_velocity_ = 0.0f;
        uint32_t pending_id_ = 0;

        float nextRandom() // 0..1, xorshift: no lock, no allocation
        {
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            return static_cast<float>(rng_ >> 8) / 16777216.0f;
        }

        // Stealing fadeout
        float steal_gain_;
        float steal_decrement_;
        static constexpr float STEAL_TIME_MS = 5.0f;

    public:
        /**
         * @brief Construct a new Voice
         * @param sample_rate Audio sample rate in Hz (MUST be > 0)
         */
        explicit SynthVoice(float sample_rate)
            : note_(-1), velocity_(0.0f), aftertouch_(0.0f), state_(VoicePhase::IDLE), age_(0), note_id_(0), velocity_mod_(0.0f), keytrack_mod_(0.0f), aftertouch_mod_(0.0f), sample_rate_(sample_rate), steal_gain_(1.0f), steal_decrement_(0.0f)
        {
            assert(sample_rate > 0.0f && "SynthVoice: sample_rate must be > 0");
            for (auto &chain : chains_)
            {
                chain.setSampleRate(sample_rate);
            }
        }

        /// @brief Default constructor for container use ONLY. setSampleRate() MUST be called before use.
        SynthVoice()
            : note_(-1), velocity_(0.0f), aftertouch_(0.0f), state_(VoicePhase::IDLE), age_(0), note_id_(0), velocity_mod_(0.0f), keytrack_mod_(0.0f), aftertouch_mod_(0.0f), sample_rate_(0.0f), steal_gain_(1.0f), steal_decrement_(0.0f)
        {
        }

        // ============ Voice Control ============

        /**
         * @brief Trigger note on
         * @param note MIDI note number (0-127)
         * @param velocity Note velocity (0 to 1)
         * @param note_id Unique identifier for this note event
         */
        void noteOn(int note, float velocity, uint32_t note_id = 0)
        {
            // the same note again while it sounds: one more hold (it releases at its last noteOff)
            holds_ = (state_ == VoicePhase::ACTIVE && note == note_) ? holds_ + 1 : 1;
            pending_off_ = false;
            has_pending_ = false;
            aftertouch_mod_ = 0.0f;
            note_ = note;
            velocity_ = velocity;
            aftertouch_ = 0.0f;
            state_ = VoicePhase::ACTIVE;
            age_ = 0;
            note_id_ = note_id;
            steal_gain_ = 1.0f;

            // Calculate per-voice modulation
            velocity_mod_ = velocity;
            keytrack_mod_ = static_cast<float>(note) / 127.0f;
            random_ = nextRandom();

            // Trigger all chains with the same note, each a few cents off its own way
            for (auto &chain : chains_)
            {
                chain.setDrift((nextRandom() * 2.0f - 1.0f) * drift_cents_ / 100.0f);
                chain.noteOn(note, velocity);
            }
            for (auto &e : mod_envs_)
                e.noteOn();
        }

        /**
         * @brief Trigger note off
         */
        void noteOff()
        {
            if (state_ == VoicePhase::STEALING && has_pending_) // the note waiting for the fade: released as it starts
            {
                pending_off_ = true;
                return;
            }
            if (state_ == VoicePhase::ACTIVE && --holds_ > 0)
                return;
            // a note shorter than a block (its on and off applied together) would end before a sample: it sounds
            // a little first
            if (state_ == VoicePhase::ACTIVE && static_cast<float>(age_) < 0.002f * sample_rate_)
            {
                pending_off_ = true;
                return;
            }
            release();
        }

        // all notes off: every hold released, a note waiting for a steal dropped
        void releaseNow()
        {
            holds_ = 0;
            has_pending_ = false;
            release();
        }

        // the note's release, now
        void release()
        {
            pending_off_ = false;
            if (state_ == VoicePhase::ACTIVE)
            {
                state_ = VoicePhase::RELEASING;

                for (auto &chain : chains_)
                {
                    chain.noteOff();
                }
                for (auto &e : mod_envs_)
                    e.noteOff();
            }
        }

        /**
         * @brief Force stop voice immediately (for panic)
         */
        void forceStop()
        {
            state_ = VoicePhase::IDLE;
            note_ = -1;
            velocity_ = 0.0f;
            steal_gain_ = 1.0f;

            for (auto &chain : chains_)
            {
                chain.reset();
            }
            for (auto &e : mod_envs_)
                e.reset();
        }

        // ============ The voice's modulation ============
        void setModEnvADSR(int i, float a, float d, float s, float r)
        {
            if (i >= 0 && i < MOD_ENVS)
                mod_envs_[i].setADSR(a, d, s, r);
        }
        /// The mod envelopes' own rate: the engine advances them once per control step (setControlRate)
        void setModEnvRate(float rate)
        {
            for (auto &e : mod_envs_)
                e.setSampleRate(rate);
        }
        void advanceModEnvs() // once per control step (every sample unless the engine has a control rate)
        {
            for (int i = 0; i < MOD_ENVS; ++i)
                mod_env_values_[i] = mod_envs_[i].getNextSample();
        }
        float modEnvValue(int i) const { return mod_env_values_[i]; }
        float randomValue() const { return random_; }
        void setDriftCents(float cents) { drift_cents_ = std::max(0.0f, cents); }
        // each voice its own random sequence, the same at every run (it was seeded by the voice's address: no two
        // renders alike, nothing to compare a change against)
        void setSeed(uint32_t seed) { rng_ = 0x9E3779B9u ^ (seed * 2654435761u) ^ 0x5bd1e995u; }

        /**
         * @brief Initiate voice stealing (quick fadeout)
         */
        // taken for another note: what plays fades out in STEAL_TIME_MS, then the new note starts (it started at
        // once, the fade never ran: every steal clicked). The voice answers to the new note meanwhile.
        void steal(int note, float velocity, uint32_t note_id)
        {
            if (state_ == VoicePhase::IDLE)
            {
                noteOn(note, velocity, note_id);
                return;
            }
            if (state_ != VoicePhase::STEALING)
            {
                state_ = VoicePhase::STEALING;
                steal_gain_ = 1.0f;
                steal_decrement_ = 1.0f / std::max(1.0f, (STEAL_TIME_MS / 1000.0f) * sample_rate_);
            }
            has_pending_ = true;
            pending_off_ = false;
            pending_note_ = note;
            pending_velocity_ = velocity;
            pending_id_ = note_id;
            note_ = note;
            note_id_ = note_id;
        }

        // how loud the voice is now (its loudest oscillator's envelope): the quietest releasing voice goes first
        float level() const
        {
            float m = 0.0f;
            for (const auto &c : chains_)
                m = std::max(m, c.ampEnvLevel());
            return m;
        }

        // ============ Per-Voice Modulation ============

        /**
         * @brief Set aftertouch value (poly or channel)
         * @param value Aftertouch (0 to 1)
         */
        void setAftertouch(float value)
        {
            aftertouch_ = std::clamp(value, 0.0f, 1.0f);
            aftertouch_mod_ = aftertouch_;
        }

        /**
         * @brief Apply pitch modulation to all chains
         * @param semitones Pitch offset in semitones
         */
        void setPitchMod(float semitones)
        {
            for (auto &chain : chains_)
            {
                chain.setPitchMod(semitones);
            }
        }

        /**
         * @brief Apply pitch modulation to a specific chain
         * @param idx Chain index (0 to N-1)
         * @param semitones Pitch offset in semitones
         */
        void setPitchMod(int idx, float semitones)
        {
            chains_[std::clamp(idx, 0, N - 1)].setPitchMod(semitones);
        }

        void setPitchMod(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setPitchMod(values[i]);
        }

        /**
         * @brief Set coarse tuning for all chains
         * @param semitones Tuning offset in semitones
         */
        void setCoarseTune(float semitones)
        {
            for (auto &chain : chains_)
            {
                chain.setCoarseTune(semitones);
            }
        }

        /**
         * @brief Set coarse tuning for a specific chain
         * @param idx Chain index (0 to N-1)
         * @param semitones Tuning offset in semitones
         */
        void setCoarseTune(int idx, float semitones)
        {
            chains_[std::clamp(idx, 0, N - 1)].setCoarseTune(semitones);
        }

        void setCoarseTune(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setCoarseTune(values[i]);
        }

        /**
         * @brief Apply cutoff modulation to all chains
         * @param mod Cutoff modulation amount
         */
        void setCutoffMod(float mod)
        {
            for (auto &chain : chains_)
            {
                chain.setCutoffMod(mod);
            }
        }

        /**
         * @brief Apply cutoff modulation to a specific chain
         * @param idx Chain index (0 to N-1)
         * @param mod Cutoff modulation amount
         */
        void setCutoffMod(int idx, float mod)
        {
            chains_[std::clamp(idx, 0, N - 1)].setCutoffMod(mod);
        }

        void setCutoffMod(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setCutoffMod(values[i]);
        }

        /**
         * @brief Apply level modulation to all chains
         * @param mod Level modulation (-1 to +1)
         */
        void setLevelMod(float mod)
        {
            for (auto &chain : chains_)
            {
                chain.setLevelMod(mod);
            }
        }

        /**
         * @brief Apply level modulation to a specific chain
         * @param idx Chain index (0 to N-1)
         * @param mod Level modulation (-1 to +1)
         */
        void setLevelMod(int idx, float mod)
        {
            chains_[std::clamp(idx, 0, N - 1)].setLevelMod(mod);
        }

        void setLevelMod(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setLevelMod(values[i]);
        }

        // ============ Chain Configuration (array overloads) ============

        void setWaveType(const WaveType (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setWaveType(values[i]);
        }

        void setWaveType(int idx, WaveType wt)
        {
            chains_[std::clamp(idx, 0, N - 1)].setWaveType(wt);
        }

        void setLevel(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setLevel(values[i]);
        }

        void setLevel(int idx, float v)
        {
            chains_[std::clamp(idx, 0, N - 1)].setLevel(v);
        }

        void setPan(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setPan(values[i]);
        }

        void setPan(int idx, float v)
        {
            chains_[std::clamp(idx, 0, N - 1)].setPan(v);
        }

        void setCutoff(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setCutoff(values[i]);
        }

        void setCutoff(int idx, float v)
        {
            chains_[std::clamp(idx, 0, N - 1)].setCutoff(v);
        }

        void setResonance(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setResonance(values[i]);
        }

        void setResonance(int idx, float v)
        {
            chains_[std::clamp(idx, 0, N - 1)].setResonance(v);
        }

        void setFilterEnvAmount(const float (&values)[N])
        {
            for (int i = 0; i < N; ++i)
                chains_[i].setFilterEnvAmount(values[i]);
        }

        void setFilterEnvAmount(int idx, float v)
        {
            chains_[std::clamp(idx, 0, N - 1)].setFilterEnvAmount(v);
        }

        void setAmpADSR(const float (&a)[N], const float (&d)[N], const float (&s)[N], const float (&r)[N])
        {
            for (int i = 0; i < N; ++i)
            {
                chains_[i].setAmpAttack(a[i]);
                chains_[i].setAmpDecay(d[i]);
                chains_[i].setAmpSustain(s[i]);
                chains_[i].setAmpRelease(r[i]);
            }
        }

        void setFilterADSR(const float (&a)[N], const float (&d)[N], const float (&s)[N], const float (&r)[N])
        {
            for (int i = 0; i < N; ++i)
            {
                chains_[i].setFilterAttack(a[i]);
                chains_[i].setFilterDecay(d[i]);
                chains_[i].setFilterSustain(s[i]);
                chains_[i].setFilterRelease(r[i]);
            }
        }

        // ============ Chain Access ============

        /**
         * @brief Get reference to a specific chain
         * @param index Chain index (0 to N-1)
         * @return Reference to chain
         */
        OscillatorChain &getChain(int index)
        {
            return chains_[std::clamp(index, 0, N - 1)];
        }

        const OscillatorChain &getChain(int index) const
        {
            return chains_[std::clamp(index, 0, N - 1)];
        }

        /**
         * @brief Get all chains array
         */
        std::array<OscillatorChain, N> &getChains() { return chains_; }
        const std::array<OscillatorChain, N> &getChains() const { return chains_; }

        // ============ Processing ============

        /**
         * @brief Process and get next sample (mono)
         * @return Mixed sample from all chains
         */
        float process()
        {
            if (age_ != UINT32_MAX)
                age_++;
            if (pending_off_ && state_ == VoicePhase::ACTIVE && static_cast<float>(age_) >= 0.002f * sample_rate_)
                release();

            // Handle stealing fadeout (as processStereo)
            if (state_ == VoicePhase::STEALING)
            {
                steal_gain_ -= steal_decrement_;
                if (steal_gain_ <= 0.0f)
                {
                    const bool off = pending_off_;
                    forceStop();
                    if (has_pending_)
                    {
                        noteOn(pending_note_, pending_velocity_, pending_id_);
                        pending_off_ = off;
                    }
                    return 0.0f;
                }
            }

            // Check if any chain is still active
            bool any_active = false;
            float mix = 0.0f;

            for (auto &chain : chains_)
            {
                if (chain.isActive())
                {
                    any_active = true;
                    mix += chain.process();
                }
            }

            // no division by the chain count: a sound of one oscillator lost 12 dB to the three silent ones.
            // The headroom is the allocator's (kVoiceGain).

            // Apply steal gain if stealing
            if (state_ == VoicePhase::STEALING)
            {
                mix *= steal_gain_;
            }

            // Update state if all chains finished
            if (!any_active && state_ == VoicePhase::RELEASING)
            {
                state_ = VoicePhase::IDLE;
                note_ = -1;
            }

            return mix;
        }

        /**
         * @brief Process and get stereo output
         * @param left Output left channel
         * @param right Output right channel
         */
        void processStereo(float &left, float &right)
        {
            // Increment age (it stops at the top: no wrap after half a day)
            if (age_ != UINT32_MAX)
                age_++;
            if (pending_off_ && state_ == VoicePhase::ACTIVE && static_cast<float>(age_) >= 0.002f * sample_rate_)
                release();

            // Handle stealing fadeout: at its end, the note that took the voice
            if (state_ == VoicePhase::STEALING)
            {
                steal_gain_ -= steal_decrement_;
                if (steal_gain_ <= 0.0f)
                {
                    const bool off = pending_off_;
                    forceStop();
                    if (has_pending_)
                    {
                        noteOn(pending_note_, pending_velocity_, pending_id_);
                        pending_off_ = off; // released after its first 2 ms if its noteOff came during the fade
                    }
                    left = 0.0f;
                    right = 0.0f;
                    return;
                }
            }

            // Check if any chain is still active
            bool any_active = false;
            left = 0.0f;
            right = 0.0f;

            for (auto &chain : chains_)
            {
                if (chain.isActive())
                {
                    any_active = true;
                    float l, r;
                    chain.processStereo(l, r);
                    left += l;
                    right += r;
                }
            }

            // no division by the chain count (see process()): the headroom is the allocator's

            // Apply steal gain if stealing
            if (state_ == VoicePhase::STEALING)
            {
                left *= steal_gain_;
                right *= steal_gain_;
            }

            // Update state if all chains finished
            if (!any_active && state_ == VoicePhase::RELEASING)
            {
                state_ = VoicePhase::IDLE;
                note_ = -1;
            }
        }

        // ============ State Queries ============

        /**
         * @brief Check if voice is idle (available for new note)
         */
        bool isIdle() const { return state_ == VoicePhase::IDLE; }

        /**
         * @brief Check if voice is active (playing or releasing)
         */
        bool isActive() const { return state_ != VoicePhase::IDLE; }

        /**
         * @brief Check if voice is in release phase
         */
        bool isReleasing() const { return state_ == VoicePhase::RELEASING; }

        /**
         * @brief Check if voice is being stolen
         */
        bool isStealing() const { return state_ == VoicePhase::STEALING; }

        /**
         * @brief Get voice state
         */
        VoicePhase getState() const { return state_; }

        /**
         * @brief Get current note
         */
        int getNote() const { return note_; }

        /**
         * @brief Get note velocity
         */
        float getVelocity() const { return velocity_; }

        /**
         * @brief Get aftertouch value
         */
        float getAftertouch() const { return aftertouch_; }

        /**
         * @brief Get voice age (samples since noteOn)
         */
        uint32_t getAge() const { return age_; }

        /**
         * @brief Get note ID
         */
        uint32_t getNoteId() const { return note_id_; }

        // ============ Per-voice modulation getters ============

        float getVelocityMod() const { return velocity_mod_; }
        float getKeytrackMod() const { return keytrack_mod_; }
        float getAftertouchMod() const { return aftertouch_mod_; }

        // ============ Configuration ============

        /**
         * @brief Set sample rate for all chains
         */
        void setSampleRate(float sample_rate)
        {
            assert(sample_rate > 0.0f && "SynthVoice::setSampleRate: sample_rate must be > 0");
            sample_rate_ = sample_rate;
            for (auto &chain : chains_)
            {
                chain.setSampleRate(sample_rate);
            }
            for (auto &e : mod_envs_)
                e.setSampleRate(sample_rate);
            // Recalculate steal decrement
            float steal_samples = (STEAL_TIME_MS / 1000.0f) * sample_rate_;
            steal_decrement_ = 1.0f / steal_samples;
        }
    };

    // ============ Type aliases for common configurations ============

    using SynthVoice1 = SynthVoice<1>; ///< Simple voice with 1 chain (ObsSynth)
    using SynthVoice4 = SynthVoice<4>; ///< Full voice with 4 chains (ObsSynth4D)

} // namespace synth
