#pragma once
// The engine's profile: how much a product renders. "full" is the desktop's; "lite" is for a small machine (a Raspberry
// Pi 2: four Cortex-A7 cores at 900 MHz, where 32 voices of four oscillators at 96 kHz with the modulation at every
// sample do not fit in a 5 ms block). Chosen at start: OBS_PROFILE=lite|full, else lite on 32-bit ARM, else full.
#include <cstdlib>
#include <cstring>

namespace synth
{
    struct EngineProfile
    {
        const char *name;
        int maxVoices;   // polyphony played (the allocator steals past it)
        int controlRate; // LFOs, mod matrix, the voices' targets and the filters' coefficients once per this many samples
        int oversample;  // the internal render rate's multiplier (2: the saws and FM alias less; 1: half the work)
        bool skipSilent; // an oscillator at level 0 is not rendered (its envelopes still run)
    };

    inline constexpr EngineProfile kProfileFull{"full", 32, 1, 2, false};
    inline constexpr EngineProfile kProfileLite{"lite", 6, 32, 1, true};

    inline EngineProfile engineProfile()
    {
        if (const char *e = std::getenv("OBS_PROFILE"))
        {
            if (std::strcmp(e, "lite") == 0)
                return kProfileLite;
            if (std::strcmp(e, "full") == 0)
                return kProfileFull;
        }
#if defined(__arm__) && !defined(__aarch64__)
        return kProfileLite;
#else
        return kProfileFull;
#endif
    }
} // namespace synth
