/**
 * @file voice_allocator.hpp
 * @brief Voice allocation and management for polyphonic synthesizers
 *
 * Manages a pool of voices with configurable allocation strategies:
 * - Round-robin: Cycle through voices in order
 * - Oldest: Steal the oldest playing voice
 * - Quietest: Steal the voice with lowest amplitude (future)
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cassert>
#include "voice.hpp"
#include <array>
#include <algorithm>
#include <cstdint>

namespace synth
{

    /**
     * @brief Voice allocation strategy
     */
    enum class VoiceAllocationStrategy : uint8_t
    {
        ROUND_ROBIN, ///< Cycle through voices in order
        OLDEST,      ///< Steal the oldest playing voice
        LOWEST_NOTE, ///< Steal the lowest note (bass priority high)
        HIGHEST_NOTE ///< Steal the highest note (melody priority high)
    };

    /**
     * @brief Voice allocator managing a pool of polyphonic voices
     *
     * Template parameters:
     * - VoiceType: The Voice class to use (Voice<1> or Voice<4>)
     * - MAX_VOICES: Maximum number of simultaneous voices
     */
    template <typename VoiceType, int MAX_VOICES = 32>
    class VoiceAllocator
    {
    private:
        // Voice pool
        std::array<VoiceType, MAX_VOICES> voices_;

        // Allocation state
        VoiceAllocationStrategy strategy_;
        int round_robin_index_;
        int voice_limit_ = MAX_VOICES; // the voices played: setVoiceLimit
        uint32_t next_note_id_;

        // Sample rate
        float sample_rate_;

    public:
        static constexpr int NUM_VOICES = MAX_VOICES;

        /// The polyphony actually played, 1..MAX_VOICES (a Pi 2 cannot render 32 voices of four oscillators):
        /// notes past it steal, as they would past MAX_VOICES. Voices already playing past it finish.
        void setVoiceLimit(int n)
        {
            voice_limit_ = std::clamp(n, 1, MAX_VOICES);
            round_robin_index_ %= voice_limit_;
        }
        int getVoiceLimit() const { return voice_limit_; }

        /**
         * @brief Construct a new VoiceAllocator
         * @param sample_rate Audio sample rate in Hz
         * @param strategy Voice allocation strategy
         */
        explicit VoiceAllocator(float sample_rate,
                                VoiceAllocationStrategy strategy = VoiceAllocationStrategy::OLDEST)
            : strategy_(strategy), round_robin_index_(0), next_note_id_(1), sample_rate_(sample_rate)
        {
            assert(sample_rate > 0.0f && "VoiceAllocator: sample_rate must be > 0");
            uint32_t seed = 1;
            for (auto &voice : voices_)
            {
                voice.setSampleRate(sample_rate);
                voice.setSeed(seed++);
            }
        }

        // ============ Note Control ============

        /**
         * @brief Trigger a note on
         * @param note MIDI note number (0-127)
         * @param velocity Note velocity (0 to 1)
         * @return Pointer to the allocated voice, or nullptr if failed
         */
        VoiceType *noteOn(int note, float velocity)
        {
            // First, check if this note is already playing and retrigger it
            VoiceType *existing = findVoiceByNote(note);
            if (existing)
            {
                // Retrigger the same voice
                existing->noteOn(note, velocity, next_note_id_++);
                return existing;
            }

            // Find a free voice
            VoiceType *voice = findFreeVoice();

            // If no free voice, steal one based on strategy
            if (!voice)
            {
                voice = stealVoice();
                if (voice) // it fades out first, then plays the note
                    voice->steal(note, velocity, next_note_id_++);
                return voice;
            }

            voice->noteOn(note, velocity, next_note_id_++);
            return voice;
        }

        /**
         * @brief Trigger a note off
         * @param note MIDI note number
         */
        void noteOff(int note)
        {
            for (auto &voice : voices_)
            {
                if (voice.getNote() == note && voice.isActive() && !voice.isReleasing())
                {
                    voice.noteOff();
                    break; // Only release one voice per note
                }
            }
        }

        /**
         * @brief Release all notes (all notes off)
         */
        void allNotesOff()
        {
            for (auto &voice : voices_)
            {
                if (voice.isActive() && !voice.isReleasing())
                    voice.releaseNow(); // every hold, no wait: all notes off means off
            }
        }

        /**
         * @brief Force stop all voices immediately (panic)
         */
        void panic()
        {
            for (auto &voice : voices_)
            {
                voice.forceStop();
            }
            round_robin_index_ = 0;
        }

        /**
         * @brief Set aftertouch for a specific note (poly aftertouch)
         * @param note MIDI note number
         * @param value Aftertouch value (0 to 1)
         */
        void setPolyAftertouch(int note, float value)
        {
            for (auto &voice : voices_)
            {
                if (voice.getNote() == note && voice.isActive())
                {
                    voice.setAftertouch(value);
                }
            }
        }

        /**
         * @brief Set channel aftertouch (affects all voices)
         * @param value Aftertouch value (0 to 1)
         */
        void setChannelAftertouch(float value)
        {
            for (auto &voice : voices_)
            {
                if (voice.isActive())
                {
                    voice.setAftertouch(value);
                }
            }
        }

        /**
         * @brief Set pitch bend for all voices
         * @param semitones Pitch bend in semitones
         */
        void setPitchBend(float semitones)
        {
            for (auto &voice : voices_)
            {
                if (voice.isActive())
                {
                    voice.setPitchMod(semitones);
                }
            }
        }

        // ============ Processing ============

        // The voices' sum: a fixed gain. It used to be 1/sqrt(voices playing), a gain that moved with every note
        // (each note in or out changed the others' level at once); the peaks of a full chord are the output
        // limiter's (ObsSynth4D), not the voices'.
        static constexpr float kVoiceGain = 0.6f;

        /**
         * @brief Process all voices and mix to mono output
         * @return Mixed sample
         */
        float process()
        {
            float mix = 0.0f;
            int active_count = 0;

            for (auto &voice : voices_)
            {
                if (voice.isActive())
                {
                    mix += voice.process();
                    active_count++;
                }
            }

            (void)active_count;
            return mix * kVoiceGain;
        }

        /**
         * @brief Process all voices and mix to stereo output
         * @param left Output left channel
         * @param right Output right channel
         */
        void processStereo(float &left, float &right)
        {
            left = 0.0f;
            right = 0.0f;
            int active_count = 0;

            for (auto &voice : voices_)
            {
                if (voice.isActive())
                {
                    float l, r;
                    voice.processStereo(l, r);
                    left += l;
                    right += r;
                    active_count++;
                }
            }

            (void)active_count;
            left *= kVoiceGain;
            right *= kVoiceGain;
        }

        /**
         * @brief Generate a buffer of mono samples
         * @param buffer Output buffer
         * @param size Number of samples to generate
         */
        void generateBuffer(float *buffer, int size)
        {
            if (!buffer || size <= 0)
                return;

            for (int i = 0; i < size; ++i)
            {
                buffer[i] = process();
            }
        }

        /**
         * @brief Generate a buffer of stereo samples (interleaved L,R,L,R...)
         * @param buffer Output buffer
         * @param frames Number of frames to generate
         */
        void generateStereoBuffer(float *buffer, int frames)
        {
            if (!buffer || frames <= 0)
                return;

            for (int i = 0; i < frames; ++i)
            {
                float left, right;
                processStereo(left, right);
                buffer[i * 2] = left;
                buffer[i * 2 + 1] = right;
            }
        }

        // ============ Voice Access ============

        /**
         * @brief Get reference to a specific voice
         * @param index Voice index (0 to MAX_VOICES-1)
         * @return Reference to voice
         */
        VoiceType &getVoice(int index)
        {
            return voices_[std::clamp(index, 0, MAX_VOICES - 1)];
        }

        const VoiceType &getVoice(int index) const
        {
            return voices_[std::clamp(index, 0, MAX_VOICES - 1)];
        }

        /**
         * @brief Find voice by note
         * @param note MIDI note number
         * @return Pointer to voice playing the note, or nullptr
         */
        VoiceType *findVoiceByNote(int note)
        {
            for (auto &voice : voices_)
            {
                if (voice.getNote() == note && voice.isActive() && !voice.isStealing())
                {
                    return &voice;
                }
            }
            return nullptr;
        }

        // ============ State Queries ============

        /**
         * @brief Get count of active voices
         */
        int getActiveVoiceCount() const
        {
            int count = 0;
            for (const auto &voice : voices_)
            {
                if (voice.isActive())
                {
                    count++;
                }
            }
            return count;
        }

        /**
         * @brief Get count of releasing voices
         */
        int getReleasingVoiceCount() const
        {
            int count = 0;
            for (const auto &voice : voices_)
            {
                if (voice.isReleasing())
                {
                    count++;
                }
            }
            return count;
        }

        /**
         * @brief Check if any voice is active
         */
        bool isActive() const
        {
            for (const auto &voice : voices_)
            {
                if (voice.isActive())
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Check if a specific note is playing
         * @param note MIDI note number
         */
        bool isNotePlaying(int note) const
        {
            for (const auto &voice : voices_)
            {
                if (voice.getNote() == note && voice.isActive())
                {
                    return true;
                }
            }
            return false;
        }

        // ============ Configuration ============

        /**
         * @brief Set voice allocation strategy
         */
        void setStrategy(VoiceAllocationStrategy strategy)
        {
            strategy_ = strategy;
        }

        VoiceAllocationStrategy getStrategy() const { return strategy_; }

        /**
         * @brief Set sample rate for all voices
         */
        void setSampleRate(float sample_rate)
        {
            sample_rate_ = sample_rate;
            for (auto &voice : voices_)
            {
                voice.setSampleRate(sample_rate);
            }
        }

        /**
         * @brief Configure all voices with the same settings
         * @param configure Lambda/function to apply to each voice
         */
        template <typename ConfigFunc>
        void configureVoices(ConfigFunc configure)
        {
            for (auto &voice : voices_)
            {
                configure(voice);
            }
        }

        /**
         * @brief Configure all chains in all voices with the same settings
         * @param configure Lambda/function to apply to each chain
         */
        template <typename ConfigFunc>
        void configureChains(ConfigFunc configure)
        {
            for (auto &voice : voices_)
            {
                for (int i = 0; i < VoiceType::NUM_CHAINS; ++i)
                {
                    configure(voice.getChain(i), i);
                }
            }
        }

    private:
        /**
         * @brief Find a free (idle) voice
         * @return Pointer to free voice, or nullptr if none available
         */
        VoiceType *findFreeVoice()
        {
            // First pass: find completely idle voice
            // only the first voice_limit_ voices are played (a small machine's polyphony); the rest stay idle
            for (int v = 0; v < voice_limit_; ++v)
            {
                if (voices_[v].isIdle())
                {
                    return &voices_[v];
                }
            }

            // Second pass: a releasing voice, the quietest (it was the first in the array)
            VoiceType *quietest = nullptr;
            for (int v = 0; v < voice_limit_; ++v)
                if (voices_[v].isReleasing() && (!quietest || voices_[v].level() < quietest->level()))
                    quietest = &voices_[v];
            return quietest;
        }

        /**
         * @brief Steal a voice based on current strategy
         * @return Pointer to stolen voice
         */
        VoiceType *stealVoice()
        {
            VoiceType *victim = nullptr;

            switch (strategy_)
            {
            case VoiceAllocationStrategy::ROUND_ROBIN:
                victim = &voices_[round_robin_index_];
                round_robin_index_ = (round_robin_index_ + 1) % voice_limit_;
                break;

            case VoiceAllocationStrategy::OLDEST:
                victim = findOldestVoice();
                break;

            case VoiceAllocationStrategy::LOWEST_NOTE:
                victim = findLowestNoteVoice();
                break;

            case VoiceAllocationStrategy::HIGHEST_NOTE:
                victim = findHighestNoteVoice();
                break;
            }

            return victim; // the caller steals it for its note
        }

        /**
         * @brief Find the oldest playing voice
         */
        VoiceType *findOldestVoice()
        {
            // by note order (the ids count up), not by age: notes of one block all have age 0, and the first
            // voice was taken every time. A voice already being stolen is not taken again while others play.
            VoiceType *oldest = nullptr;
            for (auto &voice : voices_)
                if (voice.isActive() && !voice.isStealing() && (!oldest || voice.getNoteId() - oldest->getNoteId() > 0x80000000u))
                    oldest = &voice;
            return oldest ? oldest : &voices_[0];
        }

        /**
         * @brief Find the voice playing the lowest note
         */
        VoiceType *findLowestNoteVoice()
        {
            VoiceType *lowest = nullptr;
            int min_note = 128;

            for (auto &voice : voices_)
            {
                if (voice.isActive() && voice.getNote() < min_note)
                {
                    min_note = voice.getNote();
                    lowest = &voice;
                }
            }

            return lowest ? lowest : &voices_[0];
        }

        /**
         * @brief Find the voice playing the highest note
         */
        VoiceType *findHighestNoteVoice()
        {
            VoiceType *highest = nullptr;
            int max_note = -1;

            for (auto &voice : voices_)
            {
                if (voice.isActive() && voice.getNote() > max_note)
                {
                    max_note = voice.getNote();
                    highest = &voice;
                }
            }

            return highest ? highest : &voices_[0];
        }
    };

    // ============ Type aliases for common configurations ============

    /// Simple synth allocator: 32 voices, 1 chain each
    using SimpleVoiceAllocator = VoiceAllocator<SynthVoice1, 32>;

    /// Full synth allocator: 32 voices, 4 chains each
    using FullVoiceAllocator = VoiceAllocator<SynthVoice4, 32>;

    /// Small synth allocator: 8 voices, 4 chains each (for CPU-limited systems)
    using SmallVoiceAllocator = VoiceAllocator<SynthVoice4, 8>;

} // namespace synth
