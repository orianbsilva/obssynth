/**
 * @file effect_slot.hpp
 * @brief Effect slot interface for modular effect chains
 *
 * Provides abstract interface for effect slots that can be
 * dynamically assigned different effect types at runtime.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cassert>
#include <memory>
#include <string>

namespace synth
{

    /**
     * @brief Effect types available for slots
     */
    enum class EffectType
    {
        NONE,       ///< Bypass (no effect)
        CHORUS,     ///< Chorus effect
        REVERB,     ///< Reverb effect
        DELAY,      ///< Delay effect
        OVERDRIVE,  ///< Overdrive/distortion
        FILTER,     ///< Filter effect
        PHASER,     ///< Phaser effect
        FLANGER,    ///< Flanger effect
        TREMOLO,    ///< Tremolo effect
        BITCRUSH,   ///< Bitcrusher effect
        TUBE_REVERB ///< Physical model tube reverb (digital waveguide)
    };

    /**
     * @brief Abstract base class for all effects
     *
     * All effect implementations should derive from this class
     * to be usable in effect slots.
     */
    class IEffect
    {
    public:
        virtual ~IEffect() = default;

        /**
         * @brief Process a single sample
         * @param input Input sample
         * @return Processed output sample
         */
        virtual float process(float input) = 0;

        /**
         * @brief Process a buffer of samples in-place
         * @param buffer Input/output buffer
         * @param num_samples Number of samples
         */
        virtual void processBuffer(float *buffer, int num_samples)
        {
            for (int i = 0; i < num_samples; ++i)
            {
                buffer[i] = process(buffer[i]);
            }
        }

        /**
         * @brief Reset effect state
         */
        virtual void reset() = 0;

        /**
         * @brief Set sample rate
         * @param sample_rate Sample rate in Hz
         */
        virtual void setSampleRate(float sample_rate) = 0;

        /**
         * @brief Get effect type
         * @return Effect type enum
         */
        virtual EffectType getType() const = 0;

        /**
         * @brief Get effect name
         * @return Human-readable effect name
         */
        virtual const char *getName() const = 0;

        /**
         * @brief Set parameter by index
         * @param index Parameter index (0-based)
         * @param value Normalized value (0.0 to 1.0)
         */
        virtual void setParameter(int index, float value) = 0;

        /**
         * @brief Get parameter by index
         * @param index Parameter index (0-based)
         * @return Normalized value (0.0 to 1.0)
         */
        virtual float getParameter(int index) const = 0;

        /**
         * @brief Get number of parameters
         * @return Number of adjustable parameters
         */
        virtual int getNumParameters() const = 0;

        /**
         * @brief Get parameter name
         * @param index Parameter index
         * @return Parameter name string
         */
        virtual const char *getParameterName(int index) const = 0;
    };

    /**
     * @brief Effect slot that can hold different effects
     *
     * A slot that can dynamically change its effect type at runtime.
     * Provides dry/wet mix control and bypass functionality.
     */
    class EffectSlot
    {
    private:
        std::unique_ptr<IEffect> effect_;
        float mix_;         ///< Dry/wet mix (0.0 = dry, 1.0 = wet)
        bool bypassed_;     ///< Bypass flag
        float sample_rate_; ///< Current sample rate

    public:
        /**
         * @brief Construct empty effect slot
         * @param sample_rate Initial sample rate
         */
        explicit EffectSlot(float sample_rate)
            : effect_(nullptr), mix_(1.0f), bypassed_(false), sample_rate_(sample_rate)
        {
            assert(sample_rate > 0.0f && "EffectSlot: sample_rate must be > 0");
        }

        /**
         * @brief Set the effect in this slot
         * @param effect Effect to use (takes ownership)
         */
        void setEffect(std::unique_ptr<IEffect> effect)
        {
            effect_ = std::move(effect);
            if (effect_)
            {
                effect_->setSampleRate(sample_rate_);
            }
        }

        /**
         * @brief Clear the effect (set to bypass)
         */
        void clearEffect()
        {
            effect_.reset();
        }

        /**
         * @brief Check if slot has an effect
         * @return true if effect is loaded
         */
        bool hasEffect() const { return effect_ != nullptr; }

        /**
         * @brief Get current effect type
         * @return Effect type or NONE if empty
         */
        EffectType getEffectType() const
        {
            return effect_ ? effect_->getType() : EffectType::NONE;
        }

        /**
         * @brief Set dry/wet mix
         * @param mix Mix value (0.0 = dry, 1.0 = wet)
         */
        void setMix(float mix)
        {
            mix_ = std::clamp(mix, 0.0f, 1.0f);
        }

        /**
         * @brief Get current mix value
         * @return Mix value (0.0 to 1.0)
         */
        float getMix() const { return mix_; }

        /**
         * @brief Set bypass state
         * @param bypass true to bypass effect
         */
        void setBypassed(bool bypass) { bypassed_ = bypass; }

        /**
         * @brief Check if bypassed
         * @return true if bypassed
         */
        bool isBypassed() const { return bypassed_; }

        /**
         * @brief Set sample rate
         * @param sample_rate Sample rate in Hz
         */
        void setSampleRate(float sample_rate)
        {
            sample_rate_ = sample_rate;
            if (effect_)
            {
                effect_->setSampleRate(sample_rate);
            }
        }

        /**
         * @brief Reset effect state
         */
        void reset()
        {
            if (effect_)
            {
                effect_->reset();
            }
        }

        /**
         * @brief Process a single sample
         * @param input Input sample
         * @return Processed sample with dry/wet mix applied
         */
        float process(float input)
        {
            if (bypassed_ || !effect_)
            {
                return input;
            }

            float wet = effect_->process(input);
            return input * (1.0f - mix_) + wet * mix_;
        }

        /**
         * @brief Process a buffer of samples
         * @param buffer Input/output buffer
         * @param num_samples Number of samples
         */
        void processBuffer(float *buffer, int num_samples)
        {
            if (bypassed_ || !effect_)
            {
                return; // Pass through unchanged
            }

            for (int i = 0; i < num_samples; ++i)
            {
                float dry = buffer[i];
                float wet = effect_->process(dry);
                buffer[i] = dry * (1.0f - mix_) + wet * mix_;
            }
        }

        /**
         * @brief Set effect parameter
         * @param index Parameter index
         * @param value Normalized value (0.0 to 1.0)
         */
        void setParameter(int index, float value)
        {
            if (effect_)
            {
                effect_->setParameter(index, value);
            }
        }

        /**
         * @brief Get effect parameter
         * @param index Parameter index
         * @return Normalized value or 0.0 if no effect
         */
        float getParameter(int index) const
        {
            return effect_ ? effect_->getParameter(index) : 0.0f;
        }

        /**
         * @brief Get pointer to effect (for advanced access)
         * @return Raw pointer to effect (may be null)
         */
        IEffect *getEffect() { return effect_.get(); }
        const IEffect *getEffect() const { return effect_.get(); }
    };

    /**
     * @brief Get effect type as string
     * @param type Effect type
     * @return String representation
     */
    inline const char *effectTypeToString(EffectType type)
    {
        switch (type)
        {
        case EffectType::NONE:
            return "None";
        case EffectType::CHORUS:
            return "Chorus";
        case EffectType::REVERB:
            return "Reverb";
        case EffectType::DELAY:
            return "Delay";
        case EffectType::OVERDRIVE:
            return "Overdrive";
        case EffectType::FILTER:
            return "Filter";
        case EffectType::PHASER:
            return "Phaser";
        case EffectType::FLANGER:
            return "Flanger";
        case EffectType::TREMOLO:
            return "Tremolo";
        case EffectType::BITCRUSH:
            return "Bitcrush";
        case EffectType::TUBE_REVERB:
            return "Tube Reverb";
        default:
            return "Unknown";
        }
    }

} // namespace synth
