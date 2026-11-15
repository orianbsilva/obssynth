/**
 * @file obs_synth_mod_source.hpp
 * @brief ObsSynth modulation sources (4-oscillator polyphonic synthesizer)
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>

namespace synth
{

    /**
     * @brief ObsSynth modulation source types
     *
     * Bipolar sources output -1.0 to +1.0
     * Unipolar sources output 0.0 to 1.0
     */
    enum class ObsSynthSrc : uint8_t
    {
        // LFOs (Global, bipolar -1 to +1)
        LFO1 = 0,
        LFO2,
        LFO3,
        LFO4,

        // Filter Envelopes (Per-voice, unipolar 0 to 1)
        FILTER_ENV1,
        FILTER_ENV2,
        FILTER_ENV3,
        FILTER_ENV4,

        // Amplitude Envelopes (Per-voice, unipolar 0 to 1)
        AMP_ENV1,
        AMP_ENV2,
        AMP_ENV3,
        AMP_ENV4,

        // Modulation Envelope (Per-voice, unipolar 0 to 1)
        MOD_ENV,
        MOD_ENV2,
        MOD_ENV3,

        // XY Pad (Global, unipolar 0 to 1)
        XY_X,
        XY_Y,

        // MIDI Controllers
        VELOCITY,   ///< Note velocity (per-note, 0 to 1)
        AFTERTOUCH, ///< Channel aftertouch (per-note, 0 to 1)
        MOD_WHEEL,  ///< CC1 (global, 0 to 1)
        PITCH_BEND, ///< Pitch bend (global, -1 to +1)
        EXPRESSION, ///< CC11 (global, 0 to 1)

        // Note-based
        KEYTRACK, ///< Key position (per-note, 0 to 1 across keyboard)
        RANDOM,   ///< Random per-note (0 to 1)

        // Custom CCs
        CC_A, ///< Assignable CC (default: CC16)
        CC_B, ///< Assignable CC (default: CC17)
        CC_C, ///< Assignable CC (default: CC18)
        CC_D, ///< Assignable CC (default: CC19)

        NUM_SOURCES
    };

    constexpr int NUM_OBS_SYNTH_SOURCES = static_cast<int>(ObsSynthSrc::NUM_SOURCES);

    constexpr bool isPerVoiceSource(ObsSynthSrc source)
    {
        switch (source)
        {
        case ObsSynthSrc::FILTER_ENV1:
        case ObsSynthSrc::FILTER_ENV2:
        case ObsSynthSrc::FILTER_ENV3:
        case ObsSynthSrc::FILTER_ENV4:
        case ObsSynthSrc::AMP_ENV1:
        case ObsSynthSrc::AMP_ENV2:
        case ObsSynthSrc::AMP_ENV3:
        case ObsSynthSrc::AMP_ENV4:
        case ObsSynthSrc::MOD_ENV:
        case ObsSynthSrc::MOD_ENV2:
        case ObsSynthSrc::MOD_ENV3:
        case ObsSynthSrc::VELOCITY:
        case ObsSynthSrc::AFTERTOUCH:
        case ObsSynthSrc::KEYTRACK:
        case ObsSynthSrc::RANDOM:
            return true;
        default:
            return false;
        }
    }

    constexpr bool isBipolarSource(ObsSynthSrc source)
    {
        switch (source)
        {
        case ObsSynthSrc::LFO1:
        case ObsSynthSrc::LFO2:
        case ObsSynthSrc::LFO3:
        case ObsSynthSrc::LFO4:
        case ObsSynthSrc::PITCH_BEND:
            return true;
        default:
            return false;
        }
    }

    inline const char *obsSynthSrcToString(ObsSynthSrc source)
    {
        switch (source)
        {
        case ObsSynthSrc::LFO1:
            return "LFO 1";
        case ObsSynthSrc::LFO2:
            return "LFO 2";
        case ObsSynthSrc::LFO3:
            return "LFO 3";
        case ObsSynthSrc::LFO4:
            return "LFO 4";
        case ObsSynthSrc::FILTER_ENV1:
            return "Filter Env 1";
        case ObsSynthSrc::FILTER_ENV2:
            return "Filter Env 2";
        case ObsSynthSrc::FILTER_ENV3:
            return "Filter Env 3";
        case ObsSynthSrc::FILTER_ENV4:
            return "Filter Env 4";
        case ObsSynthSrc::AMP_ENV1:
            return "Amp Env 1";
        case ObsSynthSrc::AMP_ENV2:
            return "Amp Env 2";
        case ObsSynthSrc::AMP_ENV3:
            return "Amp Env 3";
        case ObsSynthSrc::AMP_ENV4:
            return "Amp Env 4";
        case ObsSynthSrc::MOD_ENV:
            return "Mod Env";
        case ObsSynthSrc::MOD_ENV2:
            return "Mod Env 2";
        case ObsSynthSrc::MOD_ENV3:
            return "Mod Env 3";
        case ObsSynthSrc::XY_X:
            return "XY X";
        case ObsSynthSrc::XY_Y:
            return "XY Y";
        case ObsSynthSrc::VELOCITY:
            return "Velocity";
        case ObsSynthSrc::AFTERTOUCH:
            return "Aftertouch";
        case ObsSynthSrc::MOD_WHEEL:
            return "Mod Wheel";
        case ObsSynthSrc::PITCH_BEND:
            return "Pitch Bend";
        case ObsSynthSrc::EXPRESSION:
            return "Expression";
        case ObsSynthSrc::KEYTRACK:
            return "Key Track";
        case ObsSynthSrc::RANDOM:
            return "Random";
        case ObsSynthSrc::CC_A:
            return "CC A";
        case ObsSynthSrc::CC_B:
            return "CC B";
        case ObsSynthSrc::CC_C:
            return "CC C";
        case ObsSynthSrc::CC_D:
            return "CC D";
        default:
            return "Unknown";
        }
    }

} // namespace synth
