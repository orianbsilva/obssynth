/**
 * @file obs_bass_mod_dest.hpp
 * @brief ObsBass modulation destinations (basic linear bass synth)
 *
 * Single oscillator chain: pitch, cutoff, resonance, level.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>

namespace synth
{

    enum class ObsBassDst : uint16_t
    {
        PITCH = 0, ///< Pitch (±24 semitones)
        CUTOFF,    ///< Filter cutoff
        RESO,      ///< Filter resonance
        LEVEL,     ///< Output level
        PAN,       ///< Pan
        LFO_RATE,  ///< LFO rate
        LFO_DEPTH, ///< LFO depth
        FX_MIX,    ///< Effect mix

        NUM_DESTINATIONS
    };

} // namespace synth
