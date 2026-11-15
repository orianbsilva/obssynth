#pragma once
// ObsSynthCT1's CC overlay: the factory maps. A target is a parameter by its name ("p.OP2_POS", "p.FM_2_1",
// "p.INDEX", obs_synth_ct1.hpp's names), or a press: @preset+ @preset- @overlay.
#include "../common/cc_overlay.hpp"

#include <string>
#include <vector>

namespace synth::ct1app
{
    // Novation Launchkey 49, with the CCs v1 knew it by: the encoders 21-25 the five operators' positions (the
    // tables are the instrument), 26 INDEX, 27 CUTOFF, 28 REVERB MIX; the faders 71-75 the five levels, 76 DRIVE,
    // 77 DELAY MIX, 78 VEL > IDX, 79 the volume (and 7); the track buttons (102-103, 106-107) the presets.
    inline ccov::Map launchkeyCt1(const std::string &name, const std::string &match)
    {
        ccov::Map m;
        m.name = name;
        m.match = match;
        for (int o = 0; o < 5; ++o)
        {
            m.cc.push_back({21 + o, -1, "p.OP" + std::to_string(o + 1) + "_POS"});
            m.cc.push_back({71 + o, -1, "p.OP" + std::to_string(o + 1) + "_LEVEL"});
        }
        const std::pair<int, const char *> rest[] = {{26, "p.INDEX"}, {27, "p.CUTOFF"}, {28, "p.REV_MIX"}, {76, "p.DRIVE"}, {77, "p.DLY_MIX"},
                                                     {78, "p.VEL_INDEX"}, {79, "p.VOLUME"}, {7, "p.VOLUME"}, {103, "@preset-"}, {102, "@preset+"},
                                                     {106, "@preset-"}, {107, "@preset+"}};
        for (const auto &[cc, t] : rest)
            m.cc.push_back({cc, -1, t});
        return m;
    }

    inline std::vector<ccov::Map> factoryControllers() { return {launchkeyCt1("Launchkey 49", "Launchkey"), launchkeyCt1("Generic", "")}; }
} // namespace synth::ct1app
