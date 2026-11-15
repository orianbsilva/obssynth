/**
 * @file obs_synth_mod_dest.hpp
 * @brief ObsSynth modulation destinations (4-oscillator polyphonic synthesizer)
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <cstdint>

namespace synth
{

    /**
     * @brief ObsSynth modulation destinations
     *
     * 4 oscillators × 17 params each, plus mixer, master FX, LFO rates.
     */
    enum class ObsSynthDst : uint16_t
    {
        // ============ Oscillator 1 (0-16) ============
        OSC1_PITCH = 0,
        OSC1_FINE,
        OSC1_WAVEPOS,
        OSC1_LEVEL,
        OSC1_PAN,
        OSC1_PWM,
        OSC1_FM_INDEX,
        OSC1_CUTOFF,
        OSC1_RESO,
        OSC1_FENV_AMT,
        OSC1_ATK,
        OSC1_DEC,
        OSC1_SUS,
        OSC1_REL,
        OSC1_FX_MIX,
        OSC1_FX_P1,
        OSC1_FX_P2,

        // ============ Oscillator 2 (17-33) ============
        OSC2_PITCH = 17,
        OSC2_FINE,
        OSC2_WAVEPOS,
        OSC2_LEVEL,
        OSC2_PAN,
        OSC2_PWM,
        OSC2_FM_INDEX,
        OSC2_CUTOFF,
        OSC2_RESO,
        OSC2_FENV_AMT,
        OSC2_ATK,
        OSC2_DEC,
        OSC2_SUS,
        OSC2_REL,
        OSC2_FX_MIX,
        OSC2_FX_P1,
        OSC2_FX_P2,

        // ============ Oscillator 3 (34-50) ============
        OSC3_PITCH = 34,
        OSC3_FINE,
        OSC3_WAVEPOS,
        OSC3_LEVEL,
        OSC3_PAN,
        OSC3_PWM,
        OSC3_FM_INDEX,
        OSC3_CUTOFF,
        OSC3_RESO,
        OSC3_FENV_AMT,
        OSC3_ATK,
        OSC3_DEC,
        OSC3_SUS,
        OSC3_REL,
        OSC3_FX_MIX,
        OSC3_FX_P1,
        OSC3_FX_P2,

        // ============ Oscillator 4 (51-67) ============
        OSC4_PITCH = 51,
        OSC4_FINE,
        OSC4_WAVEPOS,
        OSC4_LEVEL,
        OSC4_PAN,
        OSC4_PWM,
        OSC4_FM_INDEX,
        OSC4_CUTOFF,
        OSC4_RESO,
        OSC4_FENV_AMT,
        OSC4_ATK,
        OSC4_DEC,
        OSC4_SUS,
        OSC4_REL,
        OSC4_FX_MIX,
        OSC4_FX_P1,
        OSC4_FX_P2,

        // ============ WT Morph (68-71) ============
        OSC1_WT_MORPH = 68,
        OSC2_WT_MORPH = 69,
        OSC3_WT_MORPH = 70,
        OSC4_WT_MORPH = 71,

        // ============ Mixer (200-207) ============
        MIX_CH1_LVL = 200,
        MIX_CH2_LVL,
        MIX_CH3_LVL,
        MIX_CH4_LVL,
        MIX_CH1_PAN,
        MIX_CH2_PAN,
        MIX_CH3_PAN,
        MIX_CH4_PAN,

        // ============ Master FX (220-225) ============
        MFX_MIX = 220,
        MFX_P1,
        MFX_P2,
        MFX_P3,
        MASTER_VOL,
        MASTER_PAN,

        // ============ Tube WaveShaper (226-228) ============
        TUBE_DRIVE = 226,
        TUBE_ASYM,
        TUBE_CHAOS,

        // ============ LFO Self-Modulation (230-237) ============
        LFO1_RATE = 230,
        LFO2_RATE,
        LFO3_RATE,
        LFO4_RATE,
        LFO1_DEPTH,
        LFO2_DEPTH,
        LFO3_DEPTH,
        LFO4_DEPTH,

        NUM_DESTINATIONS
    };

    constexpr int PARAMS_PER_OSC = 17;

    constexpr int getOscIndex(ObsSynthDst dest)
    {
        uint16_t d = static_cast<uint16_t>(dest);
        if (d < 17)
            return 0;
        if (d < 34)
            return 1;
        if (d < 51)
            return 2;
        if (d < 68)
            return 3;
        return -1;
    }

    constexpr int getOscParamIndex(ObsSynthDst dest)
    {
        int osc = getOscIndex(dest);
        if (osc < 0)
            return -1;
        return static_cast<int>(dest) - osc * PARAMS_PER_OSC;
    }

    constexpr ObsSynthDst getOscDest(int osc_index, int param_index)
    {
        return static_cast<ObsSynthDst>(osc_index * PARAMS_PER_OSC + param_index);
    }

} // namespace synth
