/**
 * @file test_tuning_diagnostic.cpp
 * @brief Diagnostic test for tuning accuracy and stereo pan behavior
 *
 * Tests:
 * 1. Single oscillator (OSC0, SAW, no filter envelope) at A4 (MIDI 69) → should be 440Hz
 * 2. Single oscillator with LPF fully open → should still be 440Hz
 * 3. Pan: OSC0 panned hard-left, OSC1 panned hard-right → L≠R, L+R balanced
 * 4. Sample rate used vs SAMPLE_RATE constant
 *
 * Frequency measurement: count zero-crossings over N samples, divide by 2 to get cycles.
 */

#include "../../lib/synth/synth_engine.hpp"
#include "../../lib/synth/chains/osc_chain.hpp"
#include "../../products/obs_synth/obs_synth_4d.hpp"
#include <cstdio>
#include <cmath>
#include <vector>
#include <numeric>

namespace
{
    constexpr float SAMPLE_RATE = 48000.0f;
    constexpr int   MEASURE_FRAMES = 48000; // 1 second

    // ── Measure frequency via zero-crossing counting ──────────────────────────
    // Returns frequency of dominant periodic component in mono signal.
    float measureFrequency(const float *buf, int n)
    {
        // Count positive-going zero crossings
        int crossings = 0;
        for (int i = 1; i < n; ++i)
        {
            if (buf[i - 1] < 0.0f && buf[i] >= 0.0f)
                ++crossings;
        }
        // Each cycle has 2 zero crossings (up + down) — we only count up
        float freq = (float)crossings / ((float)n / SAMPLE_RATE);
        return freq;
    }

    // ── Measure stereo balance: returns L_rms / R_rms ─────────────────────────
    float stereoBalance(const float *buf, int frames)
    {
        double lPow = 0, rPow = 0;
        for (int i = 0; i < frames; ++i)
        {
            double l = buf[i * 2];
            double r = buf[i * 2 + 1];
            lPow += l * l;
            rPow += r * r;
        }
        float lRms = std::sqrtf((float)(lPow / frames));
        float rRms = std::sqrtf((float)(rPow / frames));
        return (rRms > 1e-8f) ? lRms / rRms : 9999.0f;
    }

    void pass(const char *msg) { printf("[PASS] %s\n", msg); }
    void fail(const char *msg) { printf("[FAIL] %s\n", msg); }
    void info(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        printf("[INFO] ");
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }
}

int main()
{
    printf("=== Tuning & Pan Diagnostic ===\n");
    printf("SAMPLE_RATE = %.0f Hz\n\n", SAMPLE_RATE);

    std::vector<float> stereoBuffer(MEASURE_FRAMES * 2, 0.0f);
    std::vector<float> monoL(MEASURE_FRAMES);

    // ──────────────────────────────────────────────────────────────────────────
    // TEST 1: Raw SynthEngine — single voice, SAW wave, NO filter envelope
    // Expected: 440Hz ± 1Hz
    // ──────────────────────────────────────────────────────────────────────────
    printf("--- Test 1: SynthEngine raw (MIDI A4 = 69) ---\n");
    {
        synth::SynthEngine eng(SAMPLE_RATE, synth::WaveType::SINE);

        // Configure: single OSC, no filter env movement, wide open filter
        eng.getAllocator().configureVoices([](auto &voice) {
            voice.setCutoff({20000.0f, 20000.0f, 20000.0f, 20000.0f});
            voice.setResonance({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setFilterEnvAmount({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setLevel({1.0f, 0.0f, 0.0f, 0.0f}); // only OSC0
            voice.setCoarseTune({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setAmpADSR({0.001f, 0.01f, 1.0f, 0.01f}); // instant attack, sustain 1
        });

        eng.noteOn(69, 1.0f); // A4

        // Skip first 100ms (attack + transient)
        int skipFrames = (int)(SAMPLE_RATE * 0.1f);
        std::vector<float> skipBuf(skipFrames * 2);
        eng.generateStereoBuffer(skipBuf.data(), skipFrames);

        // Measure 1 second
        eng.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);

        // Extract L channel
        for (int i = 0; i < MEASURE_FRAMES; ++i)
            monoL[i] = stereoBuffer[i * 2];

        float freq = measureFrequency(monoL.data(), MEASURE_FRAMES);
        info("Measured frequency = %.3f Hz (expected 440.000 Hz)", freq);
        float error = std::fabsf(freq - 440.0f);
        if (error < 1.0f)
            pass("Tuning within ±1Hz");
        else
            fail("Tuning error > 1Hz — possible sample rate mismatch or coarse tune bug");
        printf("\n");
    }

    // ──────────────────────────────────────────────────────────────────────────
    // TEST 2: ObsSynth4D default preset, OSC0 only, check if filter env skews pitch
    // ──────────────────────────────────────────────────────────────────────────
    printf("--- Test 2: ObsSynth4D preset 0, checking frequency in sustain ---\n");
    {
        synth::ObsSynth4D synth(SAMPLE_RATE);
        synth.selectPreset(0);

        // Isolate to OSC0 only to get a clean measurement
        synth.setOscLevel(0, 1.0f);
        synth.setOscLevel(1, 0.0f);
        synth.setOscLevel(2, 0.0f);
        synth.setOscLevel(3, 0.0f);
        // Disable filter env on OSC0 so it doesn't sweep cutoff during sustain
        synth.getEngine().getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            if (i == 0) {
                c.setCutoff(20000.0f);
                c.setResonance(0.0f);
                c.setFilterEnvAmount(0.0f);
            }
        });

        synth.noteOn(69, 1.0f); // A4

        // Skip 500ms (well into sustain phase)
        int skipFrames = (int)(SAMPLE_RATE * 0.5f);
        std::vector<float> skipBuf(skipFrames * 2);
        synth.generateStereoBuffer(skipBuf.data(), skipFrames);

        // Measure 500ms
        int measureFrames = (int)(SAMPLE_RATE * 0.5f);
        std::vector<float> mbuf(measureFrames * 2);
        synth.generateStereoBuffer(mbuf.data(), measureFrames);
        for (int i = 0; i < measureFrames; ++i)
            monoL[i] = mbuf[i * 2];
        float freq = measureFrequency(monoL.data(), measureFrames);
        info("Measured frequency = %.3f Hz (expected 440.000 Hz)", freq);
        float error = std::fabsf(freq - 440.0f);
        if (error < 1.0f)
            pass("Tuning within ±1Hz in sustain");
        else
            fail("Tuning error in sustain — check filter/mod matrix routing");
        printf("\n");
    }

    // ──────────────────────────────────────────────────────────────────────────
    // TEST 3: ObsSynth4D — frequency during FULL NOTE (attack+decay+sustain)
    // This replicates what the tuner hears: possible pitch shift during filter sweep
    // ──────────────────────────────────────────────────────────────────────────
    printf("--- Test 3: ObsSynth4D full note (attack+decay) — detect filter resonance peak ---\n");
    {
        synth::ObsSynth4D synth(SAMPLE_RATE);
        synth.selectPreset(0);

        // Isolate OSC0, keep filter envelope active (default preset)
        synth.setOscLevel(0, 1.0f);
        synth.setOscLevel(1, 0.0f);
        synth.setOscLevel(2, 0.0f);
        synth.setOscLevel(3, 0.0f);

        synth.noteOn(69, 1.0f); // A4

        // Measure entire 1 second (including filter sweep)
        synth.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);

        for (int i = 0; i < MEASURE_FRAMES; ++i)
            monoL[i] = stereoBuffer[i * 2];

        float freq = measureFrequency(monoL.data(), MEASURE_FRAMES);
        info("Measured frequency WITH filter env = %.3f Hz (expected 440.000 Hz)", freq);
        float error = std::fabsf(freq - 440.0f);
        if (error < 2.0f)
            pass("Tuning ok even with filter envelope");
        else
            fail("Tuning skewed by filter envelope resonance peak — filter is dominating spectrum");
        printf("\n");
    }

    // ──────────────────────────────────────────────────────────────────────────
    // TEST 4: Pan behavior — L/R channel balance
    // ──────────────────────────────────────────────────────────────────────────
    printf("--- Test 4: Pan — hard left vs hard right vs center ---\n");
    {
        synth::SynthEngine eng(SAMPLE_RATE, synth::WaveType::SINE);
        eng.getAllocator().configureVoices([](auto &voice) {
            voice.setCutoff({20000.0f, 20000.0f, 20000.0f, 20000.0f});
            voice.setResonance({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setFilterEnvAmount({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setCoarseTune({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setAmpADSR({0.001f, 0.01f, 1.0f, 0.01f});
        });

        // --- Hard left (pan = 0.0) ---
        eng.getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            c.setLevel(i == 0 ? 1.0f : 0.0f);
            if (i == 0) c.setPan(0.0f); // hard left
        });
        eng.noteOn(69, 1.0f);
        eng.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);
        float balL = stereoBalance(stereoBuffer.data(), MEASURE_FRAMES);
        info("Hard-left (pan=0):   L/R ratio = %.4f  (expected >> 1, R ≈ 0)", balL);
        if (balL > 10.0f)
            pass("Hard-left panning: R is near-silent as expected");
        else
            fail("Hard-left panning: both channels have signal — pan not working");
        eng.noteOff(69);
        // let release finish
        eng.generateStereoBuffer(stereoBuffer.data(), 4800);

        // --- Hard right (pan = 1.0) ---
        eng.getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            if (i == 0) c.setPan(1.0f); // hard right
        });
        eng.noteOn(69, 1.0f);
        eng.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);
        float balR = stereoBalance(stereoBuffer.data(), MEASURE_FRAMES);
        info("Hard-right (pan=1):  L/R ratio = %.4f  (expected << 1, L ≈ 0)", balR);
        if (balR < 0.1f)
            pass("Hard-right panning: L is near-silent as expected");
        else
            fail("Hard-right panning: both channels have signal — pan not working");
        eng.noteOff(69);
        eng.generateStereoBuffer(stereoBuffer.data(), 4800);

        // --- Center (pan = 0.5) ---
        eng.getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            if (i == 0) c.setPan(0.5f); // center
        });
        eng.noteOn(69, 1.0f);
        eng.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);
        float balC = stereoBalance(stereoBuffer.data(), MEASURE_FRAMES);
        info("Center (pan=0.5):    L/R ratio = %.4f  (expected ≈ 1.0)", balC);
        if (std::fabsf(balC - 1.0f) < 0.05f)
            pass("Center panning: L and R are balanced");
        else
            fail("Center panning: L/R imbalance > 5%");
        printf("\n");
    }

    // ──────────────────────────────────────────────────────────────────────────
    // TEST 5: Check coarse_tune A4 with different OSCs
    // ──────────────────────────────────────────────────────────────────────────
    printf("--- Test 5: Verify coarse_tune=0 OSC 0 = 440Hz, coarse_tune=-12 OSC = 220Hz ---\n");
    {
        synth::SynthEngine eng(SAMPLE_RATE, synth::WaveType::SINE);
        eng.getAllocator().configureVoices([](auto &voice) {
            voice.setCutoff({20000.0f, 20000.0f, 20000.0f, 20000.0f});
            voice.setResonance({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setFilterEnvAmount({0.0f, 0.0f, 0.0f, 0.0f});
            voice.setCoarseTune({0.0f, -12.0f, 0.0f, 0.0f});
            voice.setAmpADSR({0.001f, 0.01f, 1.0f, 0.01f});
        });

        // OSC0 only (coarse=0, expect 440Hz)
        eng.getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            c.setLevel(i == 0 ? 1.0f : 0.0f);
        });
        eng.noteOn(69, 1.0f);
        std::vector<float> skip(4800 * 2);
        eng.generateStereoBuffer(skip.data(), 4800); // skip 100ms
        eng.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);
        for (int i = 0; i < MEASURE_FRAMES; ++i) monoL[i] = stereoBuffer[i * 2];
        float f0 = measureFrequency(monoL.data(), MEASURE_FRAMES);
        info("OSC0 coarse=0:  %.3f Hz (expected 440.0 Hz)", f0);

        eng.noteOff(69);
        eng.generateStereoBuffer(skip.data(), 4800);

        // OSC1 only (coarse=-12, expect 220Hz)
        eng.getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            c.setLevel(i == 1 ? 1.0f : 0.0f);
        });
        eng.noteOn(69, 1.0f);
        eng.generateStereoBuffer(skip.data(), 4800);
        eng.generateStereoBuffer(stereoBuffer.data(), MEASURE_FRAMES);
        for (int i = 0; i < MEASURE_FRAMES; ++i) monoL[i] = stereoBuffer[i * 2];
        float f1 = measureFrequency(monoL.data(), MEASURE_FRAMES);
        info("OSC1 coarse=-12: %.3f Hz (expected 220.0 Hz)", f1);

        bool t0ok = std::fabsf(f0 - 440.0f) < 1.0f;
        bool t1ok = std::fabsf(f1 - 220.0f) < 1.0f;
        if (t0ok && t1ok)
            pass("Coarse tune semitone mapping is correct");
        else
            fail("Coarse tune is wrong — check midiNoteToFrequency or updateOscillatorFrequency");
        printf("\n");
    }

    // ──────────────────────────────────────────────────────────────────────────
    // TEST 6: Pitch bend ±2 semitones
    // ──────────────────────────────────────────────────────────────────────────
    printf("--- Test 6: Pitch bend +2 semitones → A4 should become B4 (493.88 Hz) ---\n");
    {
        synth::ObsSynth4D synth(SAMPLE_RATE);
        synth.setOscLevel(0, 1.0f);
        synth.setOscLevel(1, 0.0f);
        synth.setOscLevel(2, 0.0f);
        synth.setOscLevel(3, 0.0f);
        synth.getEngine().getAllocator().configureChains([](synth::OscillatorChain &c, int i) {
            if (i == 0) {
                c.setCutoff(20000.0f);
                c.setResonance(0.0f);
                c.setFilterEnvAmount(0.0f);
                c.setCoarseTune(0.0f);
            }
        });

        synth.noteOn(69, 1.0f);
        synth.setPitchBend(2.0f); // +2 semitones

        std::vector<float> skip(4800 * 2);
        synth.generateStereoBuffer(skip.data(), 4800);

        int mf = MEASURE_FRAMES / 2;
        std::vector<float> pbuf(mf * 2);
        synth.generateStereoBuffer(pbuf.data(), mf);
        for (int i = 0; i < mf; ++i) monoL[i] = pbuf[i * 2];
        float freq = measureFrequency(monoL.data(), mf);
        float expected = 440.0f * std::powf(2.0f, 2.0f / 12.0f); // = 493.88 Hz
        info("Pitch bend +2 semi: %.3f Hz (expected %.3f Hz)", freq, expected);
        if (std::fabsf(freq - expected) < 2.0f)
            pass("Pitch bend +2 semitones is accurate");
        else
            fail("Pitch bend error > 2Hz");
        printf("\n");
    }

    printf("=== Diagnostic Complete ===\n");
    printf("\nNOTE: If TEST 3 fails but TEST 2 passes, the tuner is being confused by\n");
    printf("filter resonance peaks during the filter envelope attack sweep.\n");
    printf("Solution: reduce resonance, reduce filter env amount, or increase cutoff base.\n");
    return 0;
}
