//------------------------------------------------------------------------
// Copyright(c) 2025 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/vst/vsttypes.h"

using namespace Steinberg;
using namespace Vst;

namespace ModularSistemas
{

    // Parameter IDs
    enum ParameterIDs : ParamID
    {
        kMasterVolume = 0,
        kMasterPan,
        kWaveform,
        kOscillatorVolume,
        kAttackTime,
        kDecayTime,
        kSustainLevel,
        kReleaseTime,
        kDryWetMix
    };

    // Parameter count
    constexpr int32 kNumParameters = 9;

} // namespace ModularSistemas