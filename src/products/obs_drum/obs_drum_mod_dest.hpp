/**
 * @file obs_drum_mod_dest.hpp
 * @brief ObsDrum modulation destinations (drum machine)
 *
 * Per-pad destinations: level, pitch, decay, filter, FX.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>

namespace synth
{

    enum class ObsDrumDst : uint16_t
    {
        // Per-pad (16 pads × 6 params = IDs 0-95)
        PAD1_LEVEL = 0,
        PAD1_PITCH,
        PAD1_DECAY,
        PAD1_CUTOFF,
        PAD1_FX_MIX,
        PAD1_PAN,

        PAD2_LEVEL = 6,
        PAD2_PITCH,
        PAD2_DECAY,
        PAD2_CUTOFF,
        PAD2_FX_MIX,
        PAD2_PAN,

        // ... pads 3-16 follow same pattern at stride 6

        // Master (200+)
        MASTER_VOL = 200,
        MASTER_COMP,
        MASTER_FX_MIX,

        NUM_DESTINATIONS
    };

    constexpr int PARAMS_PER_PAD = 6;

} // namespace synth
