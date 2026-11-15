#pragma once
// The parameters the panel sets, as synth_view names them (main_screen.hpp: OscParam, GlobalParam and their
// ranges), so both apps speak of the same knobs; plus the LFOs and the mod matrix, which synth_view drives
// apart. Engine units here; the panel's controls hold 0..1 and map through range().
#include <cmath>

namespace obs
{
    enum OscParam
    {
        OSC_WAVE = 0,
        OSC_TUNE,
        OSC_FINE_TUNE,
        OSC_CUTOFF,
        OSC_RESO,
        OSC_HPF,
        OSC_FILT_TYPE,
        OSC_KEY_TRACK,
        OSC_FILT_ATK,
        OSC_FILT_DEC,
        OSC_FILT_SUS,
        OSC_FILT_REL,
        OSC_AMP_ATK,
        OSC_AMP_DEC,
        OSC_AMP_SUS,
        OSC_AMP_REL,
        OSC_FX_P1,
        OSC_FX_P2,
        OSC_FX_P3,
        OSC_LEVEL,
        OSC_PAN,
        OSC_PART_TYPE,
        OSC_WT_MORPH,
        OSC_FM_ALGORITHM,
        OSC_FM_MOD_RATIO,
        OSC_FM_MOD_INDEX,
        OSC_FM_FEEDBACK,
        OSC_FM_OP2_RATIO,
        OSC_FM_OP2_LEVEL,
        OSC_FILT_ENV_AMT, // the panel's: synth_view has no knob for it
        OSC_GR_SIZE,      // granular: a grain's length, grains per second, the position's spread (the position:
        OSC_GR_DENSITY,   // OSC_WT_MORPH, the same knob and the same matrix destination)
        OSC_GR_SPRAY,
        OSC_PARAM_COUNT
    };

    enum GlobalParam
    {
        GLB_MF_CUTOFF,
        GLB_MF_RESO,
        GLB_MF_TYPE,
        GLB_MF_BYPASS,
        GLB_DLY_L,
        GLB_DLY_R,
        GLB_DLY_FB,
        GLB_DLY_MIX,
        GLB_DLY_PING,
        GLB_DLY_DMP,
        GLB_TUBE_DRV,
        GLB_TUBE_ASYM,
        GLB_TUBE_MIX,
        GLB_MFX_P1,
        GLB_MFX_P2,
        GLB_MFX_P3,
        GLB_VOLUME,
        GLB_LFO1_RATE, // the panel's: the LFOs' rates and shapes
        GLB_LFO2_RATE,
        GLB_LFO3_RATE,
        GLB_LFO4_RATE,
        GLB_LFO1_WAVE,
        GLB_LFO2_WAVE,
        GLB_LFO3_WAVE,
        GLB_LFO4_WAVE,
        GLB_XY_X, // the XY pad: mod sources XY_X, XY_Y
        GLB_XY_Y,
        GLB_PARAM_COUNT
    };

    struct Range
    {
        float lo, hi, def;
        bool log;   // frequencies and times: even turns, even octaves
        int steps;  // a discrete choice of steps values (0: continuous)
    };

    constexpr Range OSC_RANGES[OSC_PARAM_COUNT] = {
        {0, 5, 1, false, 6},          // wave: sine saw square triangle pulse noise
        {-24, 24, 0, false, 49},      // tune, semitones
        {-1, 1, 0, false, 0},         // fine
        {20, 20000, 10000, true, 0},  // cutoff
        {0, 1, 0, false, 0},          // reso
        {20, 2000, 20, true, 0},      // hpf
        {0, 3, 0, false, 4},          // filter type
        {0, 1, 0, false, 0},          // key track
        {0.001f, 2, 0.01f, true, 0},  // filter env A
        {0.001f, 2, 0.2f, true, 0},   // D
        {0, 1, 0.5f, false, 0},       // S
        {0.01f, 5, 0.3f, true, 0},    // R
        {0.001f, 2, 0.01f, true, 0},  // amp env A
        {0.001f, 2, 0.2f, true, 0},   // D
        {0, 1, 0.7f, false, 0},       // S
        {0.01f, 5, 0.3f, true, 0},    // R
        {0, 1, 0.5f, false, 0},       // fx p1
        {0, 1, 0.5f, false, 0},       // fx p2
        {0, 1, 0.5f, false, 0},       // fx p3
        {0, 1, 0.8f, false, 0},       // level
        {-1, 1, 0, false, 0},         // pan
        {0, 4, 0, false, 5},          // part: std wt fm gr super (parts.hpp)
        {0, 1, 0, false, 0},          // wt morph
        {0, 4, 1, false, 5},          // fm algorithm
        {0.1f, 16, 1, true, 0},       // fm mod ratio
        {0, 20, 1, false, 0},         // fm index
        {0, 1, 0, false, 0},          // fm feedback
        {0.1f, 16, 2, true, 0},       // op2 ratio
        {0, 1, 0.5f, false, 0},       // op2 level
        {-1, 1, 0, false, 0},         // filter env amount
        {0.01f, 0.5f, 0.08f, true, 0}, // grain size, s
        {1, 100, 25, true, 0},        // grains per second
        {0, 1, 0.1f, false, 0},       // spray
    };

    constexpr Range GLB_RANGES[GLB_PARAM_COUNT] = {
        {20, 20000, 20000, true, 0}, // master filter cutoff
        {0, 1, 0, false, 0},         // reso
        {0, 3, 0, false, 4},         // type
        {0, 1, 0, false, 2},         // bypass
        {0.01f, 2, 0.375f, true, 0}, // delay L
        {0.01f, 2, 0.5f, true, 0},   // delay R
        {0, 0.95f, 0.35f, false, 0}, // feedback
        {0, 1, 0.25f, false, 0},     // mix
        {0, 1, 1, false, 2},         // ping-pong
        {0, 1, 0.4f, false, 0},      // damp
        {0.5f, 8, 1.8f, true, 0},    // tube drive
        {0, 1, 0.3f, false, 0},      // asym
        {0, 1, 0.4f, false, 0},      // mix
        {0, 1, 0.5f, false, 0},      // mfx
        {0, 1, 0.5f, false, 0},
        {0, 1, 0.5f, false, 0},
        {0, 1, 0.8f, false, 0},      // volume
        {0.01f, 20, 2, true, 0},     // lfo rates
        {0.01f, 20, 0.8f, true, 0},
        {0.01f, 20, 0.3f, true, 0},
        {0.01f, 20, 5, true, 0},
        {0, 4, 0, false, 5}, // lfo shapes: sine triangle square saw random
        {0, 4, 1, false, 5},
        {0, 4, 0, false, 5},
        {0, 4, 3, false, 5},
        {0, 1, 0.5f, false, 0}, // xy
        {0, 1, 0.5f, false, 0},
    };

    // 0..1 to engine units and back
    inline float toValue(const Range &r, float n)
    {
        float v = r.log ? r.lo * std::pow(r.hi / r.lo, n) : r.lo + n * (r.hi - r.lo);
        if (r.steps > 1)
            v = r.lo + std::round((v - r.lo) / (r.hi - r.lo) * static_cast<float>(r.steps - 1)) * (r.hi - r.lo) / static_cast<float>(r.steps - 1);
        return v;
    }

    inline float toNorm(const Range &r, float v)
    {
        if (r.log)
            return std::log(v / r.lo) / std::log(r.hi / r.lo);
        return (v - r.lo) / (r.hi - r.lo);
    }
} // namespace obs
