/**
 * @file obs_bass_mod_source.hpp
 * @brief ObsBass modulation sources (basic linear bass synth)
 *
 * Single-oscillator bass synth with minimal modulation.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>

namespace synth
{

    enum class ObsBassSrc : uint8_t
    {
        LFO1 = 0,   ///< Single LFO (bipolar -1 to +1)
        FILTER_ENV, ///< Filter envelope (unipolar 0 to 1)
        AMP_ENV,    ///< Amp envelope (unipolar 0 to 1)
        VELOCITY,   ///< Note velocity (0 to 1)
        MOD_WHEEL,  ///< CC1 (0 to 1)
        AFTERTOUCH, ///< Channel aftertouch (0 to 1)
        PITCH_BEND, ///< Pitch bend (bipolar -1 to +1)

        NUM_SOURCES
    };

    constexpr int NUM_OBS_BASS_SOURCES = static_cast<int>(ObsBassSrc::NUM_SOURCES);

    constexpr bool isBipolarSource(ObsBassSrc source)
    {
        switch (source)
        {
        case ObsBassSrc::LFO1:
        case ObsBassSrc::PITCH_BEND:
            return true;
        default:
            return false;
        }
    }

} // namespace synth
