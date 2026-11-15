/**
 * @file effect_factory.hpp
 * @brief Factory for creating IEffect instances from EffectType enum
 *
 * Wraps existing effect classes (Chorus, Overdrive) into IEffect adapters
 * so they can be used in EffectSlot.
 */

#pragma once

#include "effect_slot.hpp"
#include "../../effects/chorus.hpp"
#include "../../effects/overdrive.hpp"
#include "../../effects/waveshaper.hpp"
#include "../../effects/tube_reverb.hpp"
#include <memory>
#include <cmath>

namespace synth
{

    /**
     * @brief IEffect adapter for Chorus
     */
    class ChorusEffect : public IEffect
    {
    private:
        Chorus chorus_;

    public:
        explicit ChorusEffect(float sample_rate)
            : chorus_(sample_rate) {}

        float process(float input) override { return chorus_.process(input); }
        void reset() override { chorus_.clear(); }
        const char *getName() const override { return "Chorus"; }
        void setSampleRate(float sr) override
        {
            // Reconstruct in-place with new sample rate
            chorus_.~Chorus();
            new (&chorus_) Chorus(sr);
        }
        EffectType getType() const override { return EffectType::CHORUS; }

        void setParameter(int index, float value) override
        {
            switch (index)
            {
            case 0:
                chorus_.setMix(value);
                break;
            case 1:
                chorus_.setDepth(value);
                break;
            case 2:
                chorus_.setRate(0.1f + value * 9.9f);
                break;
            }
        }
        float getParameter(int index) const override
        {
            switch (index)
            {
            case 0:
                return chorus_.getMix();
            case 1:
                return chorus_.getDepth();
            case 2:
                return (chorus_.getRate() - 0.1f) / 9.9f;
            default:
                return 0.0f;
            }
        }
        int getNumParameters() const override { return 3; }
        const char *getParameterName(int index) const override
        {
            switch (index)
            {
            case 0:
                return "Mix";
            case 1:
                return "Depth";
            case 2:
                return "Rate";
            default:
                return "";
            }
        }
    };

    /**
     * @brief IEffect adapter for Overdrive
     */
    class OverdriveEffect : public IEffect
    {
    private:
        Overdrive od_;

    public:
        explicit OverdriveEffect(float /*sample_rate*/)
            : od_(2.0f, 0.5f, 1.0f) {}

        float process(float input) override
        {
            float out = 0.0f;
            od_.process(&input, &out, 1);
            return out;
        }
        void reset() override { od_.reset(); }
        const char *getName() const override { return "Overdrive"; }
        void setSampleRate(float /*sr*/) override {}
        EffectType getType() const override { return EffectType::OVERDRIVE; }

        void setParameter(int index, float value) override
        {
            switch (index)
            {
            case 0:
                od_.set_drive(1.0f + value * 9.0f);
                break;
            case 1:
                od_.set_output_level(value);
                break;
            case 2:
                od_.set_mix(value);
                break;
            }
        }
        float getParameter(int index) const override
        {
            switch (index)
            {
            case 0:
                return (od_.get_drive() - 1.0f) / 9.0f;
            case 1:
                return od_.get_output_level();
            case 2:
                return od_.get_mix();
            default:
                return 0.0f;
            }
        }
        int getNumParameters() const override { return 3; }
        const char *getParameterName(int index) const override
        {
            switch (index)
            {
            case 0:
                return "Drive";
            case 1:
                return "Output";
            case 2:
                return "Mix";
            default:
                return "";
            }
        }
    };

    /**
     * @brief IEffect adapter for TubeReverb (physical model 1D waveguide)
     *
     * P1 = tube_length [0.3–6.0 m], P2 = reflection [0.5–0.99], P3 = mix [0.0–1.0]
     */
    class TubeReverbEffect : public IEffect
    {
    private:
        TubeReverb reverb_;

    public:
        explicit TubeReverbEffect(float sample_rate)
            : reverb_(sample_rate)
        {
            reverb_.setTubeLength(2.5f);
            reverb_.setReflection(0.92f);
            reverb_.setMix(0.5f);
        }

        float process(float input) override { return reverb_.process(input); }
        void reset() override { reverb_.clear(); }
        const char *getName() const override { return "Tube Reverb"; }
        void setSampleRate(float sr) override
        {
            float len = reverb_.getTubeLength();
            float refl = reverb_.getReflection();
            float mix = reverb_.getMix();
            reverb_ = TubeReverb(sr);
            reverb_.setTubeLength(len);
            reverb_.setReflection(refl);
            reverb_.setMix(mix);
        }
        EffectType getType() const override { return EffectType::TUBE_REVERB; }

        /// P1 = tube_length normalised 0→1 maps to 0.3–6.0 m
        void setParameter(int index, float value) override
        {
            switch (index)
            {
            case 0:
                reverb_.setTubeLength(0.3f + value * 5.7f);
                break; // P1
            case 1:
                reverb_.setReflection(0.5f + value * 0.49f);
                break; // P2
            case 2:
                reverb_.setMix(value);
                break; // P3
            }
        }
        float getParameter(int index) const override
        {
            switch (index)
            {
            case 0:
                return (reverb_.getTubeLength() - 0.3f) / 5.7f;
            case 1:
                return (reverb_.getReflection() - 0.5f) / 0.49f;
            case 2:
                return reverb_.getMix();
            default:
                return 0.0f;
            }
        }
        int getNumParameters() const override { return 3; }
        const char *getParameterName(int index) const override
        {
            switch (index)
            {
            case 0:
                return "Length"; // P1
            case 1:
                return "Reflection"; // P2
            case 2:
                return "Mix"; // P3
            default:
                return "";
            }
        }
    };

    /**
     * @brief Create an IEffect instance from EffectType
     * @param type The effect type to create
     * @param sample_rate Audio sample rate
     * @return Unique pointer to new effect, or nullptr for NONE
     */
    inline std::unique_ptr<IEffect> createEffect(EffectType type, float sample_rate)
    {
        switch (type)
        {
        case EffectType::CHORUS:
            return std::make_unique<ChorusEffect>(sample_rate);
        case EffectType::OVERDRIVE:
            return std::make_unique<OverdriveEffect>(sample_rate);
        case EffectType::TUBE_REVERB:
            return std::make_unique<TubeReverbEffect>(sample_rate);
        default:
            return nullptr;
        }
    }

    /**
     * @brief Get the next effect type in cycle order
     * @param current Current effect type
     * @return Next effect type (cycles: NONE → CHORUS → OVERDRIVE → TUBE_REVERB → NONE)
     */
    inline EffectType nextEffectType(EffectType current)
    {
        switch (current)
        {
        case EffectType::NONE:
            return EffectType::CHORUS;
        case EffectType::CHORUS:
            return EffectType::OVERDRIVE;
        case EffectType::OVERDRIVE:
            return EffectType::TUBE_REVERB;
        case EffectType::TUBE_REVERB:
            return EffectType::NONE;
        default:
            return EffectType::NONE;
        }
    }

    /**
     * @brief Get the next tube model in cycle order
     * @param current Current tube model
     * @return Next tube model
     */
    inline TubeModel nextTubeModel(TubeModel current)
    {
        switch (current)
        {
        case TubeModel::AX7:
            return TubeModel::AT7;
        case TubeModel::AT7:
            return TubeModel::AU7;
        case TubeModel::AU7:
            return TubeModel::EL34;
        case TubeModel::EL34:
            return TubeModel::P6L6;
        case TubeModel::P6L6:
            return TubeModel::AX7;
        default:
            return TubeModel::AX7;
        }
    }

} // namespace synth
