#pragma once
// ObsSynth4D's panel values into its engine: what a knob, a preset or a session says (in engine units: the panel's
// ranges, params.hpp) becomes the engine's calls. One place for the app (sound.cpp, on its audio thread) and the CLAP
// plugin (ObsSynth4D.clap, on the host's): whoever owns the engine calls it there, nobody else touches the engine.
#include "params.hpp"
#include "parts.hpp"
#include "../../products/obs_synth/obs_synth_4d.hpp"

#include <array>

namespace obs
{
    struct Driver
    {
        synth::ObsSynth4D &synth;
        std::array<int, 4> tables{1, 1, 1, 1}; // each oscillator's wavetable count (the position: 0..1 of them)
        float tune[4]{}, fine[4]{};            // tune and fine make one pitch
        float vecPts[16][3] = {};
        float xy[2] = {0.5f, 0.5f};
        float modEnv[3][4] = {{0.01f, 0.2f, 0.5f, 0.3f}, {0.01f, 0.2f, 0.5f, 0.3f}, {0.01f, 0.2f, 0.5f, 0.3f}};

        explicit Driver(synth::ObsSynth4D &s) : synth(s) {}

        void osc(int o, int p, float v)
        {
            using namespace synth;
            ObsSynth4D &s = synth;
            const size_t oi = static_cast<size_t>(o);
            switch (p)
            {
            case OSC_WAVE: s.setOscWaveType(o, static_cast<WaveType>(static_cast<int>(v + 0.5f))); break;
            case OSC_TUNE: tune[oi] = v; s.setOscCoarseTune(o, tune[oi] + fine[oi]); break;
            case OSC_FINE_TUNE: fine[oi] = v; s.setOscCoarseTune(o, tune[oi] + fine[oi]); break;
            case OSC_CUTOFF: s.setOscCutoff(o, v); break;
            case OSC_RESO: s.setOscResonance(o, v); break;
            case OSC_HPF: s.setOscHPFCutoff(o, v); break;
            case OSC_FILT_TYPE: s.setOscFilterType(o, static_cast<BiquadType>(static_cast<int>(v + 0.5f))); break;
            case OSC_KEY_TRACK: s.setOscKeyTrack(o, v); break;
            case OSC_FILT_ATK: s.setOscFilterAttack(o, v); break;
            case OSC_FILT_DEC: s.setOscFilterDecay(o, v); break;
            case OSC_FILT_SUS: s.setOscFilterSustain(o, v); break;
            case OSC_FILT_REL: s.setOscFilterRelease(o, v); break;
            case OSC_AMP_ATK: s.setOscAmpAttack(o, v); break;
            case OSC_AMP_DEC: s.setOscAmpDecay(o, v); break;
            case OSC_AMP_SUS: s.setOscAmpSustain(o, v); break;
            case OSC_AMP_REL: s.setOscAmpRelease(o, v); break;
            case OSC_LEVEL: s.setOscLevel(o, v); break;
            case OSC_PAN: s.setOscPan(o, (v + 1.0f) * 0.5f); break;
            case OSC_PART_TYPE: s.setOscPartType(o, partTypeAt(v)); break;
            case OSC_WT_MORPH: // a wavetable's position, and a granular oscillator's (SUPER: its DETUNE)
                s.setOscWtMorphPos(o, v * (tables[oi] > 1 ? static_cast<float>(tables[oi] - 1) : 1.0f));
                s.setOscGrain(o, 0, v);
                break;
            case OSC_GR_SIZE: s.setOscGrain(o, 1, v); break;
            case OSC_GR_DENSITY: s.setOscGrain(o, 2, v); break;
            case OSC_GR_SPRAY: s.setOscGrain(o, 3, v); break; // (SUPER: its SIDE)
            case OSC_FM_ALGORITHM: s.setOscFMAlgorithm(o, static_cast<FMAlgorithm>(static_cast<int>(v + 0.5f))); break;
            case OSC_FM_MOD_RATIO: s.setOscFMModulatorRatio(o, v); break;
            case OSC_FM_MOD_INDEX: s.setOscFMModulationIndex(o, v); break;
            case OSC_FM_FEEDBACK: s.setOscFMFeedback(o, v); break;
            case OSC_FM_OP2_RATIO: s.setOscFMOperatorRatio(o, 2, v); break;
            case OSC_FM_OP2_LEVEL: s.setOscFMOperatorLevel(o, 2, v); break;
            case OSC_FILT_ENV_AMT: s.setOscFilterEnvAmount(o, v); break;
            default: break; // the per-oscillator effects: their slot is not on this panel yet
            }
        }

        void global(int p, float v)
        {
            using namespace synth;
            ObsSynth4D &s = synth;
            switch (p)
            {
            case GLB_MF_CUTOFF: s.setMasterFilterCutoff(v); break;
            case GLB_MF_RESO: s.setMasterFilterResonance(v); break;
            case GLB_MF_TYPE: s.setMasterFilterType(static_cast<BiquadType>(static_cast<int>(v + 0.5f))); break;
            case GLB_MF_BYPASS: s.setMasterFilterBypassed(v > 0.5f); break;
            case GLB_DLY_L: s.setDelayTimeL(v); break;
            case GLB_DLY_R: s.setDelayTimeR(v); break;
            case GLB_DLY_FB: s.setDelayFeedback(v); break;
            case GLB_DLY_MIX: s.setDelayMix(v); break;
            case GLB_DLY_PING: s.setDelayPingPong(v > 0.5f); break;
            case GLB_DLY_DMP: s.setDelayDamping(v); break;
            case GLB_TUBE_DRV: s.setDrive(v); break;
            case GLB_TUBE_ASYM: s.setAsymmetry(v); break;
            case GLB_TUBE_MIX: s.setTubeMix(v); break;
            case GLB_VOLUME: s.setMasterVolume(v); break;
            case GLB_LFO1_RATE:
            case GLB_LFO2_RATE:
            case GLB_LFO3_RATE:
            case GLB_LFO4_RATE: s.setLFORate(p - GLB_LFO1_RATE, v); break;
            case GLB_LFO1_WAVE:
            case GLB_LFO2_WAVE:
            case GLB_LFO3_WAVE:
            case GLB_LFO4_WAVE: s.getLFO(p - GLB_LFO1_WAVE).setWaveType(static_cast<LFOWaveType>(static_cast<int>(v + 0.5f))); break;
            case GLB_XY_X:
            case GLB_XY_Y: // a mod source, and the vector's offset
                xy[p - GLB_XY_X] = v;
                s.getEngine().getModMatrix().setSourceValue(static_cast<int>(p == GLB_XY_X ? ObsSynthSrc::XY_X : ObsSynthSrc::XY_Y), v);
                s.getEngine().setVectorPad(xy[0], xy[1]);
                break;
            default: break;
            }
        }


        // what the panel has no knob for: the mod envelopes (id env*4 + 0..3: A D S R), the tube's model (12) and
        // chaos (13), the vector: on (14), loop (15), points (16), point i's t, x, y (17 + i*3 + 0..2)
        void extra(int id, float v)
        {
            using namespace synth;
        if (id < 12)
        {
            float *e = modEnv[id / 4];
            e[id % 4] = v;
            synth.setModEnv(id / 4, e[0], e[1], e[2], e[3]);
        }
        else if (id == 12)
            synth.setTubeModel(static_cast<TubeModel>(static_cast<int>(v + 0.5f)));
        else if (id == 13)
            synth.setChaos(v);
        else if (id == 14)
            synth.getEngine().setVectorEnabled(v > 0.5f);
        else if (id == 15)
            synth.getEngine().setVectorLoop(v > 0.5f);
        else if (id == 16)
            synth.getEngine().setVectorCount(static_cast<int>(v + 0.5f));
        else if (id >= 17 && id < 17 + 16 * 3)
        {
            const int i = (id - 17) / 3;
            vecPts[i][(id - 17) % 3] = v;
            synth.getEngine().setVectorPoint(i, vecPts[i][0], vecPts[i][1], vecPts[i][2]);
        }
        }

        void route(int slot, int src, int dst, float amount, bool enabled)
        {
            auto &m = synth.getEngine().getModMatrix();
            m.setSlot(slot, src, dst, amount);
            m.setSlotEnabled(slot, enabled);
        }
    };
} // namespace obs
