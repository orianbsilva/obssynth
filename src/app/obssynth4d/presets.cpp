#include "presets.hpp"
#include "parts.hpp"
#include "../common/data_home.hpp"
#include "../common/sample_find.hpp"

#include "../../products/obs_synth/obs_synth_preset_bank.hpp"
#include "../../products/obs_synth/obs_synth_preset_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>

namespace obs
{
    namespace fs = std::filesystem;

    std::string userPresetDir()
    {
        return synth::dataHome() + "/obssynth4d/presets";
    }

    std::string samplesDir()
    {
        const std::string presets = userPresetDir();
        return presets.substr(0, presets.rfind('/')) + "/samples";
    }

    std::vector<std::string> listSamples()
    {
        std::vector<std::string> v = synth::listSampleNames(samplesDir()); // the person's, then the shipped ones
        std::sort(v.begin(), v.end());
        return v;
    }

    std::string samplePath(const std::string &file)
    {
        return synth::findSample(file, samplesDir());
    }

    std::vector<PresetEntry> listPresets()
    {
        std::vector<PresetEntry> v;
        for (int i = 0; i < synth::PRESET_BANK_SIZE; ++i)
            v.push_back({synth::PRESET_BANK[i]->name, i, {}});
        std::vector<PresetEntry> files;
        std::error_code ec;
        for (const auto &f : fs::directory_iterator(userPresetDir(), ec))
            if (f.path().extension() == ".json")
            {
                synth::Preset4D p{};
                std::string name = f.path().stem().string();
                if (synth::loadPresetFromFile(f.path().string(), p) && p.name && *p.name)
                    name = p.name;
                files.push_back({name, -1, f.path().string()});
            }
        std::sort(files.begin(), files.end(), [](const PresetEntry &a, const PresetEntry &b) { return a.name < b.name; });
        v.insert(v.end(), files.begin(), files.end());
        return v;
    }

    bool loadPreset(const PresetEntry &e, const int tables[4], PresetData &d)
    {
        synth::Preset4D p{};
        if (e.bank >= 0 && e.bank < synth::PRESET_BANK_SIZE)
            p = *synth::PRESET_BANK[e.bank];
        else if (!synth::loadPresetFromFile(e.path, p))
            return false;

        const float none = std::numeric_limits<float>::quiet_NaN();
        for (auto &o : d.osc)
            std::fill(std::begin(o), std::end(o), none);
        std::fill(std::begin(d.glb), std::end(d.glb), none);
        d.name = p.name ? p.name : e.name;
        for (int o = 0; o < 4; ++o)
        {
            float *v = d.osc[o];
            const int wave = static_cast<int>(p.waves[o]);
            v[OSC_WAVE] = static_cast<float>(wave <= 5 ? wave : 1); // the ramps: as a saw, the panel has no ramp
            v[OSC_TUNE] = std::round(p.coarseTune[o]);
            v[OSC_FINE_TUNE] = p.coarseTune[o] - std::round(p.coarseTune[o]);
            v[OSC_CUTOFF] = p.cutoffs[o];
            v[OSC_RESO] = p.resonances[o];
            v[OSC_FILT_ENV_AMT] = p.filterEnvAmounts[o];
            v[OSC_FILT_ATK] = p.fltA[o];
            v[OSC_FILT_DEC] = p.fltD[o];
            v[OSC_FILT_SUS] = p.fltS[o];
            v[OSC_FILT_REL] = p.fltR[o];
            v[OSC_AMP_ATK] = p.ampA[o];
            v[OSC_AMP_DEC] = p.ampD[o];
            v[OSC_AMP_SUS] = p.ampS[o];
            v[OSC_AMP_REL] = p.ampR[o];
            v[OSC_LEVEL] = p.levels[o];
            v[OSC_PAN] = p.pans[o] * 2 - 1; // the engine's 0..1, the panel's -1..1
            v[OSC_PART_TYPE] = placeOf(p.partTypes[o]);
            v[OSC_WT_MORPH] = p.partTypes[o] == synth::PartType::GRANULAR || p.partTypes[o] == synth::PartType::SUPER ? p.grain[o].position
                              : tables[o] > 1                         ? p.wtMorphPos[o] / static_cast<float>(tables[o] - 1)
                                                                      : 0;
            v[OSC_GR_SIZE] = p.grain[o].size;
            v[OSC_GR_DENSITY] = p.grain[o].density;
            v[OSC_GR_SPRAY] = p.grain[o].spray;
            v[OSC_FM_ALGORITHM] = static_cast<float>(p.fmParams[o].algorithm);
            v[OSC_FM_MOD_RATIO] = p.fmParams[o].modRatio;
            v[OSC_FM_MOD_INDEX] = p.fmParams[o].modIndex;
            v[OSC_FM_FEEDBACK] = p.fmParams[o].feedback;
            v[OSC_FM_OP2_RATIO] = p.fmParams[o].op2Ratio;
            v[OSC_FM_OP2_LEVEL] = p.fmParams[o].op2Level;
        }
        for (int l = 0; l < 4; ++l)
        {
            d.glb[GLB_LFO1_RATE + l] = p.lfos[l].rate;
            d.glb[GLB_LFO1_WAVE + l] = static_cast<float>(p.lfos[l].wave);
        }
        d.glb[GLB_DLY_L] = p.delay.timeL;
        d.glb[GLB_DLY_R] = p.delay.timeR;
        d.glb[GLB_DLY_FB] = p.delay.feedback;
        d.glb[GLB_DLY_MIX] = p.delay.mix;
        d.glb[GLB_DLY_DMP] = p.delay.damping;
        d.glb[GLB_DLY_PING] = p.delay.pingPong ? 1.0f : 0.0f;
        d.glb[GLB_TUBE_DRV] = p.tube.drive;
        d.glb[GLB_TUBE_ASYM] = p.tube.asymmetry;
        d.glb[GLB_TUBE_MIX] = p.tube.mix;
        d.glb[GLB_MF_CUTOFF] = p.masterFilterCutoff;
        d.glb[GLB_MF_RESO] = p.masterFilterResonance;
        d.routes.clear();
        for (int i = 0; i < p.numRoutes && i < synth::PRESET_MAX_ROUTES; ++i)
            d.routes.push_back({static_cast<int>(p.routes[i].src), static_cast<int>(p.routes[i].dst), p.routes[i].amount});
        for (int m = 0; m < 3; ++m)
        {
            d.modEnv[m][0] = p.modEnvs[m].attack;
            d.modEnv[m][1] = p.modEnvs[m].decay;
            d.modEnv[m][2] = p.modEnvs[m].sustain;
            d.modEnv[m][3] = p.modEnvs[m].release;
        }
        d.tubeModel = static_cast<int>(p.tube.model);
        d.tubeChaos = p.tube.chaos;
        for (int o = 0; o < 4; ++o)
            d.grainFile[static_cast<size_t>(o)] = p.grainSample[o] ? p.grainSample[o] : "";
        d.vecOn = p.vector.on;
        d.vecLoop = p.vector.loop;
        d.vecPath.clear();
        for (int i = 0; i < p.vector.count && i < 16; ++i)
            d.vecPath.push_back({p.vector.path[i].t, p.vector.path[i].x, p.vector.path[i].y});
        return true;
    }

    // a file name from a preset's name: letters, digits and dashes
    static std::string slug(const std::string &name)
    {
        std::string s;
        for (const char c : name)
        {
            if (std::isalnum(static_cast<unsigned char>(c)))
                s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            else if (!s.empty() && s.back() != '-')
                s += '-';
        }
        while (!s.empty() && s.back() == '-')
            s.pop_back();
        return s.empty() ? "preset" : s;
    }

    bool savePreset(const PresetData &d, const int tables[4], std::string &path)
    {
        synth::Preset4D p{};
        p.name = d.name.c_str();
        for (int o = 0; o < 4; ++o)
        {
            const float *v = d.osc[o];
            p.waves[o] = static_cast<synth::WaveType>(static_cast<int>(std::lround(v[OSC_WAVE])));
            p.coarseTune[o] = v[OSC_TUNE] + v[OSC_FINE_TUNE];
            p.cutoffs[o] = v[OSC_CUTOFF];
            p.resonances[o] = v[OSC_RESO];
            p.filterEnvAmounts[o] = v[OSC_FILT_ENV_AMT];
            p.fltA[o] = v[OSC_FILT_ATK];
            p.fltD[o] = v[OSC_FILT_DEC];
            p.fltS[o] = v[OSC_FILT_SUS];
            p.fltR[o] = v[OSC_FILT_REL];
            p.ampA[o] = v[OSC_AMP_ATK];
            p.ampD[o] = v[OSC_AMP_DEC];
            p.ampS[o] = v[OSC_AMP_SUS];
            p.ampR[o] = v[OSC_AMP_REL];
            p.levels[o] = v[OSC_LEVEL];
            p.pans[o] = (v[OSC_PAN] + 1) / 2;
            p.partTypes[o] = partTypeAt(v[OSC_PART_TYPE]);
            p.wtMorphPos[o] = v[OSC_WT_MORPH] * static_cast<float>(tables[o] > 1 ? tables[o] - 1 : 0);
            p.grain[o] = {v[OSC_WT_MORPH], v[OSC_GR_SIZE], v[OSC_GR_DENSITY], v[OSC_GR_SPRAY]};
            p.fmParams[o].algorithm = static_cast<synth::FMAlgorithm>(static_cast<int>(std::lround(v[OSC_FM_ALGORITHM])));
            p.fmParams[o].modRatio = v[OSC_FM_MOD_RATIO];
            p.fmParams[o].modIndex = v[OSC_FM_MOD_INDEX];
            p.fmParams[o].feedback = v[OSC_FM_FEEDBACK];
            p.fmParams[o].op2Ratio = v[OSC_FM_OP2_RATIO];
            p.fmParams[o].op2Level = v[OSC_FM_OP2_LEVEL];
        }
        for (int l = 0; l < 4; ++l)
            p.lfos[l] = {d.glb[GLB_LFO1_RATE + l], static_cast<synth::LFOWaveType>(static_cast<int>(std::lround(d.glb[GLB_LFO1_WAVE + l])))};
        for (int m = 0; m < 3; ++m)
            p.modEnvs[m] = {d.modEnv[m][0], d.modEnv[m][1], d.modEnv[m][2], d.modEnv[m][3]};
        p.delay = {d.glb[GLB_DLY_L], d.glb[GLB_DLY_R], d.glb[GLB_DLY_FB], d.glb[GLB_DLY_MIX], d.glb[GLB_DLY_DMP], d.glb[GLB_DLY_PING] > 0.5f};
        p.tube = {static_cast<synth::TubeModel>(d.tubeModel), d.glb[GLB_TUBE_DRV], d.glb[GLB_TUBE_ASYM], d.tubeChaos, d.glb[GLB_TUBE_MIX]};
        p.numRoutes = 0;
        for (const PresetRoute &r : d.routes)
            if (p.numRoutes < synth::PRESET_MAX_ROUTES)
                p.routes[p.numRoutes++] = {static_cast<synth::ObsSynthSrc>(r.src), static_cast<synth::ObsSynthDst>(r.dst), r.amount};
        p.masterFilterCutoff = d.glb[GLB_MF_CUTOFF];
        p.masterFilterResonance = d.glb[GLB_MF_RESO];
        for (int o = 0; o < 4; ++o)
            p.grainSample[o] = d.grainFile[static_cast<size_t>(o)].empty() ? nullptr : d.grainFile[static_cast<size_t>(o)].c_str();
        p.vector.on = d.vecOn;
        p.vector.loop = d.vecLoop;
        p.vector.count = 0;
        for (const auto &pt : d.vecPath)
            if (p.vector.count < 16)
                p.vector.path[p.vector.count++] = {pt[0], pt[1], pt[2]};

        std::error_code ec;
        fs::create_directories(userPresetDir(), ec);
        path = userPresetDir() + "/" + slug(d.name) + ".json";
        return synth::savePresetToFile(p, path);
    }
} // namespace obs
