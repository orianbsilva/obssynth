/**
 * @file osc_chain.hpp
 * @brief Oscillator chain implementation
 *
 * Encapsulates a complete oscillator signal path:
 * Oscillator → Filter → FilterEnv → AmpEnv → FX Slot
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>
#include <cassert>
#include <cstring>
#include "../core/biquad_filter.hpp"
#include "../core/svf_filter.hpp"
#include "../core/grain_engine.hpp"
#include "../core/sampler.hpp"
#include "../core/super_engine.hpp"
#include "../core/effect_slot.hpp"
#include "../../oscillators/oscillator.hpp"
#include "../../oscillators/adsr_envelope.hpp"
#include "../../oscillators/wave_table_osc.hpp"
#include "../../math/synt_math.hpp"

namespace synth
{
    /**
     * @brief Type of synthesis engine for a single oscillator slot.
     *
     * STANDARD_OSC - existing oscillator (Sine/Saw/PCM/…)
     * WAVETABLE    - WaveTableOsc with morph position
     * FM_ENGINE    - existing Oscillator with WaveType::FM_SYNTHESIS
     * SUPER        - seven voices of the oscillator's wave, detuned around the centre (SuperEngine)
     */
    enum class PartType : uint8_t
    {
        STANDARD_OSC = 0,
        WAVETABLE = 1,
        FM_ENGINE = 2,
        GRANULAR = 3, // grains of a recording (GrainEngine)
        SAMPLE = 4,   // a recording played at the note's pitch (SamplePlayer)
        SUPER = 5     // seven detuned voices of the wave (SuperEngine): DETUNE on the position, SIDE on the spray
    };

    /**
     * @brief Complete oscillator signal chain
     *
     * Combines oscillator, filter, envelopes, and effect slot into
     * a single processing unit with modulation support.
     */
    class OscillatorChain
    {
    private:
        // Core components
        Oscillator oscillator_;
        SvfFilter filter_;    // Main filter (LPF/HPF/BPF/Notch): a saturating state-variable filter
        BiquadFilter hpf_;    // Serial high-pass filter (always HIGHPASS)
        ADSREnvelope filter_env_;
        ADSREnvelope amp_env_;
        EffectSlot effect_slot_;

        // PartType: determines which generator feeds the chain
        PartType part_type_ = PartType::STANDARD_OSC;
        WaveType prev_wave_type_ = WaveType::SINE; // saved when switching to FM/WT
        std::unique_ptr<WaveTableOsc> wt_;         // non-null only when WAVETABLE

        // Parameters
        float sample_rate_;
        float base_frequency_;
        float base_cutoff_;
        float hpf_cutoff_;       // HPF cutoff (20 Hz = effectively off)
        float key_track_amount_; // 0 = no tracking, 1 = full 1:1 (semitone = Hz factor)
        float filter_env_amount_;
        float level_;
        float pan_;

        // Tuning
        float coarse_tune_; // semitones offset (e.g. -12 = one octave down)

        // Modulation inputs (set externally)
        float pitch_mod_;
        float cutoff_mod_;
        float level_mod_;
        float pan_mod_;
        // the level and the pan gains as played: ramped to their targets over ramp_n_ samples (the engine's control
        // step; 1: at once, as always). snap_: the next targets are taken at once (a note's start)
        int ramp_n_ = 1;
        int filter_count_ = 0;      // the filter's place in the control step
        bool skip_silent_ = false;  // a small machine's profile: an oscillator at level 0 is not rendered
        bool snap_level_ = true, snap_pan_ = true;
        float level_target_ = -1.0f, level_now_ = 0.0f, level_step_ = 0.0f;
        int level_left_ = 0;
        float pan_target_ = -1.0f, gain_l_target_ = 0.0f, gain_r_target_ = 0.0f;
        float gain_l_ = 0.0f, gain_r_ = 0.0f, gain_l_step_ = 0.0f, gain_r_step_ = 0.0f;
        int pan_left_ = 0;
        // what the matrix adds to resonance, filter env amount and the wavetable's position: each kept apart
        // from its base, so the modulation never builds on itself
        float resonance_ = 0.0f;
        float reso_mod_ = 0.0f;
        float fenv_mod_ = 0.0f;
        float wt_morph_base_ = 0.0f;
        float fm_index_base_ = 1.0f;
        float fm_index_mod_ = 0.0f;
        float key_factor_ = 1.0f; // the key tracking's cutoff factor for the note playing
        float drift_ = 0.0f;      // this note's own small detune, semitones (the voice draws it)
        float vector_gain_ = 1.0f; // this oscillator's share of the vector mix (1: vector off)
        GrainEngine grain_;        // the granular engine (PartType::GRANULAR)
        SamplePlayer sampler_;     // the sample player (PartType::SAMPLE)
        SuperEngine super_;        // the seven voices (PartType::SUPER)
        float current_freq_ = 440.0f;

        // State
        bool active_;
        int note_;
        float velocity_;

    public:
        /**
         * @brief Construct a new OscillatorChain
         * @param sample_rate Audio sample rate in Hz (MUST be > 0)
         */
        explicit OscillatorChain(float sample_rate)
            : oscillator_(440.0f, WaveType::SINE, sample_rate),
              filter_(sample_rate, BiquadType::LOWPASS, 10000.0f, 0.0f),
              hpf_(sample_rate, BiquadType::HIGHPASS, 20.0f, 0.0f),
              filter_env_(sample_rate), amp_env_(sample_rate),
              effect_slot_(sample_rate), sample_rate_(sample_rate),
              base_frequency_(440.0f), base_cutoff_(10000.0f),
              hpf_cutoff_(20.0f), key_track_amount_(0.0f),
              filter_env_amount_(0.0f), level_(1.0f), pan_(0.5f),
              coarse_tune_(0.0f), pitch_mod_(0.0f), cutoff_mod_(0.0f),
              level_mod_(0.0f), pan_mod_(0.0f), active_(false), note_(-1), velocity_(1.0f)
        {
            assert(sample_rate > 0.0f && "OscillatorChain: sample_rate must be > 0");
            // Set default envelope times
            amp_env_.setAttackTime(0.01f);
            amp_env_.setDecayTime(0.1f);
            amp_env_.setSustainLevel(0.7f);
            amp_env_.setReleaseTime(0.3f);

            filter_env_.setAttackTime(0.01f);
            filter_env_.setDecayTime(0.2f);
            filter_env_.setSustainLevel(0.5f);
            filter_env_.setReleaseTime(0.3f);
        }

        /// @brief Default constructor for container use ONLY. setSampleRate() MUST be called before use.
        OscillatorChain()
            : oscillator_(440.0f, WaveType::SINE, 1.0f),
              filter_(1.0f, BiquadType::LOWPASS, 10000.0f, 0.0f),
              hpf_(1.0f, BiquadType::HIGHPASS, 20.0f, 0.0f),
              filter_env_(1.0f), amp_env_(1.0f),
              effect_slot_(1.0f), sample_rate_(0.0f),
              base_frequency_(440.0f), base_cutoff_(10000.0f),
              hpf_cutoff_(20.0f), key_track_amount_(0.0f),
              filter_env_amount_(0.0f), level_(1.0f), pan_(0.5f),
              coarse_tune_(0.0f), pitch_mod_(0.0f), cutoff_mod_(0.0f),
              level_mod_(0.0f), pan_mod_(0.0f), active_(false), note_(-1), velocity_(1.0f)
        {
        }

        // ============ Parameter setters ============

        /**
         * @brief Set sample rate for all components
         * @param sample_rate Sample rate in Hz
         * @note Oscillator sample rate is set at construction, other components updated here
         */
        void setSampleRate(float sample_rate)
        {
            assert(sample_rate > 0.0f && "OscillatorChain::setSampleRate: sample_rate must be > 0");
            sample_rate_ = sample_rate;
            oscillator_.setSampleRate(sample_rate);
            filter_.setSampleRate(sample_rate);
            hpf_.setSampleRate(sample_rate);
            effect_slot_.setSampleRate(sample_rate);
            // the envelopes too: made by the default constructor at 1 Hz, every time they were given was taken
            // as that many samples, so attacks, decays and releases were instant
            filter_env_.setSampleRate(sample_rate);
            amp_env_.setSampleRate(sample_rate);
            grain_.setSampleRate(sample_rate);
            sampler_.setSampleRate(sample_rate);
            super_.setSampleRate(sample_rate);
        }

        /**
         * @brief Set oscillator waveform
         * @param wave_type Waveform type
         */
        void setWaveType(WaveType wave_type)
        {
            // the plain oscillator's wave: kept, and played only while the part is that oscillator (it replaced the
            // FM generator under an FM part: a preset loaded over FM played a plain wave)
            prev_wave_type_ = wave_type;
            super_.setWave(wave_type); // SUPER's shape
            if (part_type_ == PartType::STANDARD_OSC)
                oscillator_.setWaveType(wave_type);
        }

        /**
         * @brief Set base frequency
         * @param frequency Frequency in Hz
         */
        void setFrequency(float frequency)
        {
            base_frequency_ = frequency;
            updateOscillatorFrequency();
        }

        /**
         * @brief Set filter type
         * @param type Filter type
         */
        void setFilterType(BiquadType type)
        {
            filter_.setType(type);
        }

        /**
         * @brief Set filter cutoff frequency
         * @param cutoff Cutoff in Hz
         */
        void setCutoff(float cutoff)
        {
            base_cutoff_ = cutoff;
            updateFilterCutoff();
        }

        /**
         * @brief Set filter resonance
         * @param resonance Resonance (0 to 1)
         */
        void setResonance(float resonance)
        {
            resonance_ = resonance;
            filter_.setResonance(std::clamp(resonance_ + reso_mod_, 0.0f, 1.0f));
        }

        /**
         * @brief Set filter envelope amount
         * @param amount Amount (-1 to +1)
         */
        void setFilterEnvAmount(float amount)
        {
            filter_env_amount_ = std::clamp(amount, -1.0f, 1.0f);
        }

        /**
         * @brief Set HPF cutoff frequency (20 Hz = effectively bypassed)
         * @param cutoff Cutoff in Hz (20..2000)
         */
        void setHPFCutoff(float cutoff)
        {
            hpf_cutoff_ = std::clamp(cutoff, 20.0f, 2000.0f);
            hpf_.setCutoff(hpf_cutoff_);
        }

        /**
         * @brief Set keyboard tracking amount for filter cutoff
         * @param amount 0 = none, 1 = full (1 octave per octave)
         */
        void setKeyTrackAmount(float amount)
        {
            key_track_amount_ = std::clamp(amount, 0.0f, 1.0f);
        }

        /**
         * @brief Set output level
         * @param level Level (0 to 1)
         */
        void setLevel(float level)
        {
            level_ = std::clamp(level, 0.0f, 1.0f);
        }

        /**
         * @brief Set pan position
         * @param pan Pan (0 = left, 0.5 = center, 1 = right)
         */
        void setPan(float pan)
        {
            pan_ = std::clamp(pan, 0.0f, 1.0f);
        }

        /**
         * @brief Set coarse tuning offset
         * @param semitones Tuning offset in semitones (-24 to +24)
         */
        void setCoarseTune(float semitones)
        {
            coarse_tune_ = semitones;
            updateOscillatorFrequency();
        }

        // ============ Envelope setters ============

        void setAmpAttack(float seconds) { amp_env_.setAttackTime(seconds); }
        void setAmpDecay(float seconds) { amp_env_.setDecayTime(seconds); }
        void setAmpSustain(float level) { amp_env_.setSustainLevel(level); }
        void setAmpRelease(float seconds) { amp_env_.setReleaseTime(seconds); }

        void setFilterAttack(float seconds) { filter_env_.setAttackTime(seconds); }
        void setFilterDecay(float seconds) { filter_env_.setDecayTime(seconds); }
        void setFilterSustain(float level) { filter_env_.setSustainLevel(level); }
        void setFilterRelease(float seconds) { filter_env_.setReleaseTime(seconds); }

        // ============ Modulation inputs ============

        /**
         * @brief Set pitch modulation (in semitones)
         * @param semitones Pitch offset in semitones
         */
        void setPitchMod(float semitones)
        {
            pitch_mod_ = semitones;
            updateOscillatorFrequency();
        }

        /**
         * @brief Set cutoff modulation
         * @param mod Cutoff modulation amount (multiplied by range)
         */
        void setCutoffMod(float mod)
        {
            cutoff_mod_ = mod;
            updateFilterCutoff();
        }

        /**
         * @brief Set level modulation
         * @param mod Level modulation (-1 to +1)
         */
        void setLevelMod(float mod)
        {
            level_mod_ = mod;
        }

        /**
         * @brief Set pan modulation
         * @param mod Pan modulation (-1 to +1)
         */
        void setPanMod(float mod)
        {
            pan_mod_ = mod;
        }

        /// @brief The modulation's step (the engine's control rate): the level and the pan glide over it
        void setModRamp(int samples) { ramp_n_ = std::max(1, samples); }
        /// @brief Do not render an oscillator at level 0 (its envelopes still run): a small machine's profile
        void setSkipSilent(bool on) { skip_silent_ = on; }

        /// @brief Resonance modulation, added to the resonance (0..1 after)
        void setResoMod(float mod)
        {
            if (mod == reso_mod_)
                return;
            reso_mod_ = mod;
            filter_.setResonance(std::clamp(resonance_ + reso_mod_, 0.0f, 1.0f));
        }

        /// @brief Filter envelope amount modulation, added to the amount (-1..1 after)
        void setFilterEnvMod(float mod) { fenv_mod_ = mod; }

        /// @brief A note's own detune (semitones), set before its noteOn: no two notes quite alike
        void setDrift(float semitones) { drift_ = semitones; }

        // ============ Granular ============
        // (the position and the spray are also SUPER's DETUNE and SIDE: the same knobs, the same matrix destination)
        void setGrainSource(std::shared_ptr<const GrainSource> s) { grain_.setSource(std::move(s)); }
        void setGrainPosition(float p)
        {
            grain_.setPosition(p);
            super_.setDetune(p);
        }
        void setGrainSize(float seconds) { grain_.setSize(seconds); }
        void setGrainDensity(float perSecond) { grain_.setDensity(perSecond); }
        void setGrainSpray(float s)
        {
            grain_.setSpray(s);
            super_.setSide(s);
        }
        bool isSuper() const { return part_type_ == PartType::SUPER; }
        bool isGranular() const { return part_type_ == PartType::GRANULAR; }
        void setSampleSource(std::shared_ptr<const GrainSource> s) { sampler_.setSource(std::move(s)); }
        void setSampleStart(float p) { sampler_.setStart(p); }
        void setSampleLoop(bool on) { sampler_.setLoop(on); }
        void setSampleRoot(float midiNote) { sampler_.setRoot(midiNote); }
        void setVectorGain(float g) { vector_gain_ = g; }

        /// @brief The envelopes where they are now: the matrix's per-voice sources
        float ampEnvLevel() const { return amp_env_.getCurrentAmplitude(); }
        float filterEnvLevel() const { return filter_env_.getCurrentAmplitude(); }

        /// @brief Wavetable position modulation: 1 moves through the whole bank, from the base position
        void setWtMorphMod(float mod)
        {
            grain_.setPositionMod(mod); // a granular oscillator's position takes the same destination
            super_.setDetuneMod(mod);   // and SUPER's DETUNE
            if (!wt_)
                return;
            const int tc = wt_->tableCount();
            const float last = static_cast<float>(tc > 1 ? tc - 1 : 1);
            wt_->setMorphPosition(std::clamp(wt_morph_base_ + mod * last, 0.0f, last));
        }

        // ============ Note control ============

        /**
         * @brief Trigger note on
         * @param note MIDI note number
         * @param velocity Note velocity (0 to 1)
         */
        void noteOn(int note, float velocity)
        {
            assert(sample_rate_ > 0.0f && "OscillatorChain::noteOn: sample_rate not set! Call setSampleRate() first.");
            note_ = note;
            velocity_ = velocity;
            active_ = true;

            // Set frequency from MIDI note
            base_frequency_ = midiNoteToFrequency(note);
            updateOscillatorFrequency();

            // Apply keyboard tracking: offset cutoff by (note-60) semitones * amount
            // MIDI 60 = C4 = reference note (no offset)
            // (it was set once here and overwritten by process() at the next sample: key tracking never worked)
            key_factor_ = std::exp2(static_cast<float>(note - 60) * key_track_amount_ / 12.0f);
            updateFilterCutoff();

            if (part_type_ == PartType::GRANULAR)
            {
                uint32_t seed;
                std::memcpy(&seed, &drift_, sizeof seed); // each note's drift is its own: so are its grains
                // from silence: the cloud starts over; a retrigger keeps the grains sounding (cutting them clicked)
                grain_.noteOn(seed ^ static_cast<uint32_t>(note) * 2654435761u, amp_env_.getCurrentAmplitude() < 1e-3f);
            }

            if (part_type_ == PartType::SAMPLE)
                sampler_.noteOn();

            if (part_type_ == PartType::SUPER && amp_env_.getCurrentAmplitude() < 1e-3f)
            {
                uint32_t seed; // from silence: the sides from phases of this note's own (a retrigger keeps them)
                std::memcpy(&seed, &drift_, sizeof seed);
                super_.noteOn(seed ^ static_cast<uint32_t>(note) * 2654435761u);
            }

            // Trigger envelopes
            // the phases start over from silence only: a retrigger at its level would click (looked at before the
            // envelope starts: an attack of 0 jumps it to 1 at once)
            const bool silent = amp_env_.getCurrentAmplitude() < 1e-3f;
            if (silent) // from silence the level, the pan and the filter start where they are meant (no glide from the last note's)
            {
                snap_level_ = snap_pan_ = true;
                filter_count_ = 0;
            }
            amp_env_.noteOn();
            filter_env_.noteOn();
            if (silent)
            {
                oscillator_.resetPhase();
                if (part_type_ == PartType::FM_ENGINE)
                    oscillator_.resetFMModulatorPhase();
            }
        }

        /**
         * @brief Trigger note off
         */
        void noteOff()
        {
            amp_env_.noteOff();
            filter_env_.noteOff();
        }

        /**
         * @brief Check if chain is currently active
         * @return true if note is playing or releasing
         */
        bool isActive() const
        {
            return active_ && amp_env_.getState() != EnvelopeState::IDLE;
        }

        /**
         * @brief Get current note
         * @return MIDI note number or -1 if none
         */
        int getNote() const { return note_; }

        /**
         * @brief Get velocity
         * @return Note velocity (0 to 1)
         */
        float getVelocity() const { return velocity_; }

        // ============ Effect slot access ============

        EffectSlot &getEffectSlot() { return effect_slot_; }
        const EffectSlot &getEffectSlot() const { return effect_slot_; }

        // ============ Processing ============

        /**
         * @brief Process and get next sample (mono)
         * @return Processed sample
         */
        float process()
        {
            if (!isActive())
            {
                active_ = false;
                return 0.0f;
            }

            // an oscillator at level 0 (and staying there) is not rendered: its envelopes run, nothing else
            // (what it played would be multiplied by 0); it comes back through the level's glide, without a click
            if (skip_silent_ && level_now_ == 0.0f && level_left_ == 0 &&
                std::clamp(level_ + level_mod_, 0.0f, 1.0f) * vector_gain_ == 0.0f)
            {
                filter_env_.getNextSample();
                amp_env_.getNextSample();
                level_target_ = 0.0f;
                snap_level_ = false;
                return effect_slot_.process(0.0f);
            }

            // Get oscillator sample — dispatch by PartType
            float sample;
            if (part_type_ == PartType::WAVETABLE && wt_ && wt_->isLoaded())
            {
                // Advance the oscillator phase then read from wavetable
                oscillator_.advancePhase(); // (it ran the whole wave generator, FM included, to throw the sample away)
                float phase = oscillator_.getPhase() / math::TWO_PI;
                phase -= static_cast<float>(static_cast<int>(phase)); // wrap [0,1)
                if (phase < 0.0f)
                    phase += 1.0f;
                sample = wt_->getSample(phase);
            }
            else if (part_type_ == PartType::GRANULAR)
                sample = grain_.process(current_freq_);
            else if (part_type_ == PartType::SAMPLE)
                sample = sampler_.process(current_freq_);
            else if (part_type_ == PartType::SUPER)
                sample = super_.process(current_freq_);
            else
            {
                sample = oscillator_.getSample();
            }

            // Update filter cutoff with envelope
            float filter_env_value = filter_env_.getNextSample();
            // in octaves: the matrix ±4 per unit, the envelope ±6 at full amount (they were Hz added: huge with
            // the filter closed, nothing with it open)
            const float octaves = cutoff_mod_ * 4.0f + filter_env_value * std::clamp(filter_env_amount_ + fenv_mod_, -1.0f, 1.0f) * 6.0f;
            if (ramp_n_ <= 1)
                filter_.setCutoff(std::clamp(base_cutoff_ * key_factor_ * std::exp2(octaves), 20.0f, 20000.0f));
            else
            {
                // at a control rate: the cutoff (exp2, tan, a division) once per step, the coefficients gliding
                if (filter_count_ == 0)
                    filter_.setCutoffRamp(std::clamp(base_cutoff_ * key_factor_ * std::exp2(octaves), 20.0f, 20000.0f), ramp_n_);
                if (++filter_count_ >= ramp_n_)
                    filter_count_ = 0;
            }

            // Apply filter
            sample = filter_.process(sample);

            // Apply serial HPF (skipped when cutoff is at minimum 20 Hz)
            if (hpf_cutoff_ > 21.0f)
                sample = hpf_.process(sample);

            // Apply amplitude envelope
            float amp_env_value = amp_env_.getNextSample();
            sample *= amp_env_value;

            // Apply velocity
            sample *= velocity_;

            // Apply level with modulation (ramped over the engine's control step: no steps, no clicks)
            const float final_level = std::clamp(level_ + level_mod_, 0.0f, 1.0f) * vector_gain_;
            if (final_level != level_target_)
            {
                level_target_ = final_level;
                level_left_ = snap_level_ ? 0 : ramp_n_;
                level_step_ = (final_level - level_now_) / static_cast<float>(ramp_n_);
                if (level_left_ == 0)
                    level_now_ = final_level;
            }
            if (level_left_ > 0 && --level_left_ > 0)
                level_now_ += level_step_;
            else
                level_now_ = level_target_;
            snap_level_ = false;
            sample *= level_now_;

            // Apply effect slot
            sample = effect_slot_.process(sample);

            return sample;
        }

        /**
         * @brief Process and get stereo output
         * @param left Output left channel
         * @param right Output right channel
         */
        void processStereo(float &left, float &right)
        {
            float mono = process();

            // Apply pan with modulation: constant power, the gains computed when the pan moves (not every sample)
            // and ramped over the control step like the level
            const float final_pan = std::clamp(pan_ + pan_mod_ * 0.5f, 0.0f, 1.0f);
            if (final_pan != pan_target_)
            {
                pan_target_ = final_pan;
                gain_l_target_ = math::cosTurns(final_pan * 0.25f); // a quarter turn across the field
                gain_r_target_ = math::sinTurns(final_pan * 0.25f);
                pan_left_ = snap_pan_ ? 0 : ramp_n_;
                gain_l_step_ = (gain_l_target_ - gain_l_) / static_cast<float>(ramp_n_);
                gain_r_step_ = (gain_r_target_ - gain_r_) / static_cast<float>(ramp_n_);
            }
            if (pan_left_ > 0 && --pan_left_ > 0)
            {
                gain_l_ += gain_l_step_;
                gain_r_ += gain_r_step_;
            }
            else
            {
                gain_l_ = gain_l_target_;
                gain_r_ = gain_r_target_;
            }
            snap_pan_ = false;
            left = mono * gain_l_;
            right = mono * gain_r_;
        }

        /**
         * @brief Reset chain state
         */
        void reset()
        {
            oscillator_.resetPhase();
            filter_.reset();
            hpf_.reset();
            effect_slot_.reset();
            amp_env_.reset(); // (they were left at their level: the next note started without its attack)
            filter_env_.reset();
            active_ = false;
            note_ = -1;
        }

        // ============ Getters ============

        float getFrequency() const { return base_frequency_; }
        float getCutoff() const { return base_cutoff_; }
        float getResonance() const { return filter_.getResonance(); }
        float getHPFCutoff() const { return hpf_cutoff_; }
        float getKeyTrackAmount() const { return key_track_amount_; }
        float getLevel() const { return level_; }
        float getPan() const { return pan_; }
        float getCoarseTune() const { return coarse_tune_; }
        float getFilterEnvAmount() const { return filter_env_amount_; }
        WaveType getWaveType() const { return oscillator_.getWaveType(); }
        BiquadType getFilterType() const { return filter_.getType(); }

        void loadPCMWaveform(const std::string &path)
        {
            oscillator_.load_custom_waveform(path);
        }

        // ============ PartType control ============

        PartType getPartType() const { return part_type_; }

        /**
         * @brief Switch the synthesis engine for this slot.
         *
         * WAVETABLE: allocates WaveTableOsc (bank must be loaded separately).
         * FM_ENGINE: configures oscillator_ with FM_SYNTHESIS + TWO_OP_FEEDBACK.
         * STANDARD_OSC: restores the previous WaveType.
         */
        void setPartType(PartType pt)
        {
            if (pt == part_type_)
                return;
            part_type_ = pt;
            switch (pt)
            {
            case PartType::WAVETABLE:
                if (!wt_)
                    wt_ = std::make_unique<WaveTableOsc>();
                break;
            case PartType::FM_ENGINE: // the algorithm as it was set (it was reset to 2-op feedback at every switch)
                oscillator_.setWaveType(WaveType::FM_SYNTHESIS);
                break;
            case PartType::STANDARD_OSC:
                oscillator_.setWaveType(prev_wave_type_);
                break;
            case PartType::GRANULAR: // the grains read the source; the oscillator only keeps the pitch
            case PartType::SAMPLE:   // the player reads its source likewise
            case PartType::SUPER:    // the seven voices keep their own phases
                break;
            }
        }

        // ============ WaveTable control ============

        bool loadWtBank(const std::string &path, size_t samplesPerWave = 0)
        {
            if (!wt_)
                wt_ = std::make_unique<WaveTableOsc>();
            return wt_->loadBank(path, samplesPerWave);
        }

        void setWtMorphPosition(float pos)
        {
            wt_morph_base_ = pos;
            if (wt_)
                wt_->setMorphPosition(pos);
        }

        float getWtMorphPosition() const
        {
            return wt_ ? wt_->getMorphPosition() : 0.0f;
        }

        int getWtTableCount() const
        {
            return wt_ ? wt_->tableCount() : 0;
        }

        bool isWtLoaded() const
        {
            return wt_ && wt_->isLoaded();
        }

        const std::string &getWtFilename() const
        {
            static const std::string empty;
            return wt_ ? wt_->getFilename() : empty;
        }

        /** Raw PCMWaveform pointer for UI visualisation — null if WT not loaded. */
        const PCMWaveform *getWtWaveform() const
        {
            return (wt_ && wt_->isLoaded()) ? wt_->getWaveform() : nullptr;
        }

        // ============ FM control (delegates to oscillator_) ============

        void setFMAlgorithm(FMAlgorithm algo) { oscillator_.setFMAlgorithm(algo); }
        void setFMModulatorRatio(float r) { oscillator_.setFMModulatorRatio(r); }
        void setFMModulationIndex(float idx)
        {
            fm_index_base_ = idx;
            oscillator_.setFMModulationIndex(std::max(0.0f, fm_index_base_ + fm_index_mod_ * 10.0f));
        }
        /// @brief FM index modulation: ±1 = ±10 on the index, from its base
        void setFMIndexMod(float mod)
        {
            if (mod == fm_index_mod_)
                return;
            fm_index_mod_ = mod;
            oscillator_.setFMModulationIndex(std::max(0.0f, fm_index_base_ + fm_index_mod_ * 10.0f));
        }
        void setFMFeedback(float fb) { oscillator_.setFMFeedback(fb); }
        void setFMOperatorRatio(int op, float r) { oscillator_.setFMOperatorRatio(op, r); }
        void setFMOperatorLevel(int op, float l) { oscillator_.setFMOperatorLevel(op, l); }

        FMAlgorithm getFMAlgorithm() const { return oscillator_.getFMAlgorithm(); }
        float getFMModulatorRatio() const { return oscillator_.getFMModulatorRatio(); }
        float getFMModulationIndex() const { return oscillator_.getFMModulationIndex(); }
        float getFMFeedback() const { return oscillator_.getFMFeedback(); }
        float getFMOperatorRatio(int op) const { return oscillator_.getFMOperatorRatio(op); }
        float getFMOperatorLevel(int op) const { return oscillator_.getFMOperatorLevel(op); }

        // Amp envelope getters
        float getAmpAttack() const { return amp_env_.getAttackTime(); }
        float getAmpDecay() const { return amp_env_.getDecayTime(); }
        float getAmpSustain() const { return amp_env_.getSustainLevel(); }
        float getAmpRelease() const { return amp_env_.getReleaseTime(); }

        // Filter envelope getters
        float getFilterAttack() const { return filter_env_.getAttackTime(); }
        float getFilterDecay() const { return filter_env_.getDecayTime(); }
        float getFilterSustain() const { return filter_env_.getSustainLevel(); }
        float getFilterRelease() const { return filter_env_.getReleaseTime(); }

        /**
         * @brief Lookup table: MIDI note 0-127 → exact frequency in Hz (public static)
         *        Pre-computed: A4 (note 69) = 440 Hz, 12-TET equal temperament.
         *        Eliminates powf() drift on every noteOn.
         */
        static constexpr float kMidiNoteFreqTable[128] = {
            //   C       C#      D       D#      E       F       F#      G       G#      A       A#      B
            8.17580f,
            8.66196f,
            9.17702f,
            9.72272f,
            10.30086f,
            10.91338f,
            11.56233f,
            12.24986f,
            12.97828f,
            13.75000f,
            14.56762f,
            15.43385f, // Oct -1
            16.35160f,
            17.32391f,
            18.35405f,
            19.44544f,
            20.60172f,
            21.82676f,
            23.12465f,
            24.49971f,
            25.95655f,
            27.50000f,
            29.13524f,
            30.86771f, // Oct 0
            32.70320f,
            34.64783f,
            36.70810f,
            38.89087f,
            41.20344f,
            43.65353f,
            46.24930f,
            48.99943f,
            51.91310f,
            55.00000f,
            58.27047f,
            61.73541f, // Oct 1
            65.40639f,
            69.29566f,
            73.41619f,
            77.78175f,
            82.40689f,
            87.30706f,
            92.49861f,
            97.99886f,
            103.82620f,
            110.00000f,
            116.54094f,
            123.47083f, // Oct 2
            130.81278f,
            138.59132f,
            146.83238f,
            155.56349f,
            164.81378f,
            174.61412f,
            184.99721f,
            195.99772f,
            207.65235f,
            220.00000f,
            233.08188f,
            246.94165f, // Oct 3
            261.62557f,
            277.18263f,
            293.66477f,
            311.12698f,
            329.62756f,
            349.22823f,
            369.99442f,
            391.99544f,
            415.30470f,
            440.00000f,
            466.16376f,
            493.88330f, // Oct 4
            523.25113f,
            554.36526f,
            587.32954f,
            622.25397f,
            659.25511f,
            698.45646f,
            739.98884f,
            783.99088f,
            830.60940f,
            880.00000f,
            932.32752f,
            987.76660f, // Oct 5
            1046.50226f,
            1108.73052f,
            1174.65907f,
            1244.50793f,
            1318.51023f,
            1396.91293f,
            1479.97769f,
            1567.98177f,
            1661.21879f,
            1760.00000f,
            1864.65505f,
            1975.53321f, // Oct 6
            2093.00452f,
            2217.46105f,
            2349.31814f,
            2489.01587f,
            2637.02046f,
            2793.82585f,
            2959.95538f,
            3135.96355f,
            3322.43758f,
            3520.00000f,
            3729.31009f,
            3951.06641f, // Oct 7
            4186.00904f,
            4434.92210f,
            4698.63629f,
            4978.03174f,
            5274.04092f,
            5587.65170f,
            5919.91076f,
            6271.92710f,
            6644.87516f,
            7040.00000f,
            7458.62018f,
            7902.13282f, // Oct 8
            8372.01808f,
            8869.84420f,
            9397.27257f,
            9956.06348f,
            10548.08184f,
            11175.30341f,
            11839.82153f,
            12543.85420f, // Oct 9 (C8-G8)
        };

        static float midiNoteToFrequency(int note)
        {
            if (note >= 0 && note < 128)
                return kMidiNoteFreqTable[note];
            return 440.0f * std::pow(2.0f, (note - 69) / 12.0f); // out-of-range fallback
        }

    private:
        /**
         * @brief Update oscillator frequency with coarse tune + pitch modulation
         */
        void updateOscillatorFrequency()
        {
            // Apply coarse tune + pitch modulation (both in semitones)
            float freq = base_frequency_ * std::exp2((coarse_tune_ + pitch_mod_ + drift_) / 12.0f);
            current_freq_ = freq;
            oscillator_.setFrequency(freq);
        }

        /**
         * @brief Update filter cutoff with modulation
         */
        void updateFilterCutoff()
        {
            filter_.setCutoff(std::clamp(base_cutoff_ * key_factor_ * std::exp2(cutoff_mod_ * 4.0f), 20.0f, 20000.0f));
        }
    };

} // namespace synth
