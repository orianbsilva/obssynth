#pragma once
// ObsDrums' CC overlay: the factory maps and the names of what a controller can move.
//   pad.<1..16>.<P>   a pad's        P: TUNE DECAY A B FX PAN LEVEL (values), MUTE SOLO BENT HIT (presses)
//   pad.sel.<P>       the pad chosen (a click in its column, or a hit from the controller's pads)
//   pad.bank<1..8>.<P> the bank's pads (bank 1: pads 1-8, bank 2: pads 9-16)
//   master.<M>        DELAY_TIME DELAY_FB DELAY_MIX REV_SIZE REV_DAMP REV_MIX DRIVE GLUE TUBE_MIX VOLUME
//   @bank+ @bank- @pad+ @pad- @kit+ @kit- @overlay   presses
#include "../common/cc_overlay.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace synth::drums
{
    inline const char *kPadIds[] = {"TUNE", "DECAY", "A", "B", "FX", "PAN", "LEVEL", "MUTE", "SOLO", "BENT", "HIT"};
    inline const char *kMasterIds[] = {"DELAY_TIME", "DELAY_FB", "DELAY_MIX", "REV_SIZE", "REV_DAMP", "REV_MIX", "DRIVE", "GLUE", "TUBE_MIX", "VOLUME"};

    // Novation Launchkey 49 with the CCs v1 (synth_view) knew it by: the encoders 21-28, the faders 71-78 and
    // 79 (the ninth); the track buttons (102-103, 106-107); the 16 pads in drum mode send 36-51, here the 16 pads
    // in order. Learn changes any of it.
    inline ccov::Map launchkeyDrums(const std::string &name, const std::string &match)
    {
        ccov::Map m;
        m.name = name;
        m.match = match;
        const char *knobs[] = {"pad.sel.TUNE", "pad.sel.DECAY", "pad.sel.A", "pad.sel.B", "pad.sel.FX", "pad.sel.PAN", "master.GLUE", "master.REV_MIX"};
        for (int i = 0; i < 8; ++i)
        {
            m.cc.push_back({21 + i, -1, knobs[i]});
            m.cc.push_back({71 + i, -1, "pad.bank" + std::to_string(i + 1) + ".LEVEL"});
        }
        m.cc.push_back({79, -1, "master.VOLUME"});
        m.cc.push_back({7, -1, "master.VOLUME"});
        m.cc.push_back({103, -1, "@bank-"});
        m.cc.push_back({102, -1, "@bank+"});
        m.cc.push_back({106, -1, "@pad-"});
        m.cc.push_back({107, -1, "@pad+"});
        if (!match.empty())
            for (int i = 0; i < 16; ++i)
                m.notes[static_cast<size_t>(36 + i)] = i;
        return m;
    }

    inline std::vector<ccov::Map> factoryControllers()
    {
        return {launchkeyDrums("Launchkey 49", "Launchkey"), launchkeyDrums("Generic", "")};
    }
} // namespace synth::drums
