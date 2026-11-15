#pragma once
// ObsSynth4D's ENGINE switch: its places (0..4) and the engine's PartTypes (SAMPLE is ObsSynth One's, not here)
#include "../../lib/synth/chains/osc_chain.hpp"
#include <algorithm>
#include <cmath>

namespace obs
{
    constexpr int kPartCount = 5;
    constexpr synth::PartType kPartTypes[kPartCount] = {synth::PartType::STANDARD_OSC, synth::PartType::WAVETABLE,
                                                        synth::PartType::FM_ENGINE, synth::PartType::GRANULAR,
                                                        synth::PartType::SUPER};

    inline synth::PartType partTypeAt(float place)
    {
        return kPartTypes[std::clamp(static_cast<int>(std::lround(place)), 0, kPartCount - 1)];
    }

    inline float placeOf(synth::PartType p)
    {
        for (int i = 0; i < kPartCount; ++i)
            if (kPartTypes[i] == p)
                return static_cast<float>(i);
        return 0.0f;
    }
} // namespace obs
