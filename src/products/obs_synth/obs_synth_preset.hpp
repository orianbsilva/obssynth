/**
 * @file obs_synth_preset.hpp
 * @brief Preset4D - Declarative preset structure for ObsSynth4D
 *
 * Pure data struct that fully describes a synth preset.
 * Can be loaded from JSON at runtime (no recompilation needed).
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include "../../lib/oscillators/oscillator.hpp"
#include "../../lib/oscillators/lfo.hpp"
#include "../../lib/effects/waveshaper.hpp"
#include "../../lib/synth/chains/osc_chain.hpp"
#include "obs_synth_mod_source.hpp"
#include "obs_synth_mod_dest.hpp"
#include <cstdint>
#include <array>

namespace synth
{

    struct ModRoute
    {
        ObsSynthSrc src;
        ObsSynthDst dst;
        float amount;
    };

    static constexpr int PRESET_MAX_ROUTES = 24;
    static constexpr int PRESET_NUM_OSCS = 4;
    static constexpr int PRESET_NUM_LFOS = 4;
    static constexpr int PRESET_NUM_MOD_ENVS = 3;

    struct LFOConfig
    {
        float rate;
        LFOWaveType wave;
    };

    struct ModEnvConfig
    {
        float attack;
        float decay;
        float sustain;
        float release;
    };

    struct DelayConfig
    {
        float timeL;
        float timeR;
        float feedback;
        float mix;
        float damping;
        bool pingPong;
    };

    struct TubeConfig
    {
        TubeModel model;
        float drive;
        float asymmetry;
        float chaos;
        float mix;
    };

    /// Physical model tube reverb (P1=tube_length, P2=reflection, P3=mix)
    struct TubeReverbConfig
    {
        float tube_length; ///< P1: metres [0.3 – 6.0]
        float reflection;  ///< P2: reflection coeff [0.5 – 0.99]
        float mix;         ///< P3: dry/wet [0.0 – 1.0]  (0.0 = off)
    };

    // vector synthesis: a path through the square of the four oscillators, from each note-on
    struct VectorPoint4D
    {
        float t, x, y; // seconds; 0..1 (x: from 1 and 3 to 2 and 4; y: from 3 and 4 up to 1 and 2)
    };
    struct VectorConfig
    {
        bool on = false;
        bool loop = false;
        int count = 0;
        VectorPoint4D path[16] = {};
    };

    struct Preset4D
    {
        // Identity
        const char *name;

        // Oscillator chains (per-osc arrays)
        WaveType waves[PRESET_NUM_OSCS];
        float levels[PRESET_NUM_OSCS];
        float pans[PRESET_NUM_OSCS];
        float cutoffs[PRESET_NUM_OSCS];
        float resonances[PRESET_NUM_OSCS];
        float filterEnvAmounts[PRESET_NUM_OSCS];
        float coarseTune[PRESET_NUM_OSCS];

        // Amp ADSR (per-osc)
        float ampA[PRESET_NUM_OSCS];
        float ampD[PRESET_NUM_OSCS];
        float ampS[PRESET_NUM_OSCS];
        float ampR[PRESET_NUM_OSCS];

        // Filter ADSR (per-osc)
        float fltA[PRESET_NUM_OSCS];
        float fltD[PRESET_NUM_OSCS];
        float fltS[PRESET_NUM_OSCS];
        float fltR[PRESET_NUM_OSCS];

        // LFOs
        LFOConfig lfos[PRESET_NUM_LFOS];

        // Mod Envelopes
        ModEnvConfig modEnvs[PRESET_NUM_MOD_ENVS];

        // Master FX
        DelayConfig delay;
        TubeConfig tube;

        // Mod Matrix routes
        ModRoute routes[PRESET_MAX_ROUTES];
        int numRoutes;

        // Master filter (applied after per-chain filters, before delay)
        float masterFilterCutoff = 20000.0f; ///< Master low-pass cutoff in Hz (20–20000)
        float masterFilterResonance = 0.0f;  ///< Master filter resonance (0–1)

        // Physical model tube reverb (off by default – mix=0)
        TubeReverbConfig tubeReverb = {1.2f, 0.92f, 0.0f};

        // ── PartType per slot (default: STANDARD_OSC → zero breaking change) ──
        PartType partTypes[PRESET_NUM_OSCS] = {
            PartType::STANDARD_OSC, PartType::STANDARD_OSC,
            PartType::STANDARD_OSC, PartType::STANDARD_OSC};

        // ── Wavetable morph position (0 = first table) ──
        float wtMorphPos[PRESET_NUM_OSCS] = {0.0f, 0.0f, 0.0f, 0.0f};

        // ── FM parameters per slot (only used when partType == FM_ENGINE) ──
        struct FMPresetParams
        {
            FMAlgorithm algorithm = FMAlgorithm::TWO_OP_FEEDBACK;
            float modRatio = 1.0f;
            float modIndex = 1.0f;
            float feedback = 0.0f;
            float op2Ratio = 2.0f;
            float op2Level = 0.5f;
        };
        FMPresetParams fmParams[PRESET_NUM_OSCS] = {};

        VectorConfig vector = {};

        // granular oscillators: where in the recording (0..1), grain length (s), grains per second, spray (0..1)
        struct GrainParams
        {
            float position = 0.3f;
            float size = 0.08f;
            float density = 25.0f;
            float spray = 0.1f;
        };
        GrainParams grain[PRESET_NUM_OSCS] = {};
        // each granular oscillator's recording: a file name in the samples folder (nullptr: the player's default)
        const char *grainSample[PRESET_NUM_OSCS] = {nullptr, nullptr, nullptr, nullptr};
    };

} // namespace synth
