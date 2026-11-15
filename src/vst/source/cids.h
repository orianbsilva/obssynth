//------------------------------------------------------------------------
// Copyright(c) 2025 My Plug-in Company.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace ModularSistemas
{
    //------------------------------------------------------------------------
    static const Steinberg::FUID kObsSynthProcessorUID(0xDA88C81B, 0x43DE5533, 0xA76359DD, 0x8C0FA6ED);
    static const Steinberg::FUID kObsSynthControllerUID(0x5A0BE953, 0x06245B56, 0x83D6ADF7, 0xC0F55FAC);

#define ObsSynthVST3Category "Instrument|Synth"

    //------------------------------------------------------------------------
} // namespace ModularSistemas
