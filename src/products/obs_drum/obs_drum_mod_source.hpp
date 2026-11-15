/**
 * @file obs_drum_mod_source.hpp
 * @brief ObsDrum modulation sources (drum machine)
 *
 * Minimal mod sources for a drum machine: velocity, random, a few CCs.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>

namespace synth
{

    enum class ObsDrumSrc : uint8_t
    {
        VELOCITY = 0, ///< Hit velocity (0 to 1)
        RANDOM,       ///< Random per-hit (0 to 1)
        ACCENT,       ///< Accent flag (0 or 1)
        MOD_WHEEL,    ///< CC1 (0 to 1)
        CC_A,         ///< Assignable CC
        CC_B,         ///< Assignable CC

        NUM_SOURCES
    };

    constexpr int NUM_OBS_DRUM_SOURCES = static_cast<int>(ObsDrumSrc::NUM_SOURCES);

    constexpr bool isBipolarSource(ObsDrumSrc /*source*/)
    {
        return false; // Drum sources are all unipolar
    }

} // namespace synth
