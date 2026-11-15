/**
 * @file obs_synth_preset_bank.hpp
 * @brief Static preset bank for ObsSynth4D
 *
 * All factory presets defined as Preset4D structs.
 * New presets = new data, zero code changes.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include "obs_synth_preset.hpp"

namespace synth
{

    // ─── Init ────────────────────────────────────────────────────────────────
    // Clean slate: OSC1 sine only, all filters wide open, all FX bypassed/off.
    static constexpr Preset4D PRESET_INIT = {
        "Init",
        // waves: osc1 sine, rest sine (muted)
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        // levels: only osc1 active
        {1.0f, 0.0f, 0.0f, 0.0f},
        // pans: center
        {0.5f, 0.5f, 0.5f, 0.5f},
        // cutoffs: fully open
        {20000.0f, 20000.0f, 20000.0f, 20000.0f},
        // resonances: off
        {0.0f, 0.0f, 0.0f, 0.0f},
        // filterEnvAmounts: off
        {0.0f, 0.0f, 0.0f, 0.0f},
        // coarseTune: unison
        {0.0f, 0.0f, 0.0f, 0.0f},
        // ampA: instant attack
        {0.001f, 0.001f, 0.001f, 0.001f},
        // ampD
        {0.001f, 0.001f, 0.001f, 0.001f},
        // ampS: full sustain
        {1.0f, 1.0f, 1.0f, 1.0f},
        // ampR: short release
        {0.05f, 0.05f, 0.05f, 0.05f},
        // fltA
        {0.001f, 0.001f, 0.001f, 0.001f},
        // fltD
        {0.001f, 0.001f, 0.001f, 0.001f},
        // fltS
        {1.0f, 1.0f, 1.0f, 1.0f},
        // fltR
        {0.05f, 0.05f, 0.05f, 0.05f},
        // lfos: all off
        {
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs: all off
        {
            {0.001f, 0.001f, 0.0f, 0.05f},
            {0.001f, 0.001f, 0.0f, 0.05f},
            {0.001f, 0.001f, 0.0f, 0.05f},
        },
        // delay: fully off
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
        // tube: drive=1 (unity), mix=0 (bypassed)
        {TubeModel::AX7, 1.0f, 0.0f, 0.0f, 0.0f},
        // mod routes: none
        {},
        0,        // numRoutes
        20000.0f, // masterFilterCutoff: fully open
        0.0f,     // masterFilterResonance: off
    };

    // ─── Lush Pad ────────────────────────────────────────────────────────────
    static constexpr Preset4D PRESET_LUSH_PAD = {
        "Lush Poly Pad",
        // waves
        {WaveType::SAW, WaveType::SQUARE, WaveType::TRIANGLE, WaveType::SINE},
        // levels
        {0.7f, 0.5f, 0.75f, 0.3f},
        // pans
        {0.35f, 0.65f, 0.25f, 0.75f},
        // cutoffs
        {3000.0f, 2000.0f, 3500.0f, 5500.0f},
        // resonances
        {0.25f, 0.35f, 0.1f, 0.0f},
        // filterEnvAmounts
        {0.4f, 0.3f, 0.15f, 0.0f},
        // coarseTune
        {0.0f, 0.0f, 0.0f, -12.0f},
        // ampA, ampD, ampS, ampR
        {0.15f, 0.2f, 0.2f, 0.001f},
        {0.3f, 0.4f, 0.8f, 0.1f},
        {0.7f, 0.6f, 0.4f, 0.8f},
        {1.0f, 1.2f, 0.6f, 1.5f},
        // fltA, fltD, fltS, fltR
        {0.05f, 0.1f, 0.01f, 0.01f},
        {0.5f, 0.8f, 0.2f, 0.1f},
        {0.3f, 0.4f, 0.5f, 1.0f},
        {0.8f, 1.0f, 0.3f, 0.3f},
        // lfos
        {
            {2.0f, LFOWaveType::SINE},
            {0.8f, LFOWaveType::TRIANGLE},
            {0.3f, LFOWaveType::SINE},
            {5.0f, LFOWaveType::SAW},
        },
        // modEnvs (A, D, S, R)
        {
            {0.01f, 0.3f, 0.0f, 0.5f},
            {0.05f, 0.8f, 0.4f, 1.0f},
            {0.2f, 1.5f, 0.6f, 2.0f},
        },
        // delay
        {0.375f, 0.5f, 0.35f, 0.25f, 0.4f, true},
        // tube
        {TubeModel::AU7, 1.4f, 0.2f, 0.25f, 0.35f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.008f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_LEVEL, -0.15f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC1_CUTOFF, 0.4f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 0.7f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC2_CUTOFF, 0.7f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.5f},
            {ObsSynthSrc::XY_X, ObsSynthDst::MIX_CH1_LVL, 0.8f},
            {ObsSynthSrc::XY_X, ObsSynthDst::MIX_CH2_LVL, -0.8f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::TUBE_DRIVE, 0.3f},
        },
        9, // numRoutes
        10000.0f,
        0.0f // masterFilterCutoff, masterFilterResonance
    };

    static constexpr Preset4D PRESET_ELECTRIC_KEYS = {
        "Electric Keys",
        // waves
        {WaveType::SQUARE, WaveType::TRIANGLE, WaveType::SINE, WaveType::NOISE},
        // levels
        {0.6f, 0.7f, 0.5f, 0.15f},
        // pans
        {0.45f, 0.55f, 0.5f, 0.5f},
        // cutoffs
        {6000.0f, 4000.0f, 12000.0f, 3000.0f},
        // resonances
        {0.3f, 0.15f, 0.0f, 0.4f},
        // filterEnvAmounts
        {0.7f, 0.3f, 0.0f, 0.9f},
        // coarseTune
        {0.0f, 0.0f, 0.0f, -12.0f},
        // ampA, ampD, ampS, ampR
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.15f, 0.3f, 0.5f, 0.05f},
        {0.2f, 0.3f, 0.4f, 0.0f},
        {0.3f, 0.4f, 0.5f, 0.1f},
        // fltA, fltD, fltS, fltR
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.1f, 0.2f, 0.1f, 0.03f},
        {0.1f, 0.2f, 1.0f, 0.0f},
        {0.2f, 0.3f, 0.3f, 0.05f},
        // lfos
        {
            {2.0f, LFOWaveType::SINE},
            {0.8f, LFOWaveType::TRIANGLE},
            {0.3f, LFOWaveType::SINE},
            {5.0f, LFOWaveType::SAW},
        },
        // modEnvs
        {
            {0.01f, 0.3f, 0.0f, 0.5f},
            {0.05f, 0.8f, 0.4f, 1.0f},
            {0.2f, 1.5f, 0.6f, 2.0f},
        },
        // delay
        {0.375f, 0.5f, 0.35f, 0.25f, 0.4f, true},
        // tube
        {TubeModel::EL34, 2.2f, 0.4f, 0.0f, 0.3f},
        // mod routes
        {
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.8f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC2_CUTOFF, 0.6f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.006f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_PITCH, 0.04f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC4_LEVEL, -0.3f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::TUBE_DRIVE, 0.4f},
        },
        6 // numRoutes
    };

    static constexpr Preset4D PRESET_AGGRESSIVE_BASS = {
        "Aggressive Bass",
        // waves
        {WaveType::SAW, WaveType::SAW, WaveType::SQUARE, WaveType::SAW},
        // levels
        {0.8f, 0.7f, 0.6f, 0.5f},
        // pans
        {0.4f, 0.6f, 0.5f, 0.5f},
        // cutoffs
        {1200.0f, 1000.0f, 800.0f, 1500.0f},
        // resonances
        {0.5f, 0.55f, 0.6f, 0.45f},
        // filterEnvAmounts
        {0.4f, 0.35f, 0.45f, 0.25f},
        // coarseTune
        {0.0f, 0.0f, 0.0f, -12.0f},
        // ampA, ampD, ampS, ampR
        {0.005f, 0.005f, 0.005f, 0.01f},
        {0.2f, 0.25f, 0.15f, 0.3f},
        {0.6f, 0.55f, 0.5f, 0.4f},
        {0.2f, 0.2f, 0.15f, 0.25f},
        // fltA, fltD, fltS, fltR
        {0.005f, 0.005f, 0.005f, 0.01f},
        {0.25f, 0.3f, 0.2f, 0.35f},
        {0.2f, 0.25f, 0.15f, 0.3f},
        {0.15f, 0.15f, 0.1f, 0.2f},
        // lfos
        {
            {2.0f, LFOWaveType::SINE},
            {0.8f, LFOWaveType::TRIANGLE},
            {0.3f, LFOWaveType::SINE},
            {5.0f, LFOWaveType::SAW},
        },
        // modEnvs
        {
            {0.01f, 0.3f, 0.0f, 0.5f},
            {0.05f, 0.8f, 0.4f, 1.0f},
            {0.2f, 1.5f, 0.6f, 2.0f},
        },
        // delay
        {0.375f, 0.5f, 0.35f, 0.25f, 0.4f, true},
        // tube
        {TubeModel::AX7, 1.8f, 0.3f, 0.0f, 0.4f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_CUTOFF, 0.4f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC2_CUTOFF, 0.4f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC3_CUTOFF, 0.5f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC1_PITCH, 0.01f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.9f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC2_CUTOFF, 0.9f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::OSC1_CUTOFF, 0.6f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::OSC3_CUTOFF, 0.6f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::LFO1_DEPTH, 0.8f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::TUBE_DRIVE, 0.5f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::TUBE_DRIVE, 0.5f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::TUBE_ASYM, 0.3f},
        },
        12, // numRoutes
        4000.0f,
        0.15f // masterFilterCutoff, masterFilterResonance
    };

    static constexpr Preset4D PRESET_STACKED_FIFTHS_BASS = {
        "Stacked Fifths Bass",
        // waves: SAW root, SAW 5th, TRI double-5th, SINE sub
        {WaveType::SAW, WaveType::SAW, WaveType::TRIANGLE, WaveType::SINE},
        // levels
        {0.8f, 0.55f, 0.35f, 0.7f},
        // pans: root center, 5th slightly wide, upper wider, sub center
        {0.5f, 0.42f, 0.62f, 0.5f},
        // cutoffs: low for warmth, 5th brighter, upper open, sub wide open
        {900.0f, 1400.0f, 2200.0f, 6000.0f},
        // resonances
        {0.4f, 0.3f, 0.2f, 0.0f},
        // filterEnvAmounts
        {0.5f, 0.4f, 0.3f, 0.0f},
        // coarseTune: root, +7 (5th), +14 (9th/stacked 5th), -12 (sub)
        {0.0f, 7.0f, 14.0f, -12.0f},
        // ampA: fast attack for punch
        {0.008f, 0.012f, 0.02f, 0.005f},
        // ampD
        {0.25f, 0.3f, 0.4f, 0.2f},
        // ampS
        {0.7f, 0.5f, 0.35f, 0.8f},
        // ampR
        {0.3f, 0.35f, 0.4f, 0.25f},
        // fltA: slow filter open for warmth
        {0.02f, 0.03f, 0.04f, 0.01f},
        // fltD
        {0.35f, 0.4f, 0.5f, 0.1f},
        // fltS
        {0.3f, 0.35f, 0.4f, 1.0f},
        // fltR
        {0.25f, 0.3f, 0.35f, 0.1f},
        // lfos: slow vibrato, gentle filter movement, sub pulse, fast shimmer
        {
            {1.5f, LFOWaveType::SINE},
            {0.4f, LFOWaveType::TRIANGLE},
            {0.15f, LFOWaveType::SINE},
            {3.5f, LFOWaveType::SINE},
        },
        // modEnvs
        {
            {0.01f, 0.3f, 0.0f, 0.5f},
            {0.05f, 0.8f, 0.4f, 1.0f},
            {0.2f, 1.5f, 0.6f, 2.0f},
        },
        // delay: short slapback, low mix to keep it tight
        {0.18f, 0.24f, 0.25f, 0.15f, 0.6f, false},
        // tube: AU7 warm model, moderate drive
        {TubeModel::AU7, 1.6f, 0.15f, 0.0f, 0.45f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.005f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC2_PITCH, 0.005f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_CUTOFF, 0.3f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_CUTOFF, 0.25f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC4_LEVEL, 0.1f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.7f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC2_CUTOFF, 0.5f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::OSC1_CUTOFF, 0.4f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::TUBE_DRIVE, 0.3f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC3_LEVEL, 0.6f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::TUBE_DRIVE, 0.4f},
        },
        11, // numRoutes
        5000.0f,
        0.1f // masterFilterCutoff, masterFilterResonance
    };

    static constexpr Preset4D PRESET_MINOR_NINTH_CHORD = {
        "Minor Ninth Chord",
        // waves: SAW root, SQUARE m3, TRIANGLE 5th, SINE 9th
        {WaveType::SAW, WaveType::SQUARE, WaveType::TRIANGLE, WaveType::SINE},
        // levels: root dominant, thirds present, 5th support, 9th color
        {0.7f, 0.55f, 0.5f, 0.45f},
        // pans: spread wide for chord voicing
        {0.35f, 0.6f, 0.3f, 0.7f},
        // cutoffs: warm but open enough for clarity
        {2500.0f, 2000.0f, 3500.0f, 5000.0f},
        // resonances
        {0.2f, 0.25f, 0.15f, 0.1f},
        // filterEnvAmounts
        {0.4f, 0.35f, 0.2f, 0.1f},
        // coarseTune: root, +3 (minor 3rd), +7 (5th), +14 (9th)
        {0.0f, 3.0f, 7.0f, 14.0f},
        // ampA: pad-like slow attack
        {0.12f, 0.15f, 0.1f, 0.18f},
        // ampD
        {0.4f, 0.5f, 0.3f, 0.6f},
        // ampS
        {0.65f, 0.6f, 0.7f, 0.55f},
        // ampR
        {0.8f, 0.9f, 0.7f, 1.0f},
        // fltA: gentle filter open
        {0.08f, 0.1f, 0.06f, 0.12f},
        // fltD
        {0.6f, 0.7f, 0.5f, 0.8f},
        // fltS
        {0.4f, 0.35f, 0.5f, 0.6f},
        // fltR
        {0.7f, 0.8f, 0.6f, 0.9f},
        // lfos: slow vibrato, gentle pan/cutoff movement, subtle drift, shimmer
        {
            {1.2f, LFOWaveType::SINE},
            {0.3f, LFOWaveType::TRIANGLE},
            {0.08f, LFOWaveType::SINE},
            {4.0f, LFOWaveType::SINE},
        },
        // modEnvs
        {
            {0.01f, 0.3f, 0.0f, 0.5f},
            {0.05f, 0.8f, 0.4f, 1.0f},
            {0.2f, 1.5f, 0.6f, 2.0f},
        },
        // delay: medium stereo delay for space
        {0.33f, 0.45f, 0.3f, 0.2f, 0.5f, true},
        // tube: AT7 gentle warmth
        {TubeModel::AT7, 1.2f, 0.1f, 0.0f, 0.3f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.006f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC2_PITCH, 0.006f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC3_PITCH, 0.006f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC4_PITCH, 0.006f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_CUTOFF, 0.25f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_CUTOFF, 0.2f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC1_LEVEL, 0.08f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC4_LEVEL, 0.1f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 0.6f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC2_CUTOFF, 0.6f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.4f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::OSC4_LEVEL, 0.3f},
        },
        12 // numRoutes
    };

    // ============ Single-Oscillator Test Presets (for tube/FX visualization) ============

    static constexpr Preset4D PRESET_SINGLE_SINE = {
        "Single Sine",
        // waves
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        // levels (only osc1 active)
        {1.0f, 0.0f, 0.0f, 0.0f},
        // pans (center)
        {0.5f, 0.5f, 0.5f, 0.5f},
        // cutoffs (wide open)
        {20000.0f, 20000.0f, 20000.0f, 20000.0f},
        // resonances
        {0.0f, 0.0f, 0.0f, 0.0f},
        // filterEnvAmounts
        {0.0f, 0.0f, 0.0f, 0.0f},
        // coarseTune
        {0.0f, 0.0f, 0.0f, 0.0f},
        // ampA, ampD, ampS, ampR
        {0.005f, 0.005f, 0.005f, 0.005f},
        {0.1f, 0.1f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.3f, 0.3f, 0.3f, 0.3f},
        // fltA, fltD, fltS, fltR
        {0.005f, 0.005f, 0.005f, 0.005f},
        {0.1f, 0.1f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.3f, 0.3f, 0.3f, 0.3f},
        // lfos (all off)
        {
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs (inactive)
        {
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
        },
        // delay (off)
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
        // tube (moderate for visualization)
        {TubeModel::AX7, 2.0f, 0.3f, 0.15f, 0.6f},
        // mod routes
        {},
        0 // numRoutes
    };

    static constexpr Preset4D PRESET_SINGLE_SAW = {
        "Single Saw",
        // waves
        {WaveType::SAW, WaveType::SAW, WaveType::SAW, WaveType::SAW},
        // levels (only osc1 active)
        {1.0f, 0.0f, 0.0f, 0.0f},
        // pans (center)
        {0.5f, 0.5f, 0.5f, 0.5f},
        // cutoffs (wide open)
        {20000.0f, 20000.0f, 20000.0f, 20000.0f},
        // resonances
        {0.0f, 0.0f, 0.0f, 0.0f},
        // filterEnvAmounts
        {0.0f, 0.0f, 0.0f, 0.0f},
        // coarseTune
        {0.0f, 0.0f, 0.0f, 0.0f},
        // ampA, ampD, ampS, ampR
        {0.005f, 0.005f, 0.005f, 0.005f},
        {0.1f, 0.1f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.3f, 0.3f, 0.3f, 0.3f},
        // fltA, fltD, fltS, fltR
        {0.005f, 0.005f, 0.005f, 0.005f},
        {0.1f, 0.1f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.3f, 0.3f, 0.3f, 0.3f},
        // lfos (all off)
        {
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs (inactive)
        {
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
        },
        // delay (off)
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
        // tube (moderate for visualization)
        {TubeModel::AX7, 2.0f, 0.3f, 0.15f, 0.6f},
        // mod routes
        {},
        0 // numRoutes
    };

    static constexpr Preset4D PRESET_SINGLE_SQUARE = {
        "Single Square",
        // waves
        {WaveType::SQUARE, WaveType::SQUARE, WaveType::SQUARE, WaveType::SQUARE},
        // levels (only osc1 active)
        {1.0f, 0.0f, 0.0f, 0.0f},
        // pans (center)
        {0.5f, 0.5f, 0.5f, 0.5f},
        // cutoffs (wide open)
        {20000.0f, 20000.0f, 20000.0f, 20000.0f},
        // resonances
        {0.0f, 0.0f, 0.0f, 0.0f},
        // filterEnvAmounts
        {0.0f, 0.0f, 0.0f, 0.0f},
        // coarseTune
        {0.0f, 0.0f, 0.0f, 0.0f},
        // ampA, ampD, ampS, ampR
        {0.005f, 0.005f, 0.005f, 0.005f},
        {0.1f, 0.1f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.3f, 0.3f, 0.3f, 0.3f},
        // fltA, fltD, fltS, fltR
        {0.005f, 0.005f, 0.005f, 0.005f},
        {0.1f, 0.1f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.3f, 0.3f, 0.3f, 0.3f},
        // lfos (all off)
        {
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs (inactive)
        {
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
        },
        // delay (off)
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
        // tube (moderate for visualization)
        {TubeModel::AX7, 2.0f, 0.3f, 0.15f, 0.6f},
        // mod routes
        {},
        0 // numRoutes
    };

    // ─── Saw UP + Quintas (perfect fifth +7 st) + TubeReverb ───────────────
    static constexpr Preset4D PRESET_SAW_QUINTAS_TUBE = {
        "Saw Quintas Tube",
        // waves: osc1=root saw, osc2=quinta saw, osc3/4 off
        {WaveType::SAW, WaveType::SAW, WaveType::SAW, WaveType::SAW},
        // levels
        {0.75f, 0.65f, 0.0f, 0.0f},
        // pans
        {0.38f, 0.62f, 0.5f, 0.5f},
        // cutoffs (open)
        {14000.0f, 14000.0f, 20000.0f, 20000.0f},
        // resonances
        {0.05f, 0.05f, 0.0f, 0.0f},
        // filterEnvAmounts
        {0.0f, 0.0f, 0.0f, 0.0f},
        // coarseTune: osc2 = +7 semitones (quinta justa)
        {0.0f, 7.0f, 0.0f, 0.0f},
        // ampA, ampD, ampS, ampR
        {0.008f, 0.008f, 0.008f, 0.008f},
        {0.15f, 0.15f, 0.1f, 0.1f},
        {0.85f, 0.80f, 1.0f, 1.0f},
        {0.6f, 0.6f, 0.3f, 0.3f},
        // fltA, fltD, fltS, fltR
        {0.008f, 0.008f, 0.008f, 0.008f},
        {0.2f, 0.2f, 0.1f, 0.1f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.4f, 0.4f, 0.3f, 0.3f},
        // lfos (off)
        {
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs (inactive)
        {
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
            {0.005f, 0.1f, 0.0f, 0.3f},
        },
        // delay (off)
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
        // tube saturation (subtle warmth on the saw)
        {TubeModel::AU7, 1.5f, 0.15f, 0.05f, 0.3f},
        // mod routes (none)
        {},
        0, // numRoutes
        20000.0f,
        0.0f, // masterFilterCutoff, masterFilterResonance
        // tubeReverb: P1=2.5m (f0≈68Hz), P2=0.93, P3=0.5
        {2.5f, 0.93f, 0.5f},
    };

    // ─── Acid Growl (TB-303 style) ──────────────────────────────────────────
    static constexpr Preset4D PRESET_ACID_GROWL = {
        "Acid Growl",
        // waves: SAW root, SAW octave up, SQUARE accent, NOISE transient
        {WaveType::SAW, WaveType::SAW, WaveType::SQUARE, WaveType::NOISE},
        // levels
        {0.9f, 0.45f, 0.35f, 0.12f},
        // pans: root center, slight spread
        {0.5f, 0.45f, 0.55f, 0.5f},
        // cutoffs: very low, filter env will rip it open
        {350.0f, 400.0f, 600.0f, 2000.0f},
        // resonances: high for the acid squeal
        {0.75f, 0.7f, 0.65f, 0.3f},
        // filterEnvAmounts: massive env on root and octave
        {0.9f, 0.85f, 0.6f, 0.0f},
        // coarseTune: octave up for OSC2, sub for OSC4
        {0.0f, 12.0f, 7.0f, -12.0f},
        // ampA: instant attack for acid punch
        {0.001f, 0.001f, 0.002f, 0.001f},
        // ampD
        {0.08f, 0.1f, 0.12f, 0.04f},
        // ampS
        {0.5f, 0.3f, 0.25f, 0.0f},
        // ampR
        {0.1f, 0.12f, 0.08f, 0.05f},
        // fltA: snappy filter attack
        {0.001f, 0.001f, 0.002f, 0.001f},
        // fltD: short decay for acid character
        {0.12f, 0.15f, 0.1f, 0.05f},
        // fltS
        {0.0f, 0.0f, 0.0f, 0.0f},
        // fltR
        {0.08f, 0.1f, 0.06f, 0.04f},
        // lfos: slight pitch wobble, rhythmic filter pulse
        {
            {0.5f, LFOWaveType::SINE},
            {4.0f, LFOWaveType::SQUARE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs
        {
            {0.001f, 0.08f, 0.0f, 0.1f},
            {0.001f, 0.15f, 0.0f, 0.1f},
            {0.005f, 0.3f, 0.0f, 0.2f},
        },
        // delay: short dotted-8th slapback for groove
        {0.19f, 0.14f, 0.4f, 0.18f, 0.4f, false},
        // tube: EL34 heavy drive for that acid grind
        {TubeModel::EL34, 3.5f, 0.5f, 0.2f, 0.55f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.006f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_CUTOFF, 0.25f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 1.0f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC2_CUTOFF, 0.9f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::OSC1_CUTOFF, 0.7f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::TUBE_DRIVE, 0.6f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::LFO1_DEPTH, 0.9f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::TUBE_DRIVE, 0.6f},
        },
        8, // numRoutes
        5500.0f,
        0.25f // masterFilterCutoff, masterFilterResonance
    };

    // ─── Crystal Pluck ──────────────────────────────────────────────────────
    static constexpr Preset4D PRESET_CRYSTAL_PLUCK = {
        "Crystal Pluck",
        // waves: TRI body, SINE fundamental, SQUARE shimmer, SINE octave bell
        {WaveType::TRIANGLE, WaveType::SINE, WaveType::SQUARE, WaveType::SINE},
        // levels
        {0.75f, 0.8f, 0.25f, 0.4f},
        // pans: slight stereo spread
        {0.42f, 0.58f, 0.35f, 0.65f},
        // cutoffs: bright and open
        {12000.0f, 18000.0f, 8000.0f, 16000.0f},
        // resonances: subtle bell resonance
        {0.1f, 0.0f, 0.2f, 0.15f},
        // filterEnvAmounts: mild env for click transient
        {0.3f, 0.1f, 0.4f, 0.15f},
        // coarseTune: octave up for shimmer and bell
        {0.0f, 0.0f, 12.0f, 24.0f},
        // ampA: instant
        {0.001f, 0.001f, 0.001f, 0.001f},
        // ampD: the pluck shape
        {0.4f, 0.6f, 0.25f, 0.8f},
        // ampS
        {0.0f, 0.0f, 0.0f, 0.0f},
        // ampR: clean tail
        {0.5f, 0.7f, 0.3f, 0.9f},
        // fltA
        {0.001f, 0.001f, 0.001f, 0.001f},
        // fltD: filter closes fast
        {0.15f, 0.2f, 0.1f, 0.25f},
        // fltS
        {0.5f, 0.6f, 0.4f, 0.7f},
        // fltR
        {0.3f, 0.4f, 0.2f, 0.5f},
        // lfos: very slow shimmer, ultra-subtle vibrato
        {
            {0.15f, LFOWaveType::SINE},
            {0.08f, LFOWaveType::TRIANGLE},
            {0.0f, LFOWaveType::SINE},
            {6.0f, LFOWaveType::SINE},
        },
        // modEnvs
        {
            {0.001f, 0.2f, 0.0f, 0.3f},
            {0.001f, 0.4f, 0.0f, 0.5f},
            {0.001f, 0.8f, 0.0f, 1.0f},
        },
        // delay: bright stereo ping-pong
        {0.25f, 0.375f, 0.4f, 0.3f, 0.5f, true},
        // tube: AU7 gentle, just warmth
        {TubeModel::AU7, 1.2f, 0.05f, 0.0f, 0.2f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC3_LEVEL, 0.1f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC4_LEVEL, 0.08f},
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC1_PITCH, 0.004f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.5f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC3_LEVEL, 0.4f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC3_CUTOFF, 0.6f},
        },
        6, // numRoutes
        18000.0f,
        0.0f // masterFilterCutoff, masterFilterResonance
    };

    // ─── Dirty Lead ─────────────────────────────────────────────────────────
    static constexpr Preset4D PRESET_DIRTY_LEAD = {
        "Dirty Lead",
        // waves: two SAWs in unison + detune, SQUARE sub, SINE sub octave
        {WaveType::SAW, WaveType::SAW, WaveType::SQUARE, WaveType::SINE},
        // levels: SAWs dominant, sub support
        {0.85f, 0.8f, 0.4f, 0.3f},
        // pans: unison SAWs wide for fatness
        {0.3f, 0.7f, 0.5f, 0.5f},
        // cutoffs: mid-open for lead
        {4000.0f, 4500.0f, 2000.0f, 8000.0f},
        // resonances
        {0.35f, 0.3f, 0.45f, 0.1f},
        // filterEnvAmounts
        {0.5f, 0.5f, 0.3f, 0.0f},
        // coarseTune: slight detune between SAWs, sub -12
        {0.0f, 0.0f, 0.0f, -12.0f},
        // ampA: fast for lead punch
        {0.005f, 0.005f, 0.005f, 0.01f},
        // ampD
        {0.2f, 0.2f, 0.3f, 0.1f},
        // ampS: sustained lead
        {0.8f, 0.75f, 0.5f, 0.7f},
        // ampR
        {0.15f, 0.15f, 0.2f, 0.2f},
        // fltA
        {0.005f, 0.005f, 0.005f, 0.005f},
        // fltD
        {0.3f, 0.3f, 0.2f, 0.1f},
        // fltS
        {0.6f, 0.6f, 0.4f, 1.0f},
        // fltR
        {0.2f, 0.2f, 0.15f, 0.1f},
        // lfos: vibrato on mod wheel, slow filter LFO
        {
            {5.5f, LFOWaveType::SINE},
            {0.6f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        // modEnvs
        {
            {0.005f, 0.2f, 0.0f, 0.15f},
            {0.005f, 0.4f, 0.3f, 0.3f},
            {0.01f, 0.6f, 0.5f, 0.5f},
        },
        // delay: subtle doubling
        {0.12f, 0.18f, 0.3f, 0.15f, 0.35f, false},
        // tube: EL34 heavy saturation for grit
        {TubeModel::EL34, 4.0f, 0.4f, 0.3f, 0.6f},
        // mod routes
        {
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_PITCH, 0.05f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC2_PITCH, 0.05f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.015f}, // vibrato: the mod wheel brings it in (LFO1_DEPTH);
                                                                   // at 0 the slot was off and the route lost
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_CUTOFF, 0.3f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_CUTOFF, 0.3f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.6f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::TUBE_DRIVE, 0.4f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::OSC1_CUTOFF, 0.5f},
            {ObsSynthSrc::AFTERTOUCH, ObsSynthDst::TUBE_DRIVE, 0.5f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::LFO1_DEPTH, 1.0f},
        },
        10, // numRoutes
        8000.0f,
        0.2f // masterFilterCutoff, masterFilterResonance
    };

    // ─── Ambient Drone ──────────────────────────────────────────────────────
    static constexpr Preset4D PRESET_AMBIENT_DRONE = {
        "Ambient Drone",
        // waves: slow evolving layers — SAW, TRI, SINE, SINE
        {WaveType::SAW, WaveType::TRIANGLE, WaveType::SINE, WaveType::SINE},
        // levels: all present for thick texture
        {0.5f, 0.6f, 0.55f, 0.4f},
        // pans: very wide stereo image
        {0.2f, 0.8f, 0.35f, 0.65f},
        // cutoffs: warm and filtered
        {1800.0f, 2500.0f, 4000.0f, 6000.0f},
        // resonances: slight resonance for movement
        {0.15f, 0.1f, 0.05f, 0.0f},
        // filterEnvAmounts: slow filter swell
        {0.6f, 0.4f, 0.2f, 0.0f},
        // coarseTune: stacked 5ths / octaves for drone richness
        {0.0f, 7.0f, 12.0f, -12.0f},
        // ampA: very slow, evolving entry
        {2.5f, 3.0f, 2.0f, 1.5f},
        // ampD
        {1.0f, 1.2f, 0.8f, 0.5f},
        // ampS: long sustain
        {0.7f, 0.65f, 0.75f, 0.8f},
        // ampR: long release for infinite feel
        {4.0f, 5.0f, 3.5f, 6.0f},
        // fltA: filter opens slowly
        {3.0f, 3.5f, 2.5f, 1.0f},
        // fltD
        {2.0f, 2.5f, 1.5f, 0.5f},
        // fltS
        {0.5f, 0.45f, 0.6f, 1.0f},
        // fltR
        {3.0f, 4.0f, 2.5f, 1.0f},
        // lfos: very slow, organic movement
        {
            {0.07f, LFOWaveType::SINE},
            {0.11f, LFOWaveType::TRIANGLE},
            {0.05f, LFOWaveType::SINE},
            {0.19f, LFOWaveType::SINE},
        },
        // modEnvs
        {
            {1.0f, 3.0f, 0.5f, 5.0f},
            {2.0f, 5.0f, 0.7f, 8.0f},
            {3.0f, 8.0f, 0.9f, 10.0f},
        },
        // delay: long hall-like delay, high feedback
        {0.5f, 0.75f, 0.6f, 0.45f, 0.65f, true},
        // tube: AU7 gentle warmth
        {TubeModel::AU7, 1.3f, 0.1f, 0.0f, 0.4f},
        // mod routes
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.012f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC2_PITCH, 0.008f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_CUTOFF, 0.35f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_CUTOFF, 0.25f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC3_LEVEL, 0.15f},
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC4_LEVEL, 0.12f},
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC1_LEVEL, 0.1f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 0.8f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC2_CUTOFF, 0.7f},
            {ObsSynthSrc::XY_X, ObsSynthDst::MIX_CH3_LVL, 0.6f},
            {ObsSynthSrc::XY_Y, ObsSynthDst::MIX_CH4_LVL, 0.5f},
        },
        11, // numRoutes
        7000.0f,
        0.05f // masterFilterCutoff, masterFilterResonance
    };

    // ─── Warm Strings ────────────────────────────────────────────────────────
    // Slow-attack layered saws with chorus-like detuning and soft filter sweep
    static constexpr Preset4D PRESET_WARM_STRINGS = {
        "Warm Strings",
        {WaveType::SAW, WaveType::SAW, WaveType::SAW, WaveType::TRIANGLE},
        {0.7f, 0.65f, 0.55f, 0.3f},
        {0.25f, 0.75f, 0.40f, 0.60f},
        {2200.0f, 2400.0f, 1800.0f, 4000.0f},
        {0.15f, 0.10f, 0.20f, 0.05f},
        {0.5f, 0.4f, 0.6f, 0.0f},
        {0.0f, 7.0f, -7.0f, 0.0f}, // +7/-7 semitone spread
        {0.30f, 0.30f, 0.30f, 0.30f},
        {0.50f, 0.50f, 0.50f, 0.50f},
        {0.80f, 0.75f, 0.70f, 0.85f},
        {1.20f, 1.20f, 1.20f, 1.20f},
        {0.05f, 0.05f, 0.05f, 0.05f},
        {0.80f, 0.80f, 0.80f, 0.80f},
        {0.50f, 0.50f, 0.50f, 0.50f},
        {1.00f, 1.00f, 1.00f, 1.00f},
        {
            {0.30f, LFOWaveType::SINE},
            {0.25f, LFOWaveType::SINE},
            {0.20f, LFOWaveType::TRIANGLE},
            {0.0f, LFOWaveType::SINE},
        },
        {
            {0.01f, 0.4f, 0.0f, 0.8f},
            {0.05f, 1.0f, 0.3f, 1.5f},
            {0.20f, 2.0f, 0.5f, 2.0f},
        },
        {0.500f, 0.45f, 0.30f, 0.18f, 0.35f, true},
        {TubeModel::AU7, 1.2f, 0.1f, 0.15f, 0.2f},
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.005f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_PITCH, 0.005f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC1_CUTOFF, 0.25f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 0.6f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_LEVEL, 0.3f},
        },
        5,
        8000.0f,
        0.05f};

    // ─── Pluck Bass ──────────────────────────────────────────────────────────
    // Tight percussive bass with fast attack, quick filter decay
    static constexpr Preset4D PRESET_PLUCK_BASS = {
        "Pluck Bass",
        {WaveType::SAW, WaveType::SQUARE, WaveType::SINE, WaveType::SINE},
        {0.9f, 0.5f, 0.4f, 0.0f},
        {0.5f, 0.45f, 0.55f, 0.5f},
        {1800.0f, 1200.0f, 6000.0f, 20000.0f},
        {0.5f, 0.3f, 0.0f, 0.0f},
        {0.9f, 0.7f, 0.0f, 0.0f},
        {0.0f, 12.0f, 0.0f, 0.0f},
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.20f, 0.15f, 0.10f, 0.001f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.15f, 0.12f, 0.08f, 0.05f},
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.08f, 0.06f, 0.001f, 0.001f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.10f, 0.08f, 0.05f, 0.05f},
        {
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        {
            {0.001f, 0.12f, 0.0f, 0.15f},
            {0.001f, 0.08f, 0.0f, 0.10f},
            {0.001f, 0.05f, 0.0f, 0.05f},
        },
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false},
        {TubeModel::AX7, 2.5f, 0.35f, 0.0f, 0.4f},
        {
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.9f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_LEVEL, 0.4f},
        },
        2,
        20000.0f,
        0.0f};

    // ─── Icy Lead ────────────────────────────────────────────────────────────
    // Bright detuned lead with fast LFO vibrato and hard sync feel
    static constexpr Preset4D PRESET_ICY_LEAD = {
        "Icy Lead",
        {WaveType::SQUARE, WaveType::SAW, WaveType::SINE, WaveType::SINE},
        {0.8f, 0.5f, 0.2f, 0.0f},
        {0.5f, 0.52f, 0.48f, 0.5f},
        {8000.0f, 7000.0f, 15000.0f, 20000.0f},
        {0.5f, 0.3f, 0.0f, 0.0f},
        {0.6f, 0.4f, 0.0f, 0.0f},
        {0.0f, -5.0f, 0.0f, 0.0f},
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.10f, 0.10f, 0.001f, 0.001f},
        {0.75f, 0.70f, 1.0f, 1.0f},
        {0.10f, 0.10f, 0.10f, 0.10f},
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.15f, 0.12f, 0.001f, 0.001f},
        {0.10f, 0.20f, 1.0f, 1.0f},
        {0.20f, 0.18f, 0.10f, 0.10f},
        {
            {5.5f, LFOWaveType::SINE},
            {4.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        {
            {0.001f, 0.10f, 0.0f, 0.2f},
            {0.001f, 0.08f, 0.0f, 0.1f},
            {0.001f, 0.05f, 0.0f, 0.1f},
        },
        {0.250f, 0.3f, 0.20f, 0.10f, 0.25f, true},
        {TubeModel::EL34, 1.5f, 0.2f, 0.0f, 0.2f},
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.012f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_PITCH, 0.010f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::LFO1_RATE, 0.5f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.5f},
        },
        4,
        12000.0f,
        0.1f};

    // ─── Deep Atmosphere ─────────────────────────────────────────────────────
    // Slow evolving pad with heavy LFO filter modulation and reverb-like delay
    static constexpr Preset4D PRESET_DEEP_ATMOSPHERE = {
        "Deep Atmosphere",
        {WaveType::SINE, WaveType::TRIANGLE, WaveType::SAW, WaveType::SINE},
        {0.6f, 0.5f, 0.35f, 0.4f},
        {0.30f, 0.70f, 0.45f, 0.55f},
        {800.0f, 1200.0f, 600.0f, 2500.0f},
        {0.35f, 0.25f, 0.40f, 0.10f},
        {0.7f, 0.5f, 0.8f, 0.0f},
        {0.0f, 0.0f, 12.0f, -12.0f},
        {0.50f, 0.60f, 0.40f, 0.70f},
        {0.80f, 1.00f, 0.60f, 1.20f},
        {0.70f, 0.65f, 0.55f, 0.60f},
        {2.50f, 3.00f, 2.00f, 3.50f},
        {0.10f, 0.15f, 0.08f, 0.12f},
        {1.20f, 1.50f, 1.00f, 1.80f},
        {0.30f, 0.40f, 0.25f, 0.45f},
        {2.00f, 2.50f, 1.80f, 3.00f},
        {
            {0.10f, LFOWaveType::SINE},
            {0.07f, LFOWaveType::TRIANGLE},
            {0.05f, LFOWaveType::SINE},
            {0.08f, LFOWaveType::SINE},
        },
        {
            {0.20f, 2.0f, 0.4f, 3.0f},
            {0.50f, 3.0f, 0.6f, 4.0f},
            {1.00f, 5.0f, 0.7f, 6.0f},
        },
        {0.750f, 0.65f, 0.45f, 0.30f, 0.55f, true},
        {TubeModel::AU7, 1.1f, 0.05f, 0.10f, 0.15f},
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_CUTOFF, 0.50f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_CUTOFF, 0.40f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC3_LEVEL, 0.20f},
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC4_PITCH, 0.008f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 0.8f},
            {ObsSynthSrc::XY_X, ObsSynthDst::MIX_CH1_LVL, 0.5f},
            {ObsSynthSrc::XY_Y, ObsSynthDst::MIX_CH3_LVL, 0.5f},
        },
        7,
        5000.0f,
        0.1f};

    // ─── Funk Clavinet ───────────────────────────────────────────────────────
    // Snappy percussive keys with tight filter, wah character via mod wheel
    static constexpr Preset4D PRESET_FUNK_CLAV = {
        "Funk Clavinet",
        {WaveType::SQUARE, WaveType::SAW, WaveType::TRIANGLE, WaveType::NOISE},
        {0.85f, 0.45f, 0.30f, 0.08f},
        {0.48f, 0.52f, 0.50f, 0.50f},
        {2500.0f, 2000.0f, 8000.0f, 1800.0f},
        {0.6f, 0.4f, 0.0f, 0.5f},
        {0.8f, 0.6f, 0.0f, 0.95f},
        {0.0f, 0.0f, -12.0f, 0.0f},
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.12f, 0.10f, 0.08f, 0.05f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.12f, 0.10f, 0.06f, 0.04f},
        {0.001f, 0.001f, 0.001f, 0.001f},
        {0.05f, 0.04f, 0.001f, 0.03f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.08f, 0.06f, 0.04f, 0.03f},
        {
            {3.5f, LFOWaveType::TRIANGLE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
            {0.0f, LFOWaveType::SINE},
        },
        {
            {0.001f, 0.08f, 0.0f, 0.10f},
            {0.001f, 0.05f, 0.0f, 0.08f},
            {0.001f, 0.03f, 0.0f, 0.05f},
        },
        {0.125f, 0.35f, 0.15f, 0.08f, 0.18f, false},
        {TubeModel::AX7, 2.0f, 0.3f, 0.0f, 0.35f},
        {
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.85f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC2_CUTOFF, 0.65f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 0.90f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_CUTOFF, 0.30f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_LEVEL, 0.4f},
        },
        5,
        20000.0f,
        0.0f};

    // ============ Wavetable and FM ============
    // The wavetable presets read the banks as obssynth4d loads them: OSC1 and OSC2 bank_morph (32 tables, a
    // sine to 40 harmonics), OSC3 bank_harmonic (16 tables, one partial each), OSC4 bank_all_waves (sine,
    // saw, square, triangle, ramp, pulse). Positions are tables. The FM ones shape the index with a mod
    // envelope (OSCn_FM_INDEX: ±1 = ±10 on the index).

    // ─── WT Glass Sweep ─ two morph tables drifting apart, a slow pad
    static constexpr Preset4D PRESET_WT_GLASS_SWEEP = {
        "WT Glass Sweep",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.6f, 0.5f, 0.0f, 0.25f}, // levels
        {0.3f, 0.7f, 0.5f, 0.5f}, // pans
        {7000.0f, 6000.0f, 20000.0f, 4000.0f}, // cutoffs
        {0.1f, 0.1f, 0.0f, 0.0f}, // reso
        {0.1f, 0.1f, 0.0f, 0.0f}, // fenv
        {0.0f, 0.08f, 0.0f, -12.0f}, // tune
        {0.9f, 1.1f, 0.01f, 1.0f}, // ampA
        {1.0f, 1.0f, 0.1f, 1.0f}, // ampD
        {0.85f, 0.85f, 0.0f, 0.8f}, // ampS
        {1.8f, 2.0f, 0.1f, 1.5f}, // ampR
        {0.5f, 0.5f, 0.01f, 0.5f}, // fltA
        {1.5f, 1.5f, 0.1f, 1.0f}, // fltD
        {0.6f, 0.6f, 0.6f, 0.6f}, // fltS
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltR
        {{0.11f, LFOWaveType::SINE}, {0.07f, LFOWaveType::TRIANGLE}, {0.3f, LFOWaveType::SINE}, {4.5f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.42f, 0.63f, 0.45f, 0.3f, 0.55f, true}, // delay
        {TubeModel::AU7, 1.1f, 0.05f, 0.0f, 0.15f}, // tube
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_WT_MORPH, 0.3f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC2_WT_MORPH, -0.3f},
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC2_PITCH, 0.004f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_CUTOFF, 1.0f},
        },
        4, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::WAVETABLE, PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::WAVETABLE},
        {10.0f, 14.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── WT Morph Pluck ─ bright at the strike, a sine by the end: the envelope walks the table
    static constexpr Preset4D PRESET_WT_MORPH_PLUCK = {
        "WT Morph Pluck",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.8f, 0.0f, 0.0f, 0.35f}, // levels
        {0.45f, 0.5f, 0.5f, 0.55f}, // pans
        {12000.0f, 20000.0f, 20000.0f, 3000.0f}, // cutoffs
        {0.15f, 0.0f, 0.0f, 0.0f}, // reso
        {0.2f, 0.0f, 0.0f, 0.2f}, // fenv
        {0.0f, 0.0f, 0.0f, -12.0f}, // tune
        {0.002f, 0.01f, 0.01f, 0.002f}, // ampA
        {0.7f, 0.1f, 0.1f, 0.4f}, // ampD
        {0.0f, 0.0f, 0.0f, 0.0f}, // ampS
        {0.5f, 0.1f, 0.1f, 0.3f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltD
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltS
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltR
        {{0.2f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.001f, 0.35f, 0.0f, 0.3f}, {0.001f, 0.15f, 0.0f, 0.2f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.3f, 0.45f, 0.35f, 0.22f, 0.5f, true}, // delay
        {TubeModel::AT7, 1.3f, 0.1f, 0.0f, 0.2f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC1_WT_MORPH, 0.55f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_WT_MORPH, 0.15f},
            {ObsSynthSrc::MOD_ENV2, ObsSynthDst::OSC4_WT_MORPH, 0.3f},
        },
        3, // numRoutes
        18000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::STANDARD_OSC, PartType::WAVETABLE},
        {1.0f, 0.0f, 0.0f, 3.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── Overtone Whistle ─ one partial at a time, an LFO climbing the harmonic series over a sine
    static constexpr Preset4D PRESET_OVERTONE_WHISTLE = {
        "Overtone Whistle",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.45f, 0.0f, 0.6f, 0.0f}, // levels
        {0.5f, 0.5f, 0.5f, 0.5f}, // pans
        {20000.0f, 20000.0f, 20000.0f, 20000.0f}, // cutoffs
        {0.0f, 0.0f, 0.0f, 0.0f}, // reso
        {0.0f, 0.0f, 0.0f, 0.0f}, // fenv
        {0.0f, 0.0f, 0.0f, 0.0f}, // tune
        {0.25f, 0.01f, 0.35f, 0.01f}, // ampA
        {0.5f, 0.5f, 0.5f, 0.5f}, // ampD
        {0.9f, 0.0f, 0.9f, 0.0f}, // ampS
        {0.8f, 0.1f, 1.0f, 0.1f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltD
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltS
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltR
        {{0.25f, LFOWaveType::TRIANGLE}, {5.5f, LFOWaveType::SINE}, {0.13f, LFOWaveType::SINE}, {2.0f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.5f, 0.75f, 0.5f, 0.35f, 0.6f, true}, // delay
        {TubeModel::FLAT, 1.0f, 0.0f, 0.0f, 0.0f}, // tube
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC3_WT_MORPH, 0.25f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC3_PITCH, 0.003f},
            {ObsSynthSrc::XY_X, ObsSynthDst::OSC3_WT_MORPH, 0.4f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC3_LEVEL, 0.3f},
        },
        4, // numRoutes
        20000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::STANDARD_OSC, PartType::STANDARD_OSC, PartType::WAVETABLE, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 5.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── WT Shape Bass ─ square to saw on the strike, a morph sub under it
    static constexpr Preset4D PRESET_WT_SHAPE_BASS = {
        "WT Shape Bass",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.55f, 0.0f, 0.0f, 0.75f}, // levels
        {0.5f, 0.5f, 0.5f, 0.5f}, // pans
        {600.0f, 20000.0f, 20000.0f, 1800.0f}, // cutoffs
        {0.0f, 0.0f, 0.0f, 0.3f}, // reso
        {0.0f, 0.0f, 0.0f, 0.5f}, // fenv
        {-12.0f, 0.0f, 0.0f, -12.0f}, // tune
        {0.003f, 0.003f, 0.003f, 0.003f}, // ampA
        {0.3f, 0.3f, 0.3f, 0.3f}, // ampD
        {0.8f, 0.8f, 0.8f, 0.8f}, // ampS
        {0.15f, 0.15f, 0.15f, 0.15f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.25f, 0.25f, 0.25f, 0.25f}, // fltD
        {0.2f, 0.2f, 0.2f, 0.2f}, // fltS
        {0.2f, 0.2f, 0.2f, 0.2f}, // fltR
        {{0.2f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.001f, 0.25f, 0.0f, 0.2f}, {0.001f, 0.12f, 0.0f, 0.1f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.3f, 0.3f, 0.0f, 0.0f, 0.5f, false}, // delay
        {TubeModel::AX7, 2.0f, 0.2f, 0.0f, 0.35f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC4_WT_MORPH, -0.12f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC4_CUTOFF, 0.6f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC4_WT_MORPH, 0.3f},
        },
        3, // numRoutes
        12000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::STANDARD_OSC, PartType::WAVETABLE},
        {0.0f, 0.0f, 0.0f, 10.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── FM Electric Piano ─ 1:1 body whose index falls after the strike, a 14:1 tine on top
    static constexpr Preset4D PRESET_FM_ELECTRIC_PIANO = {
        "FM Electric Piano",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.75f, 0.22f, 0.0f, 0.0f}, // levels
        {0.45f, 0.55f, 0.5f, 0.5f}, // pans
        {20000.0f, 20000.0f, 20000.0f, 20000.0f}, // cutoffs
        {0.0f, 0.0f, 0.0f, 0.0f}, // reso
        {0.0f, 0.0f, 0.0f, 0.0f}, // fenv
        {0.0f, 0.0f, 0.0f, 0.0f}, // tune
        {0.002f, 0.001f, 0.01f, 0.01f}, // ampA
        {2.2f, 0.35f, 0.1f, 0.1f}, // ampD
        {0.25f, 0.0f, 0.0f, 0.0f}, // ampS
        {0.45f, 0.2f, 0.1f, 0.1f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltD
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltS
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltR
        {{4.8f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.001f, 0.9f, 0.0f, 0.4f}, {0.001f, 0.2f, 0.0f, 0.2f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.25f, 0.38f, 0.25f, 0.15f, 0.6f, true}, // delay
        {TubeModel::AT7, 1.4f, 0.15f, 0.0f, 0.25f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC1_FM_INDEX, 0.22f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_FM_INDEX, 0.12f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_LEVEL, 0.06f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_FM_INDEX, 0.2f},
        },
        4, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::FM_ENGINE, PartType::FM_ENGINE, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_SIMPLE, 1.0f, 0.6f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_SIMPLE, 14.0f, 0.8f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── FM Tubular Bell ─ inharmonic 3.5:1, a long ring that darkens
    static constexpr Preset4D PRESET_FM_TUBULAR_BELL = {
        "FM Tubular Bell",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.7f, 0.35f, 0.0f, 0.0f}, // levels
        {0.4f, 0.6f, 0.5f, 0.5f}, // pans
        {20000.0f, 20000.0f, 20000.0f, 20000.0f}, // cutoffs
        {0.0f, 0.0f, 0.0f, 0.0f}, // reso
        {0.0f, 0.0f, 0.0f, 0.0f}, // fenv
        {0.0f, 12.0f, 0.0f, 0.0f}, // tune
        {0.001f, 0.001f, 0.01f, 0.01f}, // ampA
        {4.0f, 2.5f, 0.1f, 0.1f}, // ampD
        {0.0f, 0.0f, 0.0f, 0.0f}, // ampS
        {3.0f, 2.0f, 0.1f, 0.1f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltD
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltS
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltR
        {{0.2f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.001f, 2.5f, 0.0f, 2.0f}, {0.001f, 1.2f, 0.0f, 1.0f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.6f, 0.9f, 0.4f, 0.25f, 0.6f, true}, // delay
        {TubeModel::FLAT, 1.0f, 0.0f, 0.0f, 0.0f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC1_FM_INDEX, 0.35f},
            {ObsSynthSrc::MOD_ENV2, ObsSynthDst::OSC2_FM_INDEX, 0.25f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_FM_INDEX, 0.15f},
        },
        3, // numRoutes
        20000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::FM_ENGINE, PartType::FM_ENGINE, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_SIMPLE, 3.5f, 0.5f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_SIMPLE, 1.41f, 0.4f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── FM Growl Bass ─ feedback FM, index snapped on the strike, a sine sub
    static constexpr Preset4D PRESET_FM_GROWL_BASS = {
        "FM Growl Bass",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.7f, 0.0f, 0.0f, 0.5f}, // levels
        {0.5f, 0.5f, 0.5f, 0.5f}, // pans
        {4000.0f, 20000.0f, 20000.0f, 400.0f}, // cutoffs
        {0.2f, 0.0f, 0.0f, 0.0f}, // reso
        {0.3f, 0.0f, 0.0f, 0.0f}, // fenv
        {-12.0f, 0.0f, 0.0f, -24.0f}, // tune
        {0.002f, 0.002f, 0.002f, 0.002f}, // ampA
        {0.3f, 0.3f, 0.3f, 0.3f}, // ampD
        {0.85f, 0.85f, 0.85f, 0.85f}, // ampS
        {0.12f, 0.12f, 0.12f, 0.12f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.2f, 0.2f, 0.2f, 0.2f}, // fltD
        {0.4f, 0.4f, 0.4f, 0.4f}, // fltS
        {0.2f, 0.2f, 0.2f, 0.2f}, // fltR
        {{0.2f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.001f, 0.18f, 0.15f, 0.15f}, {0.01f, 0.5f, 0.0f, 0.5f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.3f, 0.3f, 0.0f, 0.0f, 0.5f, false}, // delay
        {TubeModel::EL34, 2.2f, 0.25f, 0.0f, 0.4f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC1_FM_INDEX, 0.3f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_FM_INDEX, 0.15f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_FM_INDEX, 0.3f},
        },
        3, // numRoutes
        9000.0f, 0.1f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::FM_ENGINE, PartType::STANDARD_OSC, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.2f, 0.35f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };

    // ─── FM Brass Stab ─ a four-operator stack whose index swells with the note
    static constexpr Preset4D PRESET_FM_BRASS_STAB = {
        "FM Brass Stab",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.6f, 0.45f, 0.0f, 0.0f}, // levels
        {0.4f, 0.6f, 0.5f, 0.5f}, // pans
        {6000.0f, 5000.0f, 20000.0f, 20000.0f}, // cutoffs
        {0.1f, 0.1f, 0.0f, 0.0f}, // reso
        {0.2f, 0.2f, 0.0f, 0.0f}, // fenv
        {0.0f, 0.1f, 0.0f, 0.0f}, // tune
        {0.04f, 0.05f, 0.01f, 0.01f}, // ampA
        {0.4f, 0.4f, 0.4f, 0.4f}, // ampD
        {0.75f, 0.75f, 0.75f, 0.75f}, // ampS
        {0.25f, 0.25f, 0.25f, 0.25f}, // ampR
        {0.05f, 0.05f, 0.05f, 0.05f}, // fltA
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltD
        {0.6f, 0.6f, 0.6f, 0.6f}, // fltS
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltR
        {{5.2f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.06f, 0.4f, 0.5f, 0.3f}, {0.01f, 0.5f, 0.0f, 0.5f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.3f, 0.45f, 0.2f, 0.12f, 0.5f, true}, // delay
        {TubeModel::AX7, 1.8f, 0.2f, 0.0f, 0.3f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC1_FM_INDEX, 0.2f},
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC2_FM_INDEX, 0.2f},
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.003f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_FM_INDEX, 0.12f},
        },
        4, // numRoutes
        14000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::FM_ENGINE, PartType::FM_ENGINE, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::FOUR_OP_SERIAL, 1.0f, 0.8f, 0.0f, 1.0f, 0.6f}, {FMAlgorithm::TWO_OP_SIMPLE, 2.0f, 0.6f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
    };


    // ============ Vector ============
    // The four oscillators at the corners of a square (1 top left, 2 top right, 3 bottom left, 4 bottom
    // right), each with a different engine; every note walks the path from its note-on, the XY pad moving
    // the whole path. The wavetable corners read obssynth4d's banks (see above).

    // ─── Vector Journey ─ a slow circle through an FM bell, a bright table, a dark saw and the all-waves table
    static constexpr Preset4D PRESET_VECTOR_JOURNEY = {
        "Vector Journey",
        {WaveType::SINE, WaveType::SINE, WaveType::SAW, WaveType::SINE},
        {0.7f, 0.6f, 0.75f, 0.6f}, // levels
        {0.3f, 0.7f, 0.4f, 0.6f}, // pans
        {20000.0f, 7000.0f, 1400.0f, 4500.0f}, // cutoffs
        {0.0f, 0.15f, 0.35f, 0.1f}, // reso
        {0.0f, 0.1f, 0.25f, 0.1f}, // fenv
        {12.0f, 0.0f, -12.0f, 0.0f}, // tune
        {0.4f, 0.6f, 0.5f, 0.7f}, // ampA
        {1.0f, 1.0f, 1.0f, 1.0f}, // ampD
        {0.9f, 0.9f, 0.9f, 0.9f}, // ampS
        {2.0f, 2.2f, 1.8f, 2.0f}, // ampR
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltA
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltD
        {0.6f, 0.6f, 0.6f, 0.6f}, // fltS
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltR
        {{0.15f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {0.07f, LFOWaveType::TRIANGLE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.45f, 0.6f, 0.45f, 0.3f, 0.55f, true}, // delay
        {TubeModel::AU7, 1.2f, 0.1f, 0.0f, 0.2f}, // tube
        {
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC2_WT_MORPH, 0.25f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC3_PITCH, 0.003f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_FM_INDEX, 0.2f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC3_CUTOFF, 0.6f},
        },
        4, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::FM_ENGINE, PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::WAVETABLE},
        {0.0f, 16.0f, 0.0f, 12.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_SIMPLE, 3.5f, 1.2f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {true, true, 13, {{0.0f, 0.92f, 0.5f}, {0.5f, 0.863731f, 0.71f}, {1.0f, 0.71f, 0.863731f}, {1.5f, 0.5f, 0.92f}, {2.0f, 0.29f, 0.863731f}, {2.5f, 0.136269f, 0.71f}, {3.0f, 0.08f, 0.5f}, {3.5f, 0.136269f, 0.29f}, {4.0f, 0.29f, 0.136269f}, {4.5f, 0.5f, 0.08f}, {5.0f, 0.71f, 0.136269f}, {5.5f, 0.863731f, 0.29f}, {6.0f, 0.92f, 0.5f}}}, // vector: on, loop, the path (s, x, y)
    };

    // ─── Vector Strike to Pad ─ an FM strike in one corner falling into a warm saw pad in the other
    static constexpr Preset4D PRESET_VECTOR_STRIKE_TO_PAD = {
        "Vector Strike to Pad",
        {WaveType::SINE, WaveType::SINE, WaveType::SAW, WaveType::SINE},
        {0.8f, 0.5f, 0.7f, 0.5f}, // levels
        {0.5f, 0.6f, 0.4f, 0.5f}, // pans
        {20000.0f, 5000.0f, 2200.0f, 3000.0f}, // cutoffs
        {0.0f, 0.0f, 0.2f, 0.0f}, // reso
        {0.0f, 0.0f, 0.3f, 0.0f}, // fenv
        {0.0f, 12.0f, 0.0f, -12.0f}, // tune
        {0.001f, 0.05f, 0.4f, 0.6f}, // ampA
        {1.2f, 1.0f, 1.0f, 1.0f}, // ampD
        {0.3f, 0.6f, 0.85f, 0.8f}, // ampS
        {1.0f, 1.0f, 1.6f, 1.6f}, // ampR
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltA
        {1.2f, 1.2f, 1.2f, 1.2f}, // fltD
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltS
        {1.2f, 1.2f, 1.2f, 1.2f}, // fltR
        {{0.2f, LFOWaveType::SINE}, {5.2f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}},
        {{0.001f, 0.8f, 0.0f, 0.5f}, {0.01f, 0.5f, 0.0f, 0.5f}, {0.01f, 0.5f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.33f, 0.5f, 0.35f, 0.22f, 0.5f, true}, // delay
        {TubeModel::AT7, 1.3f, 0.1f, 0.0f, 0.2f}, // tube
        {
            {ObsSynthSrc::MOD_ENV, ObsSynthDst::OSC1_FM_INDEX, 0.3f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC3_PITCH, 0.003f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_FM_INDEX, 0.15f},
        },
        3, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::FM_ENGINE, PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::WAVETABLE},
        {0.0f, 8.0f, 0.0f, 20.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_SIMPLE, 1.0f, 0.5f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {true, false, 5, {{0.0f, 0.02f, 0.98f}, {0.25f, 0.2f, 0.75f}, {0.7f, 0.35f, 0.35f}, {1.6f, 0.25f, 0.08f}, {3.0f, 0.6f, 0.12f}}}, // vector: on, loop, the path (s, x, y)
    };

    // ─── Vector Pulse ─ a quick loop between a square and a bell table: timbre as rhythm
    static constexpr Preset4D PRESET_VECTOR_PULSE = {
        "Vector Pulse",
        {WaveType::SQUARE, WaveType::SINE, WaveType::SAW, WaveType::SINE},
        {0.6f, 0.6f, 0.6f, 0.6f}, // levels
        {0.35f, 0.65f, 0.5f, 0.5f}, // pans
        {3500.0f, 9000.0f, 2500.0f, 6000.0f}, // cutoffs
        {0.3f, 0.1f, 0.4f, 0.0f}, // reso
        {0.2f, 0.0f, 0.3f, 0.0f}, // fenv
        {0.0f, 12.0f, 0.0f, 0.0f}, // tune
        {0.005f, 0.005f, 0.005f, 0.005f}, // ampA
        {0.5f, 0.5f, 0.5f, 0.5f}, // ampD
        {0.8f, 0.8f, 0.8f, 0.8f}, // ampS
        {0.4f, 0.4f, 0.4f, 0.4f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.4f, 0.4f, 0.4f, 0.4f}, // fltD
        {0.4f, 0.4f, 0.4f, 0.4f}, // fltS
        {0.4f, 0.4f, 0.4f, 0.4f}, // fltR
        {{0.2f, LFOWaveType::SINE}, {0.1f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.357f, 0.536f, 0.4f, 0.25f, 0.5f, true}, // delay
        {TubeModel::AX7, 1.6f, 0.15f, 0.0f, 0.3f}, // tube
        {
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.3f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC3_CUTOFF, 0.5f},
        },
        2, // numRoutes
        15000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::STANDARD_OSC, PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::WAVETABLE},
        {0.0f, 6.0f, 0.0f, 26.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {true, true, 5, {{0.0f, 0.05f, 0.95f}, {0.18f, 0.95f, 0.95f}, {0.36f, 0.95f, 0.05f}, {0.54f, 0.05f, 0.05f}, {0.714f, 0.05f, 0.95f}}}, // vector: on, loop, the path (s, x, y)
    };

    // ─── Vector Overtone Choir ─ a figure of eight through partials, a soft saw and a sine: a choir that moves
    static constexpr Preset4D PRESET_VECTOR_OVERTONE_CHOIR = {
        "Vector Overtone Choir",
        {WaveType::SINE, WaveType::SINE, WaveType::SAW, WaveType::SINE},
        {0.6f, 0.5f, 0.6f, 0.7f}, // levels
        {0.3f, 0.7f, 0.5f, 0.5f}, // pans
        {20000.0f, 20000.0f, 1800.0f, 20000.0f}, // cutoffs
        {0.0f, 0.0f, 0.2f, 0.0f}, // reso
        {0.0f, 0.0f, 0.15f, 0.0f}, // fenv
        {0.0f, 0.0f, 0.0f, 0.0f}, // tune
        {0.8f, 0.9f, 0.7f, 0.6f}, // ampA
        {1.0f, 1.0f, 1.0f, 1.0f}, // ampD
        {0.9f, 0.9f, 0.9f, 0.9f}, // ampS
        {2.2f, 2.2f, 2.2f, 2.2f}, // ampR
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltA
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltD
        {0.6f, 0.6f, 0.6f, 0.6f}, // fltS
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltR
        {{0.09f, LFOWaveType::SINE}, {5.5f, LFOWaveType::SINE}, {0.13f, LFOWaveType::SINE}, {0.2f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.5f, 0.75f, 0.5f, 0.35f, 0.6f, true}, // delay
        {TubeModel::FLAT, 1.0f, 0.0f, 0.0f, 0.0f}, // tube
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC2_WT_MORPH, 0.2f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC3_PITCH, 0.004f},
            {ObsSynthSrc::LFO3, ObsSynthDst::OSC1_WT_MORPH, 0.15f},
        },
        3, // numRoutes
        18000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::WAVETABLE, PartType::WAVETABLE, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {3.0f, 4.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {true, true, 13, {{0.0f, 0.5f, 0.5f}, {0.72f, 0.71f, 0.82909f}, {1.44f, 0.863731f, 0.82909f}, {2.16f, 0.92f, 0.5f}, {2.88f, 0.863731f, 0.17091f}, {3.6f, 0.71f, 0.17091f}, {4.32f, 0.5f, 0.5f}, {5.04f, 0.29f, 0.82909f}, {5.76f, 0.136269f, 0.82909f}, {6.48f, 0.08f, 0.5f}, {7.2f, 0.136269f, 0.17091f}, {7.92f, 0.29f, 0.17091f}, {8.64f, 0.5f, 0.5f}}}, // vector: on, loop, the path (s, x, y)
    };


    // ============ Granular ============
    // Grains of the recording obssynth4d loads for them (--grain; by default the ALSA voice saying "rear
    // center": speech at 8..38% and 46..85% of it). The position is the WT position's knob and destination.

    // ─── Granular Voice Cloud ─ a slow cloud of speech, the position drifting through the words
    static constexpr Preset4D PRESET_GRANULAR_VOICE_CLOUD = {
        "Granular Voice Cloud",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.85f, 0.0f, 0.0f, 0.25f}, // levels
        {0.5f, 0.5f, 0.5f, 0.5f}, // pans
        {9000.0f, 20000.0f, 20000.0f, 1200.0f}, // cutoffs
        {0.1f, 0.0f, 0.0f, 0.0f}, // reso
        {0.0f, 0.0f, 0.0f, 0.0f}, // fenv
        {0.0f, 0.0f, 0.0f, -12.0f}, // tune
        {0.9f, 0.01f, 0.01f, 1.0f}, // ampA
        {1.0f, 1.0f, 1.0f, 1.0f}, // ampD
        {1.0f, 0.0f, 0.0f, 0.8f}, // ampS
        {2.5f, 0.1f, 0.1f, 2.0f}, // ampR
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltA
        {1.0f, 1.0f, 1.0f, 1.0f}, // fltD
        {0.6f, 0.6f, 0.6f, 0.6f}, // fltS
        {1.0f, 1.0f, 1.0f, 1.0f}, // fltR
        {{0.07f, LFOWaveType::TRIANGLE}, {0.3f, LFOWaveType::RANDOM}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.5f, 0.75f, 0.5f, 0.35f, 0.6f, true}, // delay
        {TubeModel::FLAT, 1.0f, 0.0f, 0.0f, 0.0f}, // tube
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_WT_MORPH, 0.12f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_PAN, 0.4f},
            {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::OSC1_WT_MORPH, 0.3f},
            {ObsSynthSrc::XY_X, ObsSynthDst::OSC1_WT_MORPH, 0.4f},
        },
        4, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::GRANULAR, PartType::STANDARD_OSC, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {}, // vector: off
        {{0.24f, 0.16f, 32.0f, 0.22f}, {0.3f, 0.08f, 25.0f, 0.1f}, {0.3f, 0.08f, 25.0f, 0.1f}, {0.3f, 0.08f, 25.0f, 0.1f}}, // grains: position, size s, per second, spray
    };

    // ─── Granular Glitch ─ short grains thrown wide, a saw LFO scanning, a resonant filter
    static constexpr Preset4D PRESET_GRANULAR_GLITCH = {
        "Granular Glitch",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SINE},
        {0.85f, 0.0f, 0.0f, 0.0f}, // levels
        {0.5f, 0.5f, 0.5f, 0.5f}, // pans
        {2500.0f, 20000.0f, 20000.0f, 20000.0f}, // cutoffs
        {0.55f, 0.0f, 0.0f, 0.0f}, // reso
        {0.35f, 0.0f, 0.0f, 0.0f}, // fenv
        {0.0f, 0.0f, 0.0f, 0.0f}, // tune
        {0.005f, 0.01f, 0.01f, 0.01f}, // ampA
        {0.4f, 0.4f, 0.4f, 0.4f}, // ampD
        {0.7f, 0.0f, 0.0f, 0.0f}, // ampS
        {0.3f, 0.1f, 0.1f, 0.1f}, // ampR
        {0.001f, 0.001f, 0.001f, 0.001f}, // fltA
        {0.25f, 0.25f, 0.25f, 0.25f}, // fltD
        {0.3f, 0.3f, 0.3f, 0.3f}, // fltS
        {0.2f, 0.2f, 0.2f, 0.2f}, // fltR
        {{1.4f, LFOWaveType::SAW}, {7.0f, LFOWaveType::RANDOM}, {0.3f, LFOWaveType::SINE}, {5.0f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.18f, 0.27f, 0.45f, 0.3f, 0.3f, true}, // delay
        {TubeModel::AX7, 2.0f, 0.2f, 0.0f, 0.35f}, // tube
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_WT_MORPH, 0.25f},
            {ObsSynthSrc::LFO2, ObsSynthDst::OSC1_PAN, 0.6f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.3f},
        },
        3, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::GRANULAR, PartType::STANDARD_OSC, PartType::STANDARD_OSC, PartType::STANDARD_OSC},
        {0.0f, 0.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {}, // vector: off
        {{0.6f, 0.025f, 70.0f, 0.7f}, {0.3f, 0.08f, 25.0f, 0.1f}, {0.3f, 0.08f, 25.0f, 0.1f}, {0.3f, 0.08f, 25.0f, 0.1f}}, // grains: position, size s, per second, spray
    };

    // ─── Vector Grains ─ the voice's grains in one corner, a table, an FM bell and a saw in the others
    static constexpr Preset4D PRESET_VECTOR_GRAINS = {
        "Vector Grains",
        {WaveType::SINE, WaveType::SINE, WaveType::SINE, WaveType::SAW},
        {0.85f, 0.6f, 0.6f, 0.6f}, // levels
        {0.4f, 0.6f, 0.45f, 0.55f}, // pans
        {10000.0f, 6000.0f, 20000.0f, 1600.0f}, // cutoffs
        {0.1f, 0.1f, 0.0f, 0.3f}, // reso
        {0.0f, 0.1f, 0.0f, 0.25f}, // fenv
        {0.0f, 0.0f, 12.0f, -12.0f}, // tune
        {0.6f, 0.5f, 0.3f, 0.6f}, // ampA
        {1.0f, 1.0f, 1.0f, 1.0f}, // ampD
        {0.9f, 0.9f, 0.9f, 0.9f}, // ampS
        {2.0f, 2.0f, 2.0f, 2.0f}, // ampR
        {0.5f, 0.5f, 0.5f, 0.5f}, // fltA
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltD
        {0.6f, 0.6f, 0.6f, 0.6f}, // fltS
        {1.5f, 1.5f, 1.5f, 1.5f}, // fltR
        {{0.1f, LFOWaveType::TRIANGLE}, {5.0f, LFOWaveType::SINE}, {0.3f, LFOWaveType::SINE}, {0.05f, LFOWaveType::SINE}},
        {{0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}, {0.01f, 0.3f, 0.0f, 0.5f}}, // mod envs A D S R
        {0.45f, 0.6f, 0.45f, 0.3f, 0.55f, true}, // delay
        {TubeModel::AU7, 1.2f, 0.1f, 0.0f, 0.2f}, // tube
        {
            {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_WT_MORPH, 0.1f},
            {ObsSynthSrc::LFO4, ObsSynthDst::OSC2_WT_MORPH, 0.25f},
            {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC3_FM_INDEX, 0.2f},
        },
        3, // numRoutes
        16000.0f, 0.0f, // master filter
        {1.2f, 0.92f, 0.0f}, // tube reverb: off
        {PartType::GRANULAR, PartType::WAVETABLE, PartType::FM_ENGINE, PartType::STANDARD_OSC},
        {0.0f, 14.0f, 0.0f, 0.0f}, // wavetable positions (tables)
        {{FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_SIMPLE, 3.5f, 1.0f, 0.0f, 2.0f, 0.5f}, {FMAlgorithm::TWO_OP_FEEDBACK, 1.0f, 1.0f, 0.0f, 2.0f, 0.5f}},
        {true, true, 5, {{0.0f, 0.08f, 0.92f}, {1.5f, 0.8f, 0.85f}, {3.0f, 0.85f, 0.15f}, {4.5f, 0.15f, 0.2f}, {6.0f, 0.08f, 0.92f}}}, // vector: on, loop, the path (s, x, y)
        {{0.6f, 0.12f, 28.0f, 0.2f}, {0.3f, 0.08f, 25.0f, 0.1f}, {0.3f, 0.08f, 25.0f, 0.1f}, {0.3f, 0.08f, 25.0f, 0.1f}}, // grains: position, size s, per second, spray
    };


    // ============ Preset Bank ============

    static constexpr int PRESET_BANK_SIZE = 34;
    static constexpr const Preset4D *PRESET_BANK[PRESET_BANK_SIZE] = {
        &PRESET_INIT,
        &PRESET_LUSH_PAD,
        &PRESET_ELECTRIC_KEYS,
        &PRESET_AGGRESSIVE_BASS,
        &PRESET_STACKED_FIFTHS_BASS,
        &PRESET_MINOR_NINTH_CHORD,
        &PRESET_ACID_GROWL,
        &PRESET_CRYSTAL_PLUCK,
        &PRESET_DIRTY_LEAD,
        &PRESET_AMBIENT_DRONE,
        &PRESET_SINGLE_SINE,
        &PRESET_SINGLE_SAW,
        &PRESET_SINGLE_SQUARE,
        &PRESET_SAW_QUINTAS_TUBE,
        &PRESET_WARM_STRINGS,
        &PRESET_PLUCK_BASS,
        &PRESET_ICY_LEAD,
        &PRESET_DEEP_ATMOSPHERE,
        &PRESET_FUNK_CLAV,
        &PRESET_WT_GLASS_SWEEP,
        &PRESET_WT_MORPH_PLUCK,
        &PRESET_OVERTONE_WHISTLE,
        &PRESET_WT_SHAPE_BASS,
        &PRESET_FM_ELECTRIC_PIANO,
        &PRESET_FM_TUBULAR_BELL,
        &PRESET_FM_GROWL_BASS,
        &PRESET_FM_BRASS_STAB,
        &PRESET_VECTOR_JOURNEY,
        &PRESET_VECTOR_STRIKE_TO_PAD,
        &PRESET_VECTOR_PULSE,
        &PRESET_VECTOR_OVERTONE_CHOIR,
        &PRESET_GRANULAR_VOICE_CLOUD,
        &PRESET_GRANULAR_GLITCH,
        &PRESET_VECTOR_GRAINS,
    };

} // namespace synth
