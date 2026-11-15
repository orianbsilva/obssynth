#pragma once
#include "widgets/knob.hpp"
#include "widgets/fader.hpp"
#include "widgets/envelope.hpp"
#include "widgets/mod_matrix.hpp"

// ─── Per-oscillator parameter set ────────────────────────────────────────────
struct OscParams
{
    float tune = 0.0f;   // semitones  [-24, +24]
    float fine = 0.0f;   // cents      [-100, +100]
    float level = 0.75f; // [0, 1]
    float pan = 0.0f;    // [-1, +1]
    float wave = 0.0f;   // [0, 3] (sine/saw/sq/tri)

    // Filter ENV
    float f_attack = 0.01f;
    float f_decay = 0.30f;
    float f_sustain = 0.60f;
    float f_release = 0.50f;
    float f_cutoff = 0.80f; // normalized
    float f_res = 0.20f;

    // Amp ENV
    float a_attack = 0.005f;
    float a_decay = 0.20f;
    float a_sustain = 0.80f;
    float a_release = 0.40f;

    // Effects
    float chorus_rate = 0.30f;
    float chorus_depth = 0.20f;
    float chorus_mix = 0.15f;
};

// ─── Global / Master parameters ───────────────────────────────────────────────
struct MasterParams
{
    float delay_l_time = 0.375f;
    float delay_r_time = 0.500f;
    float delay_fb = 0.40f;
    float delay_mix = 0.20f;
    float delay_damping = 0.35f;

    float tube_drive = 0.30f;
    float tube_asym = 0.10f;
    float tube_mix = 0.50f;

    float master_vol = 0.85f;
    float master_pan = 0.0f;

    float mfx_p1 = 0.50f;
    float mfx_p2 = 0.30f;
    float mfx_p3 = 0.20f;
};

// ─── Full UI state ────────────────────────────────────────────────────────────
struct SynthUIState
{
    OscParams osc[4];
    MasterParams master;
    Widgets::ADSRParams filter_adsr[4]; // per-osc filter ADSR (display copy)
    Widgets::ADSRParams amp_adsr[4];    // per-osc amp ADSR (display copy)
    Widgets::ModMatrixState mod_matrix;

    bool show_mod_matrix = false;
    bool show_imgui_demo = false;

    // Sync osc[i] ADSR structs to filter_adsr / amp_adsr on init
    void Init();
};

// ─── Entry point called every frame ──────────────────────────────────────────
void DrawSynthUI(SynthUIState &state);
