/**
 * @file obs_synth_preset_io.hpp
 * @brief JSON serialization/deserialization for Preset4D using nlohmann/json
 *
 * Load/save presets at runtime without recompilation.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include "obs_synth_preset.hpp"
#include "../../vendor/json.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <cstring>
#include <type_traits>
#include <iostream>

namespace synth
{
    using json = nlohmann::json;

    // ============ String <-> Enum helpers ============

    inline const char *waveTypeToStr(WaveType w)
    {
        switch (w)
        {
        case WaveType::SINE:
            return "SINE";
        case WaveType::SAW:
            return "SAW";
        case WaveType::SQUARE:
            return "SQUARE";
        case WaveType::TRIANGLE:
            return "TRIANGLE";
        case WaveType::NOISE:
            return "NOISE";
        default:
            return "SINE";
        }
    }

    inline WaveType strToWaveType(const std::string &s)
    {
        if (s == "SAW")
            return WaveType::SAW;
        if (s == "SQUARE")
            return WaveType::SQUARE;
        if (s == "TRIANGLE")
            return WaveType::TRIANGLE;
        if (s == "NOISE")
            return WaveType::NOISE;
        return WaveType::SINE;
    }

    inline const char *lfoWaveToStr(LFOWaveType w)
    {
        switch (w)
        {
        case LFOWaveType::SINE:
            return "SINE";
        case LFOWaveType::TRIANGLE:
            return "TRIANGLE";
        case LFOWaveType::SQUARE:
            return "SQUARE";
        case LFOWaveType::SAW:
            return "SAW";
        case LFOWaveType::RANDOM:
            return "RANDOM";
        default:
            return "SINE";
        }
    }

    inline LFOWaveType strToLFOWave(const std::string &s)
    {
        if (s == "TRIANGLE")
            return LFOWaveType::TRIANGLE;
        if (s == "SQUARE")
            return LFOWaveType::SQUARE;
        if (s == "SAW")
            return LFOWaveType::SAW;
        if (s == "RANDOM")
            return LFOWaveType::RANDOM;
        return LFOWaveType::SINE;
    }

    inline const char *tubeModelToStr(TubeModel m)
    {
        switch (m)
        {
        case TubeModel::FLAT:
            return "FLAT";
        case TubeModel::AX7:
            return "12AX7";
        case TubeModel::AT7:
            return "12AT7";
        case TubeModel::AU7:
            return "12AU7";
        case TubeModel::EL34:
            return "EL34";
        case TubeModel::P6L6:
            return "6L6";
        default:
            return "FLAT";
        }
    }

    inline TubeModel strToTubeModel(const std::string &s)
    {
        if (s == "12AT7")
            return TubeModel::AT7;
        if (s == "12AU7")
            return TubeModel::AU7;
        if (s == "EL34")
            return TubeModel::EL34;
        if (s == "6L6")
            return TubeModel::P6L6;
        if (s == "12AX7")
            return TubeModel::AX7;
        return TubeModel::FLAT;
    }

    inline const char *modSrcToStr(ObsSynthSrc src)
    {
        switch (src)
        {
        case ObsSynthSrc::LFO1:
            return "LFO1";
        case ObsSynthSrc::LFO2:
            return "LFO2";
        case ObsSynthSrc::LFO3:
            return "LFO3";
        case ObsSynthSrc::LFO4:
            return "LFO4";
        case ObsSynthSrc::VELOCITY:
            return "VELOCITY";
        case ObsSynthSrc::AFTERTOUCH:
            return "AFTERTOUCH";
        case ObsSynthSrc::MOD_WHEEL:
            return "MOD_WHEEL";
        case ObsSynthSrc::PITCH_BEND:
            return "PITCH_BEND";
        case ObsSynthSrc::XY_X:
            return "XY_X";
        case ObsSynthSrc::XY_Y:
            return "XY_Y";
        case ObsSynthSrc::EXPRESSION:
            return "EXPRESSION";
        case ObsSynthSrc::KEYTRACK:
            return "KEYTRACK";
        case ObsSynthSrc::RANDOM:
            return "RANDOM";
        case ObsSynthSrc::MOD_ENV:
            return "MOD_ENV";
        case ObsSynthSrc::MOD_ENV2:
            return "MOD_ENV2";
        case ObsSynthSrc::MOD_ENV3:
            return "MOD_ENV3";
        case ObsSynthSrc::CC_A:
            return "CC_A";
        case ObsSynthSrc::CC_B:
            return "CC_B";
        case ObsSynthSrc::CC_C:
            return "CC_C";
        case ObsSynthSrc::CC_D:
            return "CC_D";
        default:
            return "LFO1";
        }
    }

    inline ObsSynthSrc strToModSrc(const std::string &s)
    {
        if (s == "LFO1")
            return ObsSynthSrc::LFO1;
        if (s == "LFO2")
            return ObsSynthSrc::LFO2;
        if (s == "LFO3")
            return ObsSynthSrc::LFO3;
        if (s == "LFO4")
            return ObsSynthSrc::LFO4;
        if (s == "VELOCITY")
            return ObsSynthSrc::VELOCITY;
        if (s == "AFTERTOUCH")
            return ObsSynthSrc::AFTERTOUCH;
        if (s == "MOD_WHEEL")
            return ObsSynthSrc::MOD_WHEEL;
        if (s == "PITCH_BEND")
            return ObsSynthSrc::PITCH_BEND;
        if (s == "XY_X")
            return ObsSynthSrc::XY_X;
        if (s == "XY_Y")
            return ObsSynthSrc::XY_Y;
        if (s == "EXPRESSION")
            return ObsSynthSrc::EXPRESSION;
        if (s == "KEYTRACK")
            return ObsSynthSrc::KEYTRACK;
        if (s == "RANDOM")
            return ObsSynthSrc::RANDOM;
        if (s == "MOD_ENV")
            return ObsSynthSrc::MOD_ENV;
        if (s == "MOD_ENV2")
            return ObsSynthSrc::MOD_ENV2;
        if (s == "MOD_ENV3")
            return ObsSynthSrc::MOD_ENV3;
        if (s == "CC_A")
            return ObsSynthSrc::CC_A;
        if (s == "CC_B")
            return ObsSynthSrc::CC_B;
        if (s == "CC_C")
            return ObsSynthSrc::CC_C;
        if (s == "CC_D")
            return ObsSynthSrc::CC_D;
        return ObsSynthSrc::LFO1;
    }

    inline const char *modDstToStr(ObsSynthDst d)
    {
        switch (d)
        {
        case ObsSynthDst::OSC1_PITCH:
            return "OSC1_PITCH";
        case ObsSynthDst::OSC1_CUTOFF:
            return "OSC1_CUTOFF";
        case ObsSynthDst::OSC1_RESO:
            return "OSC1_RESO";
        case ObsSynthDst::OSC1_LEVEL:
            return "OSC1_LEVEL";
        case ObsSynthDst::OSC2_PITCH:
            return "OSC2_PITCH";
        case ObsSynthDst::OSC2_CUTOFF:
            return "OSC2_CUTOFF";
        case ObsSynthDst::OSC2_RESO:
            return "OSC2_RESO";
        case ObsSynthDst::OSC2_LEVEL:
            return "OSC2_LEVEL";
        case ObsSynthDst::OSC3_PITCH:
            return "OSC3_PITCH";
        case ObsSynthDst::OSC3_CUTOFF:
            return "OSC3_CUTOFF";
        case ObsSynthDst::OSC3_RESO:
            return "OSC3_RESO";
        case ObsSynthDst::OSC3_LEVEL:
            return "OSC3_LEVEL";
        case ObsSynthDst::OSC4_PITCH:
            return "OSC4_PITCH";
        case ObsSynthDst::OSC4_CUTOFF:
            return "OSC4_CUTOFF";
        case ObsSynthDst::OSC4_RESO:
            return "OSC4_RESO";
        case ObsSynthDst::OSC4_LEVEL:
            return "OSC4_LEVEL";
        case ObsSynthDst::MIX_CH1_LVL:
            return "MIX_CH1_LVL";
        case ObsSynthDst::MIX_CH2_LVL:
            return "MIX_CH2_LVL";
        case ObsSynthDst::MIX_CH3_LVL:
            return "MIX_CH3_LVL";
        case ObsSynthDst::MIX_CH4_LVL:
            return "MIX_CH4_LVL";
        case ObsSynthDst::MFX_MIX:
            return "MFX_MIX";
        case ObsSynthDst::MFX_P1:
            return "MFX_P1";
        case ObsSynthDst::MFX_P2:
            return "MFX_P2";
        case ObsSynthDst::LFO1_RATE:
            return "LFO1_RATE";
        case ObsSynthDst::LFO2_RATE:
            return "LFO2_RATE";
        case ObsSynthDst::LFO1_DEPTH:
            return "LFO1_DEPTH";
        case ObsSynthDst::LFO2_DEPTH:
            return "LFO2_DEPTH";
        case ObsSynthDst::TUBE_DRIVE:
            return "TUBE_DRIVE";
        case ObsSynthDst::TUBE_ASYM:
            return "TUBE_ASYM";
        case ObsSynthDst::TUBE_CHAOS:
            return "TUBE_CHAOS";
        case ObsSynthDst::OSC1_WT_MORPH:
            return "OSC1_WT_MORPH";
        case ObsSynthDst::OSC2_WT_MORPH:
            return "OSC2_WT_MORPH";
        case ObsSynthDst::OSC3_WT_MORPH:
            return "OSC3_WT_MORPH";
        case ObsSynthDst::OSC4_WT_MORPH:
            return "OSC4_WT_MORPH";
        default:
            return nullptr; // no name: written as DST_<number> (it used to become OSC1_PITCH)
        }
    }

    inline ObsSynthDst strToModDst(const std::string &s)
    {
        if (s.rfind("DST_", 0) == 0)
            return static_cast<ObsSynthDst>(std::atoi(s.c_str() + 4));
        if (s == "OSC1_PITCH")
            return ObsSynthDst::OSC1_PITCH;
        if (s == "OSC1_CUTOFF")
            return ObsSynthDst::OSC1_CUTOFF;
        if (s == "OSC1_RESO")
            return ObsSynthDst::OSC1_RESO;
        if (s == "OSC1_LEVEL")
            return ObsSynthDst::OSC1_LEVEL;
        if (s == "OSC2_PITCH")
            return ObsSynthDst::OSC2_PITCH;
        if (s == "OSC2_CUTOFF")
            return ObsSynthDst::OSC2_CUTOFF;
        if (s == "OSC2_RESO")
            return ObsSynthDst::OSC2_RESO;
        if (s == "OSC2_LEVEL")
            return ObsSynthDst::OSC2_LEVEL;
        if (s == "OSC3_PITCH")
            return ObsSynthDst::OSC3_PITCH;
        if (s == "OSC3_CUTOFF")
            return ObsSynthDst::OSC3_CUTOFF;
        if (s == "OSC3_RESO")
            return ObsSynthDst::OSC3_RESO;
        if (s == "OSC3_LEVEL")
            return ObsSynthDst::OSC3_LEVEL;
        if (s == "OSC4_PITCH")
            return ObsSynthDst::OSC4_PITCH;
        if (s == "OSC4_CUTOFF")
            return ObsSynthDst::OSC4_CUTOFF;
        if (s == "OSC4_RESO")
            return ObsSynthDst::OSC4_RESO;
        if (s == "OSC4_LEVEL")
            return ObsSynthDst::OSC4_LEVEL;
        if (s == "MIX_CH1_LVL")
            return ObsSynthDst::MIX_CH1_LVL;
        if (s == "MIX_CH2_LVL")
            return ObsSynthDst::MIX_CH2_LVL;
        if (s == "MIX_CH3_LVL")
            return ObsSynthDst::MIX_CH3_LVL;
        if (s == "MIX_CH4_LVL")
            return ObsSynthDst::MIX_CH4_LVL;
        if (s == "MFX_MIX")
            return ObsSynthDst::MFX_MIX;
        if (s == "MFX_P1")
            return ObsSynthDst::MFX_P1;
        if (s == "MFX_P2")
            return ObsSynthDst::MFX_P2;
        if (s == "LFO1_RATE")
            return ObsSynthDst::LFO1_RATE;
        if (s == "LFO2_RATE")
            return ObsSynthDst::LFO2_RATE;
        if (s == "LFO1_DEPTH")
            return ObsSynthDst::LFO1_DEPTH;
        if (s == "LFO2_DEPTH")
            return ObsSynthDst::LFO2_DEPTH;
        if (s == "TUBE_DRIVE")
            return ObsSynthDst::TUBE_DRIVE;
        if (s == "TUBE_ASYM")
            return ObsSynthDst::TUBE_ASYM;
        if (s == "TUBE_CHAOS")
            return ObsSynthDst::TUBE_CHAOS;
        if (s == "OSC1_WT_MORPH")
            return ObsSynthDst::OSC1_WT_MORPH;
        if (s == "OSC2_WT_MORPH")
            return ObsSynthDst::OSC2_WT_MORPH;
        if (s == "OSC3_WT_MORPH")
            return ObsSynthDst::OSC3_WT_MORPH;
        if (s == "OSC4_WT_MORPH")
            return ObsSynthDst::OSC4_WT_MORPH;
        return ObsSynthDst::OSC1_PITCH;
    }

    // ============ Preset4D -> JSON ============

    inline json presetToJson(const Preset4D &p)
    {
        json j;
        j["name"] = p.name;

        // Waves as string array
        json waves = json::array();
        for (int i = 0; i < PRESET_NUM_OSCS; ++i)
            waves.push_back(waveTypeToStr(p.waves[i]));
        j["waves"] = waves;

        // Float arrays
        j["levels"] = std::vector<float>(p.levels, p.levels + PRESET_NUM_OSCS);
        j["pans"] = std::vector<float>(p.pans, p.pans + PRESET_NUM_OSCS);
        j["cutoffs"] = std::vector<float>(p.cutoffs, p.cutoffs + PRESET_NUM_OSCS);
        j["resonances"] = std::vector<float>(p.resonances, p.resonances + PRESET_NUM_OSCS);
        j["filterEnvAmounts"] = std::vector<float>(p.filterEnvAmounts, p.filterEnvAmounts + PRESET_NUM_OSCS);
        j["coarseTune"] = std::vector<float>(p.coarseTune, p.coarseTune + PRESET_NUM_OSCS);

        j["ampA"] = std::vector<float>(p.ampA, p.ampA + PRESET_NUM_OSCS);
        j["ampD"] = std::vector<float>(p.ampD, p.ampD + PRESET_NUM_OSCS);
        j["ampS"] = std::vector<float>(p.ampS, p.ampS + PRESET_NUM_OSCS);
        j["ampR"] = std::vector<float>(p.ampR, p.ampR + PRESET_NUM_OSCS);

        j["fltA"] = std::vector<float>(p.fltA, p.fltA + PRESET_NUM_OSCS);
        j["fltD"] = std::vector<float>(p.fltD, p.fltD + PRESET_NUM_OSCS);
        j["fltS"] = std::vector<float>(p.fltS, p.fltS + PRESET_NUM_OSCS);
        j["fltR"] = std::vector<float>(p.fltR, p.fltR + PRESET_NUM_OSCS);

        // LFOs
        json lfos = json::array();
        for (int i = 0; i < PRESET_NUM_LFOS; ++i)
        {
            lfos.push_back({{"rate", p.lfos[i].rate},
                            {"wave", lfoWaveToStr(p.lfos[i].wave)}});
        }
        j["lfos"] = lfos;

        // Mod Envelopes
        json modEnvs = json::array();
        for (int i = 0; i < PRESET_NUM_MOD_ENVS; ++i)
        {
            modEnvs.push_back({{"attack", p.modEnvs[i].attack},
                               {"decay", p.modEnvs[i].decay},
                               {"sustain", p.modEnvs[i].sustain},
                               {"release", p.modEnvs[i].release}});
        }
        j["modEnvs"] = modEnvs;

        // Delay
        j["delay"] = {
            {"timeL", p.delay.timeL},
            {"timeR", p.delay.timeR},
            {"feedback", p.delay.feedback},
            {"mix", p.delay.mix},
            {"damping", p.delay.damping},
            {"pingPong", p.delay.pingPong}};

        // Tube
        j["tube"] = {
            {"model", tubeModelToStr(p.tube.model)},
            {"drive", p.tube.drive},
            {"asymmetry", p.tube.asymmetry},
            {"chaos", p.tube.chaos},
            {"mix", p.tube.mix}};

        // Mod routes
        json routes = json::array();
        for (int i = 0; i < p.numRoutes; ++i)
        {
            routes.push_back({{"src", modSrcToStr(p.routes[i].src)},
                              {"dst", modDstToStr(p.routes[i].dst) ? std::string(modDstToStr(p.routes[i].dst))
                                                                   : "DST_" + std::to_string(static_cast<int>(p.routes[i].dst))},
                              {"amount", p.routes[i].amount}});
        }
        j["routes"] = routes;

        // Master filter
        // each oscillator's engine, its wavetable position and its FM: without them a WT or FM sound came back
        // as plain oscillators
        json parts = json::array(), morph = json::array(), fm = json::array();
        for (int i = 0; i < PRESET_NUM_OSCS; ++i)
        {
            parts.push_back(static_cast<int>(p.partTypes[i]));
            morph.push_back(p.wtMorphPos[i]);
            const auto &f = p.fmParams[i];
            fm.push_back({{"algorithm", static_cast<int>(f.algorithm)}, {"modRatio", f.modRatio}, {"modIndex", f.modIndex},
                          {"feedback", f.feedback}, {"op2Ratio", f.op2Ratio}, {"op2Level", f.op2Level}});
        }
        j["partTypes"] = parts;
        json path = json::array();
        for (int i = 0; i < p.vector.count && i < 16; ++i)
            path.push_back({p.vector.path[i].t, p.vector.path[i].x, p.vector.path[i].y});
        j["vector"] = {{"on", p.vector.on}, {"loop", p.vector.loop}, {"path", path}};
        json grain = json::array();
        for (int i = 0; i < PRESET_NUM_OSCS; ++i)
            grain.push_back({{"position", p.grain[i].position}, {"size", p.grain[i].size}, {"density", p.grain[i].density}, {"spray", p.grain[i].spray}});
        j["grain"] = grain;
        json samples = json::array();
        for (int i = 0; i < PRESET_NUM_OSCS; ++i)
            samples.push_back(p.grainSample[i] ? std::string(p.grainSample[i]) : std::string());
        j["grainSample"] = samples;
        j["wtMorphPos"] = morph;
        j["fm"] = fm;
        j["masterFilterCutoff"] = p.masterFilterCutoff;
        j["masterFilterResonance"] = p.masterFilterResonance;

        return j;
    }

    // ============ JSON -> Preset4D ============

    inline bool jsonToPreset(const json &j, Preset4D &p)
    {
        // Static buffer for name
        static thread_local char nameBuf[256];

        try
        {
            // every field zeroed, the defaulted ones too: what the JSON leaves out stays 0 (memset is safe: the type is
            // trivially copyable; its default member initializers only make it non-trivial to construct)
            static_assert(std::is_trivially_copyable_v<Preset4D>, "jsonToPreset zeroes Preset4D with memset");
            std::memset(static_cast<void *>(&p), 0, sizeof(Preset4D));

            // Name
            std::string name = j.value("name", "Untitled");
            std::strncpy(nameBuf, name.c_str(), sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            p.name = nameBuf;

            // Waves
            if (j.contains("waves"))
            {
                auto &arr = j["waves"];
                for (int i = 0; i < PRESET_NUM_OSCS && i < (int)arr.size(); ++i)
                    p.waves[i] = strToWaveType(arr[i].get<std::string>());
            }

            // Float arrays helper
            auto readFloats = [&](const char *key, float *out)
            {
                if (j.contains(key))
                {
                    auto &arr = j[key];
                    for (int i = 0; i < PRESET_NUM_OSCS && i < (int)arr.size(); ++i)
                        out[i] = arr[i].get<float>();
                }
            };

            readFloats("levels", p.levels);
            readFloats("pans", p.pans);
            readFloats("cutoffs", p.cutoffs);
            readFloats("resonances", p.resonances);
            readFloats("filterEnvAmounts", p.filterEnvAmounts);
            readFloats("coarseTune", p.coarseTune);
            readFloats("ampA", p.ampA);
            readFloats("ampD", p.ampD);
            readFloats("ampS", p.ampS);
            readFloats("ampR", p.ampR);
            readFloats("fltA", p.fltA);
            readFloats("fltD", p.fltD);
            readFloats("fltS", p.fltS);
            readFloats("fltR", p.fltR);

            // LFOs
            if (j.contains("lfos"))
            {
                auto &arr = j["lfos"];
                for (int i = 0; i < PRESET_NUM_LFOS && i < (int)arr.size(); ++i)
                {
                    p.lfos[i].rate = arr[i].value("rate", 1.0f);
                    p.lfos[i].wave = strToLFOWave(arr[i].value("wave", "SINE"));
                }
            }

            // Mod Envelopes
            if (j.contains("modEnvs"))
            {
                auto &arr = j["modEnvs"];
                for (int i = 0; i < PRESET_NUM_MOD_ENVS && i < (int)arr.size(); ++i)
                {
                    p.modEnvs[i].attack = arr[i].value("attack", 0.005f);
                    p.modEnvs[i].decay = arr[i].value("decay", 0.1f);
                    p.modEnvs[i].sustain = arr[i].value("sustain", 0.0f);
                    p.modEnvs[i].release = arr[i].value("release", 0.3f);
                }
            }

            // Delay
            if (j.contains("delay"))
            {
                auto &d = j["delay"];
                p.delay.timeL = d.value("timeL", 0.375f);
                p.delay.timeR = d.value("timeR", 0.5f);
                p.delay.feedback = d.value("feedback", 0.35f);
                p.delay.mix = d.value("mix", 0.25f);
                p.delay.damping = d.value("damping", 0.4f);
                p.delay.pingPong = d.value("pingPong", true);
            }

            // Tube
            if (j.contains("tube"))
            {
                auto &t = j["tube"];
                p.tube.model = strToTubeModel(t.value("model", "12AX7"));
                p.tube.drive = t.value("drive", 1.8f);
                p.tube.asymmetry = t.value("asymmetry", 0.3f);
                p.tube.chaos = t.value("chaos", 0.0f);
                p.tube.mix = t.value("mix", 0.4f);
            }

            // Mod routes
            if (j.contains("routes"))
            {
                auto &arr = j["routes"];
                p.numRoutes = 0;
                for (int i = 0; i < (int)arr.size() && i < PRESET_MAX_ROUTES; ++i)
                {
                    p.routes[i].src = strToModSrc(arr[i].value("src", "LFO1"));
                    p.routes[i].dst = strToModDst(arr[i].value("dst", "OSC1_PITCH"));
                    p.routes[i].amount = arr[i].value("amount", 0.0f);
                    p.numRoutes++;
                }
            }

            // Master filter
            p.masterFilterCutoff = j.value("masterFilterCutoff", 20000.0f);
            p.masterFilterResonance = j.value("masterFilterResonance", 0.0f);

            // engines, wavetable positions, FM (files from before them: plain oscillators, FM's defaults)
            p.tubeReverb = TubeReverbConfig{1.2f, 0.92f, 0.0f};
            for (int i = 0; i < PRESET_NUM_OSCS; ++i)
            {
                p.partTypes[i] = PartType::STANDARD_OSC;
                p.wtMorphPos[i] = 0.0f;
                p.fmParams[i] = Preset4D::FMPresetParams{};
            }
            p.vector = VectorConfig{};
            for (int i = 0; i < PRESET_NUM_OSCS; ++i)
                p.grain[i] = Preset4D::GrainParams{};
            static thread_local char sampleBuf[PRESET_NUM_OSCS][160];
            for (int i = 0; i < PRESET_NUM_OSCS; ++i)
                p.grainSample[i] = nullptr;
            if (j.contains("grainSample"))
                for (int i = 0; i < PRESET_NUM_OSCS && i < (int)j["grainSample"].size(); ++i)
                {
                    const std::string f = j["grainSample"][i].get<std::string>();
                    if (f.empty())
                        continue;
                    std::strncpy(sampleBuf[i], f.c_str(), sizeof sampleBuf[i] - 1);
                    sampleBuf[i][sizeof sampleBuf[i] - 1] = '\0';
                    p.grainSample[i] = sampleBuf[i];
                }
            if (j.contains("grain"))
                for (int i = 0; i < PRESET_NUM_OSCS && i < (int)j["grain"].size(); ++i)
                {
                    const auto &g = j["grain"][i];
                    p.grain[i] = {g.value("position", 0.3f), g.value("size", 0.08f), g.value("density", 25.0f), g.value("spray", 0.1f)};
                }
            if (j.contains("vector"))
            {
                const auto &v = j["vector"];
                p.vector.on = v.value("on", false);
                p.vector.loop = v.value("loop", false);
                if (v.contains("path"))
                    for (const auto &pt : v["path"])
                        if (p.vector.count < 16 && pt.is_array() && pt.size() == 3)
                            p.vector.path[p.vector.count++] = {pt[0].get<float>(), pt[1].get<float>(), pt[2].get<float>()};
            }
            if (j.contains("partTypes"))
                for (int i = 0; i < PRESET_NUM_OSCS && i < (int)j["partTypes"].size(); ++i)
                    p.partTypes[i] = static_cast<PartType>(std::clamp(j["partTypes"][i].get<int>(), 0, 3));
            if (j.contains("wtMorphPos"))
                for (int i = 0; i < PRESET_NUM_OSCS && i < (int)j["wtMorphPos"].size(); ++i)
                    p.wtMorphPos[i] = j["wtMorphPos"][i].get<float>();
            if (j.contains("fm"))
                for (int i = 0; i < PRESET_NUM_OSCS && i < (int)j["fm"].size(); ++i)
                {
                    const auto &f = j["fm"][i];
                    auto &o = p.fmParams[i];
                    o.algorithm = static_cast<FMAlgorithm>(std::clamp(f.value("algorithm", 1), 0, 4));
                    o.modRatio = f.value("modRatio", 1.0f);
                    o.modIndex = f.value("modIndex", 1.0f);
                    o.feedback = f.value("feedback", 0.0f);
                    o.op2Ratio = f.value("op2Ratio", 2.0f);
                    o.op2Level = f.value("op2Level", 0.5f);
                }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[PresetIO] Parse error: " << e.what() << std::endl;
            return false;
        }
    }

    // ============ File I/O ============

    inline bool savePresetToFile(const Preset4D &preset, const std::string &filepath)
    {
        try
        {
            json j = presetToJson(preset);
            std::ofstream file(filepath);
            if (!file.is_open())
                return false;
            file << j.dump(2);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[PresetIO] Save error: " << e.what() << std::endl;
            return false;
        }
    }

    inline bool loadPresetFromFile(const std::string &filepath, Preset4D &preset)
    {
        try
        {
            std::ifstream file(filepath);
            if (!file.is_open())
                return false;
            json j = json::parse(file);
            return jsonToPreset(j, preset);
        }
        catch (const std::exception &e)
        {
            std::cerr << "[PresetIO] Load error: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace synth
