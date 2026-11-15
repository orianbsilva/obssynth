#pragma once
// ObsSynth4D's CC overlay: the factory maps and the names of what a controller can move (params.hpp's, without
// their OSC_ / GLB_):
//   osc.<1..4>.<P>   an oscillator's      (CUTOFF, RESO, AMP_ATK, LEVEL, WT_MORPH...)
//   osc.sel.<P>      the chosen oscillator's (the one whose filter and envelopes MAIN shows)
//   glb.<G>          the rest              (VOLUME, TUBE_DRV, DLY_MIX, MF_CUTOFF, LFO1_RATE, XY_X...)
//   osc.all.<P>      the four at once
//   lfo.sel.RATE, lfo.sel.WAVE  the chosen LFO's
//   mod.WHEEL mod.EXPRESSION mod.CC_A..D   the matrix's sources (no knob: they follow at once)
//   extra.TUBE_CHAOS the tube's chaos (a preset's, no knob)
//   @osc+ @osc- @lfo+ @lfo- @page @preset+ @preset- @overlay   presses
#include "../common/cc_overlay.hpp"
#include "params.hpp"

#include <string>
#include <vector>

namespace obs
{
    inline const char *kOscIds[OSC_PARAM_COUNT] = {
        "WAVE", "TUNE", "FINE_TUNE", "CUTOFF", "RESO", "HPF", "FILT_TYPE", "KEY_TRACK", "FILT_ATK", "FILT_DEC", "FILT_SUS",
        "FILT_REL", "AMP_ATK", "AMP_DEC", "AMP_SUS", "AMP_REL", "FX_P1", "FX_P2", "FX_P3", "LEVEL", "PAN", "PART_TYPE",
        "WT_MORPH", "FM_ALGORITHM", "FM_MOD_RATIO", "FM_MOD_INDEX", "FM_FEEDBACK", "FM_OP2_RATIO", "FM_OP2_LEVEL",
        "FILT_ENV_AMT", "GR_SIZE", "GR_DENSITY", "GR_SPRAY"};
    inline const char *kGlbIds[GLB_PARAM_COUNT] = {
        "MF_CUTOFF", "MF_RESO", "MF_TYPE", "MF_BYPASS", "DLY_L", "DLY_R", "DLY_FB", "DLY_MIX", "DLY_PING", "DLY_DMP",
        "TUBE_DRV", "TUBE_ASYM", "TUBE_MIX", "MFX_P1", "MFX_P2", "MFX_P3", "VOLUME", "LFO1_RATE", "LFO2_RATE",
        "LFO3_RATE", "LFO4_RATE", "LFO1_WAVE", "LFO2_WAVE", "LFO3_WAVE", "LFO4_WAVE", "XY_X", "XY_Y"};

    // Novation Launchkey 49 as v1 (synth_view) had it: the encoders 21-28 on the four pans, LFO 1's rate, the
    // delay's feedback, the tube's drive and mix, 29 its chaos; 30-33 the matrix's CC A-D, 34-37 the four
    // wavetable positions; the faders 71-74 the four levels, 75-78 cutoff, resonance, attack and release of the
    // four at once, 79 the delay's mix. And: 7 the volume, the track buttons (102-103, 106-107) the oscillator
    // MAIN shows. Learn changes any of it.
    inline ccov::Map launchkeySynth(const std::string &name, const std::string &match)
    {
        ccov::Map m;
        m.name = name;
        m.match = match;
        const char *knobs[] = {"osc.1.PAN", "osc.2.PAN", "osc.3.PAN", "osc.4.PAN", "glb.LFO1_RATE",
                               "glb.DLY_FB", "glb.TUBE_DRV", "glb.TUBE_MIX", "extra.TUBE_CHAOS"};
        for (int i = 0; i < 9; ++i)
            m.cc.push_back({21 + i, -1, knobs[i]});
        for (int i = 0; i < 4; ++i)
        {
            m.cc.push_back({30 + i, -1, std::string("mod.CC_") + static_cast<char>('A' + i)});
            m.cc.push_back({34 + i, -1, "osc." + std::to_string(i + 1) + ".WT_MORPH"});
            m.cc.push_back({71 + i, -1, "osc." + std::to_string(i + 1) + ".LEVEL"});
        }
        const char *faders[] = {"osc.all.CUTOFF", "osc.all.RESO", "osc.all.AMP_ATK", "osc.all.AMP_REL", "glb.DLY_MIX"};
        for (int i = 0; i < 5; ++i)
            m.cc.push_back({75 + i, -1, faders[i]});
        m.cc.push_back({7, -1, "glb.VOLUME"});
        m.cc.push_back({103, -1, "@osc-"});
        m.cc.push_back({102, -1, "@osc+"});
        m.cc.push_back({106, -1, "@osc-"});
        m.cc.push_back({107, -1, "@osc+"});
        return m;
    }

    inline std::vector<ccov::Map> factoryControllers()
    {
        return {launchkeySynth("Launchkey 49", "Launchkey"), launchkeySynth("Generic", "")};
    }
} // namespace obs
