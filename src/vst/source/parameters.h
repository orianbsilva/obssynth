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
        kDryWetMix,

        // FM Synthesis Parameters
        kFMModulatorRatio,
        kFMModulationIndex,
        kFMAlgorithm,
        kFMOperator2Ratio,
        kFMOperator2Level,
        kFMOperator3Ratio,
        kFMOperator3Level,
        kFMOperator4Ratio,
        kFMOperator4Level,
        kFMFeedback,

        // Wave Table Parameters
        kWaveTablePosition,
        kWaveTableMorph,
        kPCMMix,
        kFMMix,

        // LFO and Modulation
        kLFORate,
        kLFODepth,

        // Filter Parameters
        kFilterCutoff,
        kFilterResonance,

        // Effects Parameters
        kChorusRate,
        kChorusDepth,
        kReverbRoom,
        kReverbMix,
        kOverdrive,
        kGlobalMix
    };

} // namespace ModularSistemas