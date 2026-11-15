#pragma once
// The presets: the engine's bank (obs_synth_preset_bank.hpp) and the person's own, JSON files in
// ~/.local/share/obssynth4d/presets, in the engine's format (obs_synth_preset_io.hpp: synth_view reads them
// too). Here only reading and writing them as values in engine units; the panel turns them into its state,
// and the state reaches the engine the way a knob does (the queue), never from here.
#include "params.hpp"

#include <array>
#include <string>
#include <vector>

namespace obs
{
    struct PresetEntry
    {
        std::string name;
        int bank = -1;    // the engine's bank, or -1: a file
        std::string path; // the file
    };

    struct PresetRoute
    {
        int src, dst;
        float amount; // engine units, -2..2
    };

    struct PresetData
    {
        std::string name;
        float osc[4][OSC_PARAM_COUNT]; // engine units; NaN: the preset does not say
        float glb[GLB_PARAM_COUNT];
        std::vector<PresetRoute> routes;
        float modEnv[3][4]; // the mod envelopes' A D S R
        int tubeModel = 0;
        float tubeChaos = 0;
        bool vecOn = false, vecLoop = false; // vector synthesis: on, its path looping
        std::vector<std::array<float, 3>> vecPath; // t (s), x, y
        std::array<std::string, 4> grainFile;      // each granular oscillator's recording (a samples-folder name; "": the default)
    };

    std::string userPresetDir();
    std::string samplesDir();                // the recordings for the grains: ~/.local/share/obssynth4d/samples
    std::vector<std::string> listSamples(); // its WAV files, by name
    std::string samplePath(const std::string &file); // a name there, or a path as it is
    std::vector<PresetEntry> listPresets(); // the bank first, then the files by name
    // tables: each oscillator's wavetable count (the preset keeps a table, the panel 0..1)
    bool loadPreset(const PresetEntry &e, const int tables[4], PresetData &out);
    bool savePreset(const PresetData &d, const int tables[4], std::string &path); // into userPresetDir()
} // namespace obs
