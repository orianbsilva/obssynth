#pragma once
// ObsSynth One's values and how they reach the engine, shared by the plugin (plugins/clap/one_clap.cpp) and the panel
// standing alone (obssynthone without --remote): the defaults, the factory's presets, the driver that turns a value
// (0..1, one_params.hpp's ids) into the product's setters (products/obs_one/obs_synth_one.hpp).
#include "one_params.hpp"
#include "../../products/obs_one/obs_synth_one.hpp"
#include "../../lib/synth/core/grain_engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace onep
{
    using Values = std::array<float, NPARAMS>; // 0..1

    inline Values defaults()
    {
        Values v{};
        for (int i = 0; i < NPARAMS; ++i)
            v[static_cast<size_t>(i)] = std::clamp(toNorm(rangeOf(i), rangeOf(i).def), 0.0f, 1.0f);
        // the effects start dry (4D's defaults are a pad's): a program asks for them (`delay`, `distort`), and a dry
        // one is not run
        v[static_cast<size_t>(idOf("GLB_TUBE_MIX"))] = 0.0f;
        v[static_cast<size_t>(idOf("GLB_DLY_MIX"))] = 0.0f;
        return v;
    }

    struct Preset
    {
        const char *name;
        std::vector<std::pair<const char *, float>> set; // by name, engine units, over the defaults
        const char *sample = nullptr;                    // the recording's file name (its index depends on what is installed)
    };
    inline const std::vector<Preset> &presets()
    {
        static const std::vector<Preset> p = {
            {"Init Saw", {}},
            {"Pure Sine", {{"OSC_WAVE", 0}, {"OSC_CUTOFF", 20000}}},
            {"Pluck", {{"OSC_CUTOFF", 700}, {"OSC_FILT_ENV_AMT", 0.7f}, {"OSC_FILT_DEC", 0.25f}, {"OSC_FILT_SUS", 0}, {"OSC_AMP_DEC", 0.35f}, {"OSC_AMP_SUS", 0}}},
            {"Morph Pad", {{"OSC_PART_TYPE", 1}, {"OSC_WT_MORPH", 0.38f}, {"OSC_CUTOFF", 5000}, {"OSC_AMP_ATK", 0.4f}, {"OSC_AMP_REL", 1.2f}}},
            {"FM Bell", {{"OSC_PART_TYPE", 2}, {"OSC_FM_MOD_RATIO", 3.5f}, {"OSC_FM_MOD_INDEX", 6}, {"OSC_AMP_DEC", 1.4f}, {"OSC_AMP_SUS", 0}, {"OSC_AMP_REL", 1.0f}}},
            {"Sample Organ", {{"OSC_PART_TYPE", 4}, {"OSC_SMP_LOOP", 1}, {"OSC_AMP_ATK", 0.05f}, {"OSC_AMP_REL", 0.6f}}, "vcsl-pipe-organ-quiet-C4.wav"},
            {"Grain Cloud", {{"OSC_PART_TYPE", 3}, {"OSC_GR_SIZE", 0.12f}, {"OSC_GR_DENSITY", 40}, {"OSC_GR_SPRAY", 0.3f}, {"OSC_AMP_ATK", 0.5f}, {"OSC_AMP_REL", 1.5f}}},
        };
        return p;
    }
    // a preset's values: its settings over the defaults; indexOf(file name) is the recording's index in the list the
    // sample engine has (-1: not installed)
    template <typename IndexOf>
    Values fromPreset(const Preset &p, IndexOf indexOf)
    {
        Values v = defaults();
        for (const auto &[name, x] : p.set)
            if (const int i = idOf(name); i >= 0)
            {
                const Range &rg = rangeOf(i);
                v[static_cast<size_t>(i)] = std::clamp(toNorm(rg, std::clamp(x, std::min(rg.lo, rg.hi), std::max(rg.lo, rg.hi))), 0.0f, 1.0f);
            }
        if (p.sample)
            if (const int i = indexOf(p.sample); i >= 0)
                v[static_cast<size_t>(idOf("OSC_SMP_SAMPLE"))] = static_cast<float>(i) / 63.0f;
        return v;
    }

    // a value into the product, from the panel's units (params.hpp), as ObsSynth4D's driver does for the oscillator 1
    struct Driver
    {
        synth::ObsSynthOne &one;
        float route[4][3] = {}; // each route's source, destination and amount, kept to say them together
        int routeSlot = 0, lfoSlot = 0; // which of the four routes and two LFOs the parameter being set belongs to
        int tables = 1; // the wavetable's count (the position: 0..1 of them)
        const std::vector<std::shared_ptr<const synth::GrainSource>> *samples = nullptr; // the recordings, loaded on the main thread
        void osc(int p, float v)
        {
            using namespace synth;
            switch (p)
            {
            case OSC_WAVE: one.setWaveType(static_cast<WaveType>(static_cast<int>(v + 0.5f))); break;
            case X_PART_TYPE: one.setPartType(static_cast<PartType>(static_cast<int>(v + 0.5f))); break;
            case X_SMP_SAMPLE:
                if (samples && !samples->empty())
                    one.setSampleSource((*samples)[std::min(static_cast<size_t>(v + 0.5f), samples->size() - 1)]);
                break;
            case X_SMP_LOOP: one.setSampleLoop(v > 0.5f); break;
            case X_SMP_ROOT: one.setSampleRoot(v); break;
            case X_LFO_RATE: one.setLfoRate(lfoSlot, v); break;
            case X_LFO_WAVE: one.setLfoWave(lfoSlot, static_cast<int>(v + 0.5f)); break;
            case X_MOD_SRC: route[routeSlot][0] = v; one.setRoute(routeSlot, static_cast<int>(v + 0.5f), static_cast<int>(route[routeSlot][1] + 0.5f), route[routeSlot][2]); break;
            case X_MOD_DST: route[routeSlot][1] = v; one.setRoute(routeSlot, static_cast<int>(route[routeSlot][0] + 0.5f), static_cast<int>(v + 0.5f), route[routeSlot][2]); break;
            case X_MOD_AMT: route[routeSlot][2] = v; one.setRoute(routeSlot, static_cast<int>(route[routeSlot][0] + 0.5f), static_cast<int>(route[routeSlot][1] + 0.5f), v); break;
            case OSC_TUNE: one.setCoarseTune(v); break;
            case OSC_FINE_TUNE: one.setFineTune(v); break;
            case OSC_LEVEL: one.setLevel(v); break;
            case OSC_PAN: one.setPan((v + 1.0f) * 0.5f); break;
            case OSC_CUTOFF: one.setCutoff(v); break;
            case OSC_RESO: one.setResonance(v); break;
            case OSC_HPF: one.setHPFCutoff(v); break;
            case OSC_FILT_TYPE: one.setFilterType(static_cast<BiquadType>(static_cast<int>(v + 0.5f))); break;
            case OSC_KEY_TRACK: one.setKeyTrack(v); break;
            case OSC_FILT_ENV_AMT: one.setFilterEnvAmount(v); break;
            case OSC_FILT_ATK: one.setFilterAttack(v); break;
            case OSC_FILT_DEC: one.setFilterDecay(v); break;
            case OSC_FILT_SUS: one.setFilterSustain(v); break;
            case OSC_FILT_REL: one.setFilterRelease(v); break;
            case OSC_AMP_ATK: one.setAmpAttack(v); break;
            case OSC_AMP_DEC: one.setAmpDecay(v); break;
            case OSC_AMP_SUS: one.setAmpSustain(v); break;
            case OSC_AMP_REL: one.setAmpRelease(v); break;
            case OSC_WT_MORPH: // a wavetable's position, and a granular oscillator's
                one.setWtMorphPos(v * (tables > 1 ? static_cast<float>(tables - 1) : 1.0f));
                one.setGrain(0, v);
                one.setSampleStart(v);
                break;
            case OSC_GR_SIZE: one.setGrain(1, v); break;
            case OSC_GR_DENSITY: one.setGrain(2, v); break;
            case OSC_GR_SPRAY: one.setGrain(3, v); break;
            case OSC_FM_ALGORITHM: one.setFMAlgorithm(static_cast<FMAlgorithm>(static_cast<int>(v + 0.5f))); break;
            case OSC_FM_MOD_RATIO: one.setFMModulatorRatio(v); break;
            case OSC_FM_MOD_INDEX: one.setFMModulationIndex(v); break;
            case OSC_FM_FEEDBACK: one.setFMFeedback(v); break;
            case OSC_FM_OP2_RATIO: one.setFMOperatorRatio(2, v); break;
            case OSC_FM_OP2_LEVEL: one.setFMOperatorLevel(2, v); break;
            default: break;
            }
        }
        void global(int p, float v)
        {
            switch (p)
            {
            case GLB_TUBE_DRV: one.setDrive(v); break;
            case GLB_TUBE_ASYM: one.setAsymmetry(v); break;
            case GLB_TUBE_MIX: one.setTubeMix(v); break;
            case GLB_DLY_L: one.setDelayTimeL(v); break;
            case GLB_DLY_R: one.setDelayTimeR(v); break;
            case GLB_DLY_FB: one.setDelayFeedback(v); break;
            case GLB_DLY_MIX: one.setDelayMix(v); break;
            case GLB_DLY_PING: one.setDelayPingPong(v > 0.5f); break;
            case GLB_DLY_DMP: one.setDelayDamping(v); break;
            case GLB_VOLUME: one.setVolume(v); break;
            default: break;
            }
        }
    };

    inline void drive(Driver &dv, int id, float n)
    {
        const float x = toValue(rangeOf(id), n);
        // MODn_* and LFOn_*: the number in the name says which
        if (DEFS[id].idx == X_MOD_SRC || DEFS[id].idx == X_MOD_DST || DEFS[id].idx == X_MOD_AMT)
            dv.routeSlot = DEFS[id].name[3] - '1';
        if (DEFS[id].idx == X_LFO_RATE || DEFS[id].idx == X_LFO_WAVE)
            dv.lfoSlot = DEFS[id].name[3] - '1';
        if (DEFS[id].glb)
            dv.global(DEFS[id].idx, x);
        else
            dv.osc(DEFS[id].idx, x);
    }
    inline void applyAll(Driver &dv, const Values &v)
    {
        for (int i = 0; i < NPARAMS; ++i)
            drive(dv, i, v[static_cast<size_t>(i)]);
    }

} // namespace onep
