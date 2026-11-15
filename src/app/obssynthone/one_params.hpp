#pragma once
// ObsSynth One's parameters, shared by the plugin (plugins/clap/one_clap.cpp) and its panel (obssynthone): their
// numbers are the CLAP parameter ids and the lines of the link between them. 4D's names and ranges (params.hpp).
#include "../obssynth4d/params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace onep
{
    using namespace obs;
    // what the One shows of the engine: (name, global?, index in OscParam or GlobalParam)
    struct Def
    {
        const char *name;
        bool glb;
        int idx;
    };
    // what One has of its own, past 4D's lists (idx 1000 + k indexes EXTRA): the engine has a fifth value (sample), and the
    // sample player's choice, loop and root note
    enum Extra { X_PART_TYPE = 1000, X_SMP_SAMPLE, X_SMP_LOOP, X_SMP_ROOT, X_MOD_SRC, X_MOD_DST, X_MOD_AMT, X_LFO_RATE, X_LFO_WAVE };
    constexpr Range EXTRA[] = {
        {0, 4, 0, false, 5},     // engine: std wt fm gr smp
        {0, 63, 0, false, 64},   // sample: the index in the plugin's list
        {0, 1, 0, false, 2},     // loop
        {12, 108, 60, false, 97}, // root note, MIDI
        {0, 8, 0, false, 9},      // a route's source: off, LFO 1, LFO 2, filter env, amp env, velocity, wheel, key, random
        {0, 7, 0, false, 8},      // a route's destination: position, cutoff, reso, pitch, level, pan, FM index, filter env amount
        {-1, 1, 0, false, 0},     // a route's amount
        {0.01f, 20, 2, true, 0},  // an LFO's rate, Hz
        {0, 4, 0, false, 5},      // an LFO's shape: sine, triangle, square, saw, random
    };
    constexpr Def DEFS[] = {
        {"OSC_PART_TYPE", false, X_PART_TYPE}, // the engine: std wt fm gr smp
        {"OSC_WAVE", false, OSC_WAVE},
        {"OSC_WT_MORPH", false, OSC_WT_MORPH}, // the position: the wavetable's frame, the grains' read point, the sample's start
        {"OSC_TUNE", false, OSC_TUNE},
        {"OSC_FINE_TUNE", false, OSC_FINE_TUNE},
        {"OSC_LEVEL", false, OSC_LEVEL},
        {"OSC_PAN", false, OSC_PAN},
        {"OSC_CUTOFF", false, OSC_CUTOFF},
        {"OSC_RESO", false, OSC_RESO},
        {"OSC_HPF", false, OSC_HPF},
        {"OSC_FILT_TYPE", false, OSC_FILT_TYPE},
        {"OSC_KEY_TRACK", false, OSC_KEY_TRACK},
        {"OSC_FILT_ENV_AMT", false, OSC_FILT_ENV_AMT},
        {"OSC_FILT_ATK", false, OSC_FILT_ATK},
        {"OSC_FILT_DEC", false, OSC_FILT_DEC},
        {"OSC_FILT_SUS", false, OSC_FILT_SUS},
        {"OSC_FILT_REL", false, OSC_FILT_REL},
        {"OSC_AMP_ATK", false, OSC_AMP_ATK},
        {"OSC_AMP_DEC", false, OSC_AMP_DEC},
        {"OSC_AMP_SUS", false, OSC_AMP_SUS},
        {"OSC_AMP_REL", false, OSC_AMP_REL},
        {"OSC_FM_ALGORITHM", false, OSC_FM_ALGORITHM},
        {"OSC_FM_MOD_RATIO", false, OSC_FM_MOD_RATIO},
        {"OSC_FM_MOD_INDEX", false, OSC_FM_MOD_INDEX},
        {"OSC_FM_FEEDBACK", false, OSC_FM_FEEDBACK},
        {"OSC_FM_OP2_RATIO", false, OSC_FM_OP2_RATIO},
        {"OSC_FM_OP2_LEVEL", false, OSC_FM_OP2_LEVEL},
        {"OSC_GR_SIZE", false, OSC_GR_SIZE},
        {"OSC_GR_DENSITY", false, OSC_GR_DENSITY},
        {"OSC_GR_SPRAY", false, OSC_GR_SPRAY},
        {"GLB_TUBE_DRV", true, GLB_TUBE_DRV},
        {"GLB_TUBE_ASYM", true, GLB_TUBE_ASYM},
        {"GLB_TUBE_MIX", true, GLB_TUBE_MIX},
        {"GLB_DLY_L", true, GLB_DLY_L},
        {"GLB_DLY_R", true, GLB_DLY_R},
        {"GLB_DLY_FB", true, GLB_DLY_FB},
        {"GLB_DLY_MIX", true, GLB_DLY_MIX},
        {"GLB_DLY_PING", true, GLB_DLY_PING},
        {"GLB_DLY_DMP", true, GLB_DLY_DMP},
        {"GLB_VOLUME", true, GLB_VOLUME},
        {"OSC_SMP_SAMPLE", false, X_SMP_SAMPLE},
        {"OSC_SMP_LOOP", false, X_SMP_LOOP},
        {"OSC_SMP_ROOT", false, X_SMP_ROOT},
        {"MOD1_SRC", false, X_MOD_SRC}, {"MOD1_DST", false, X_MOD_DST}, {"MOD1_AMT", false, X_MOD_AMT},
        {"MOD2_SRC", false, X_MOD_SRC}, {"MOD2_DST", false, X_MOD_DST}, {"MOD2_AMT", false, X_MOD_AMT},
        {"MOD3_SRC", false, X_MOD_SRC}, {"MOD3_DST", false, X_MOD_DST}, {"MOD3_AMT", false, X_MOD_AMT},
        {"MOD4_SRC", false, X_MOD_SRC}, {"MOD4_DST", false, X_MOD_DST}, {"MOD4_AMT", false, X_MOD_AMT},
        {"LFO1_RATE", false, X_LFO_RATE}, {"LFO1_WAVE", false, X_LFO_WAVE},
        {"LFO2_RATE", false, X_LFO_RATE}, {"LFO2_WAVE", false, X_LFO_WAVE},
    };
    constexpr int NPARAMS = static_cast<int>(sizeof DEFS / sizeof DEFS[0]);
    constexpr unsigned PROGRAM_ID = NPARAMS;

    inline const char *const kModSources[9] = {"Off", "LFO 1", "LFO 2", "Filter Env", "Amp Env", "Velocity", "Wheel", "Key", "Random"};
    inline const char *const kModDests[8] = {"Position", "Cutoff", "Resonance", "Pitch", "Level", "Pan", "FM Index", "Filter Env"};
    inline const char *const kLfoWaves[5] = {"Sine", "Triangle", "Square", "Saw", "Random"};

    inline const Range &rangeOf(int id)
    {
        return DEFS[id].idx >= 1000 ? EXTRA[DEFS[id].idx - 1000] : DEFS[id].glb ? GLB_RANGES[DEFS[id].idx] : OSC_RANGES[DEFS[id].idx];
    }
    inline int idOfC(const char *name) // without allocating (the audio thread)
    {
        for (int i = 0; i < NPARAMS; ++i)
            if (std::strcmp(name, DEFS[i].name) == 0)
                return i;
        return -1;
    }
    inline int idOf(const std::string &name)
    {
        for (int i = 0; i < NPARAMS; ++i)
            if (name == DEFS[i].name)
                return i;
        return -1;
    }


    inline std::string shown(int id, float n)
    {
        const Range &rg = rangeOf(id);
        const float v = toValue(rg, n);
        const std::string name = DEFS[id].name;
        char b[48];
        const double d = static_cast<double>(v);
        auto has = [&](const char *s) { return name.find(s) != std::string::npos; };
        if (rg.steps > 0)
            std::snprintf(b, sizeof b, "%d", static_cast<int>(std::lround(v)));
        else if (has("CUTOFF") || has("HPF"))
            std::snprintf(b, sizeof b, d >= 1000 ? "%.2f kHz" : "%.1f Hz", d >= 1000 ? d / 1000 : d);
        else if (has("_ATK") || has("_DEC") || has("_REL") || has("DLY_L") || has("DLY_R") || has("GR_SIZE"))
            std::snprintf(b, sizeof b, d >= 1 ? "%.2f s" : "%.0f ms", d >= 1 ? d : d * 1000);
        else if (has("TUNE"))
            std::snprintf(b, sizeof b, "%+.2f st", d);
        else
            std::snprintf(b, sizeof b, "%.2f", d);
        return b;
    }

} // namespace onep
