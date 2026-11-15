/**
 * @file test_synth_core.cpp
 * @brief Unit tests for Synth Core DSP components using doctest
 *
 * Test coverage:
 * - BiquadFilter: LP/HP/BP/Notch modes, parameter smoothing
 * - ModMatrix: Routing, accumulation, bipolar amounts
 * - OscillatorChain: Note on/off, filter, stereo, modulation
 * - EffectSlot: Bypass, dry/wet mix
 *
 * Run with:
 *   ./build/bin/Debug/synth_core_tests.exe
 *   ctest --test-dir build -V
 */

#include <doctest/doctest.h>
#include <cmath>

// Synth core components
#include "synth/synth.hpp"
#include "../src/products/obs_synth/obs_synth_mod_source.hpp"
#include "../src/products/obs_synth/obs_synth_mod_dest.hpp"

using namespace synth;

// ============================================================================
// TEST SUITE: BiquadFilter
// ============================================================================

TEST_SUITE("BiquadFilter")
{

    TEST_CASE("construction with defaults")
    {
        BiquadFilter filter(44100.0f);

        CHECK(filter.getType() == BiquadType::LOWPASS);
        CHECK(filter.getCutoff() == doctest::Approx(1000.0f));
        CHECK(filter.getResonance() == doctest::Approx(0.0f));
    }

    TEST_CASE("lowpass attenuates high frequencies")
    {
        BiquadFilter filter(44100.0f);
        filter.setSampleRate(44100.0f);
        filter.setType(BiquadType::LOWPASS);
        filter.setCutoff(500.0f);
        filter.setResonance(0.0f);

        // Process a high frequency signal (5kHz) - should be attenuated
        float high_freq_energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float input = sinf(2.0f * 3.14159f * 5000.0f * i / 44100.0f);
            float output = filter.process(input);
            high_freq_energy += output * output;
        }

        // Process a low frequency signal (100Hz) - should pass through
        filter.reset();
        float low_freq_energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float input = sinf(2.0f * 3.14159f * 100.0f * i / 44100.0f);
            float output = filter.process(input);
            low_freq_energy += output * output;
        }

        // Low frequency should have more energy than high frequency
        CHECK(low_freq_energy > high_freq_energy * 2.0f);
    }

    TEST_CASE("highpass attenuates low frequencies")
    {
        BiquadFilter filter(44100.0f);
        filter.setSampleRate(44100.0f);
        filter.setType(BiquadType::HIGHPASS);
        filter.setCutoff(2000.0f);
        filter.setResonance(0.0f);

        // Process low frequency (100Hz) - should be attenuated
        float low_freq_energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float input = sinf(2.0f * 3.14159f * 100.0f * i / 44100.0f);
            float output = filter.process(input);
            low_freq_energy += output * output;
        }

        // Process high frequency (5kHz) - should pass
        filter.reset();
        float high_freq_energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float input = sinf(2.0f * 3.14159f * 5000.0f * i / 44100.0f);
            float output = filter.process(input);
            high_freq_energy += output * output;
        }

        CHECK(high_freq_energy > low_freq_energy * 2.0f);
    }

    TEST_CASE("type switching")
    {
        BiquadFilter filter(44100.0f);

        filter.setType(BiquadType::LOWPASS);
        CHECK(filter.getType() == BiquadType::LOWPASS);

        filter.setType(BiquadType::HIGHPASS);
        CHECK(filter.getType() == BiquadType::HIGHPASS);

        filter.setType(BiquadType::BANDPASS);
        CHECK(filter.getType() == BiquadType::BANDPASS);

        filter.setType(BiquadType::NOTCH);
        CHECK(filter.getType() == BiquadType::NOTCH);

        filter.setType(BiquadType::ALLPASS);
        CHECK(filter.getType() == BiquadType::ALLPASS);
    }

    TEST_CASE("parameter smoothing")
    {
        BiquadFilter filter(44100.0f);
        filter.setSampleRate(44100.0f);
        filter.setCutoff(1000.0f);

        // Abrupt cutoff change
        filter.setCutoff(5000.0f);

        // Process a few samples - smoothing should prevent clicks
        float prev_output = 0.0f;
        float max_delta = 0.0f;

        for (int i = 0; i < 100; ++i)
        {
            float output = filter.process(0.5f);
            float delta = std::fabs(output - prev_output);
            max_delta = std::max(max_delta, delta);
            prev_output = output;
        }

        // With smoothing, transitions should be gradual
        CHECK(max_delta < 1.0f);
    }

} // TEST_SUITE("BiquadFilter")

// ============================================================================
// TEST SUITE: ModMatrix
// ============================================================================

TEST_SUITE("ModMatrix")
{

    TEST_CASE("construction")
    {
        ModMatrix matrix;

        CHECK(matrix.getActiveSlotCount() == 0);
    }

    TEST_CASE("add single route")
    {
        ModMatrix matrix;

        int slot = matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 0.5f);

        CHECK(slot >= 0);
        CHECK(matrix.getActiveSlotCount() == 1);

        const ModSlot *s = matrix.getSlot(slot);
        CHECK(s != nullptr);
        CHECK(s->source == (int)ObsSynthSrc::LFO1);
        CHECK(s->destination == (int)ObsSynthDst::OSC1_PITCH);
        CHECK(s->amount == doctest::Approx(0.5f));
        CHECK(s->enabled == true);
    }

    TEST_CASE("add multiple routes")
    {
        ModMatrix matrix;

        matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 0.5f);
        matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_CUTOFF, 0.3f);
        matrix.addRoute((int)ObsSynthSrc::FILTER_ENV1, (int)ObsSynthDst::OSC1_LEVEL, 1.0f);

        CHECK(matrix.getActiveSlotCount() == 3);
    }

    TEST_CASE("process modulation values")
    {
        ModMatrix matrix;
        matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 0.5f);

        // Set source value
        matrix.setSourceValue((int)ObsSynthSrc::LFO1, 1.0f);

        // Process
        matrix.process();

        // Check destination
        float value = matrix.getModValue((int)ObsSynthDst::OSC1_PITCH);
        CHECK(value == doctest::Approx(0.5f));
    }

    TEST_CASE("accumulation from multiple sources")
    {
        ModMatrix matrix;

        // Two sources modulating same destination
        matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 0.3f);
        matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_PITCH, 0.2f);

        matrix.setSourceValue((int)ObsSynthSrc::LFO1, 1.0f);
        matrix.setSourceValue((int)ObsSynthSrc::LFO2, 1.0f);
        matrix.process();

        float value = matrix.getModValue((int)ObsSynthDst::OSC1_PITCH);
        CHECK(value == doctest::Approx(0.5f));
    }

    TEST_CASE("negative modulation amount")
    {
        ModMatrix matrix;
        matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, -0.5f);

        matrix.setSourceValue((int)ObsSynthSrc::LFO1, 1.0f);
        matrix.process();

        float value = matrix.getModValue((int)ObsSynthDst::OSC1_PITCH);
        CHECK(value == doctest::Approx(-0.5f));
    }

    TEST_CASE("remove route")
    {
        ModMatrix matrix;
        int slot1 = matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 0.5f);
        matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_CUTOFF, 0.3f);

        CHECK(matrix.getActiveSlotCount() == 2);

        matrix.removeRoute(slot1);

        CHECK(matrix.getActiveSlotCount() == 1);
    }

    TEST_CASE("clear all routes")
    {
        ModMatrix matrix;
        matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 0.5f);
        matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_CUTOFF, 0.3f);
        matrix.addRoute((int)ObsSynthSrc::FILTER_ENV1, (int)ObsSynthDst::OSC1_LEVEL, 1.0f);

        matrix.clear();

        CHECK(matrix.getActiveSlotCount() == 0);
    }

} // TEST_SUITE("ModMatrix")

// ============================================================================
// TEST SUITE: ModSource & ModDest Helpers
// ============================================================================

TEST_SUITE("ModSource and ModDest")
{

    TEST_CASE("ModSource properties")
    {
        // Test bipolar sources
        CHECK(isBipolarSource(ObsSynthSrc::LFO1) == true);
        CHECK(isBipolarSource(ObsSynthSrc::PITCH_BEND) == true);

        // Test unipolar sources
        CHECK(isBipolarSource(ObsSynthSrc::VELOCITY) == false);
        CHECK(isBipolarSource(ObsSynthSrc::MOD_WHEEL) == false);

        // Test per-voice sources
        CHECK(isPerVoiceSource(ObsSynthSrc::VELOCITY) == true);
        CHECK(isPerVoiceSource(ObsSynthSrc::FILTER_ENV1) == true);

        // Test global sources
        CHECK(isPerVoiceSource(ObsSynthSrc::LFO1) == false);
        CHECK(isPerVoiceSource(ObsSynthSrc::XY_X) == false);
    }

    TEST_CASE("ModDest helpers")
    {
        // Get oscillator index from destination
        CHECK(getOscIndex(ObsSynthDst::OSC1_PITCH) == 0);
        CHECK(getOscIndex(ObsSynthDst::OSC2_CUTOFF) == 1);
        CHECK(getOscIndex(ObsSynthDst::OSC3_LEVEL) == 2);
        CHECK(getOscIndex(ObsSynthDst::OSC4_PAN) == 3);
        CHECK(getOscIndex(ObsSynthDst::MIX_CH1_LVL) == -1);

        // Test parameter index extraction
        CHECK(getOscParamIndex(ObsSynthDst::OSC1_PITCH) == 0);
        CHECK(getOscParamIndex(ObsSynthDst::OSC1_CUTOFF) == 7);

        // Test getOscDest
        CHECK(getOscDest(0, 0) == ObsSynthDst::OSC1_PITCH);
        CHECK(getOscDest(1, 7) == ObsSynthDst::OSC2_CUTOFF);
    }

} // TEST_SUITE("ModSource and ModDest")

// ============================================================================
// TEST SUITE: OscillatorChain
// ============================================================================

TEST_SUITE("OscillatorChain")
{

    TEST_CASE("construction")
    {
        OscillatorChain chain(44100.0f);

        CHECK(chain.isActive() == false);
    }

    TEST_CASE("note on and off")
    {
        OscillatorChain chain(44100.0f);

        chain.noteOn(60, 1.0f); // Middle C, full velocity
        CHECK(chain.isActive() == true);

        chain.noteOff();
        // Still active during release phase

        // Process until envelope completes
        for (int i = 0; i < 44100; ++i)
        {
            chain.process();
        }

        CHECK(chain.isActive() == false);
    }

    TEST_CASE("generates audio output")
    {
        OscillatorChain chain(44100.0f);
        chain.setWaveType(WaveType::SINE);
        chain.noteOn(69, 1.0f); // A4 = 440Hz

        float energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float sample = chain.process();
            energy += sample * sample;
        }

        // Should produce non-zero output
        CHECK(energy > 0.0f);
    }

    TEST_CASE("filter affects sound character")
    {
        const float sample_rate = 44100.0f;

        // Chain with high cutoff
        OscillatorChain chain_bright(sample_rate);
        chain_bright.setWaveType(WaveType::SAW);
        chain_bright.setCutoff(10000.0f);
        chain_bright.noteOn(60, 1.0f);

        // Chain with low cutoff
        OscillatorChain chain_dark(sample_rate);
        chain_dark.setWaveType(WaveType::SAW);
        chain_dark.setCutoff(500.0f);
        chain_dark.noteOn(60, 1.0f);

        // Measure high frequency content (simple: variance of sample deltas)
        float energy_bright = 0.0f;
        float energy_dark = 0.0f;
        float prev_bright = 0.0f;
        float prev_dark = 0.0f;

        for (int i = 0; i < 1000; ++i)
        {
            float bright = chain_bright.process();
            float dark = chain_dark.process();

            energy_bright += (bright - prev_bright) * (bright - prev_bright);
            energy_dark += (dark - prev_dark) * (dark - prev_dark);

            prev_bright = bright;
            prev_dark = dark;
        }

        // Bright should have more high-frequency content
        CHECK(energy_bright > energy_dark);
    }

    TEST_CASE("stereo output")
    {
        OscillatorChain chain(44100.0f);

        chain.noteOn(60, 1.0f);

        // Test center pan - process multiple samples for reliable test
        chain.setPan(0.5f);
        float left_energy = 0.0f;
        float right_energy = 0.0f;

        for (int i = 0; i < 100; ++i)
        {
            float left, right;
            chain.processStereo(left, right);
            left_energy += left * left;
            right_energy += right * right;
        }

        // At center pan, energies should be approximately equal
        CHECK(left_energy == doctest::Approx(right_energy).epsilon(0.1));

        // Test left pan - new chain to avoid state issues
        OscillatorChain chain_left(44100.0f);
        chain_left.noteOn(60, 1.0f);
        chain_left.setPan(0.0f); // Full left

        left_energy = 0.0f;
        right_energy = 0.0f;

        for (int i = 0; i < 100; ++i)
        {
            float left, right;
            chain_left.processStereo(left, right);
            left_energy += left * left;
            right_energy += right * right;
        }

        // Left should have more energy than right
        CHECK(left_energy > right_energy * 2.0f);
    }

    TEST_CASE("modulation inputs")
    {
        OscillatorChain chain(44100.0f);

        chain.noteOn(60, 1.0f);

        // Test pitch modulation
        chain.setPitchMod(12.0f); // +1 octave
        // Frequency should be doubled (approximately)

        // Test cutoff modulation
        chain.setCutoffMod(2000.0f); // +2000Hz

        // Test level modulation
        chain.setLevelMod(0.5f); // 50% level reduction

        // These should not crash and chain should still produce output
        float sample = chain.process();
        CHECK(std::isfinite(sample) == true);
    }

} // TEST_SUITE("OscillatorChain")

// ============================================================================
// TEST SUITE: EffectSlot
// ============================================================================

TEST_SUITE("EffectSlot")
{

    TEST_CASE("construction")
    {
        EffectSlot slot(44100.0f);

        CHECK(slot.hasEffect() == false);
        CHECK(slot.getEffectType() == EffectType::NONE);
        CHECK(slot.isBypassed() == false);
        CHECK(slot.getMix() == doctest::Approx(1.0f));
    }

    TEST_CASE("bypass mode")
    {
        EffectSlot slot(44100.0f);

        slot.setBypassed(true);
        CHECK(slot.isBypassed() == true);

        // Bypassed slot should pass through unchanged
        float input = 0.5f;
        float output = slot.process(input);
        CHECK(output == doctest::Approx(input).epsilon(0.0001f));
    }

    TEST_CASE("dry/wet mix")
    {
        EffectSlot slot(44100.0f);

        slot.setMix(0.5f);
        CHECK(slot.getMix() == doctest::Approx(0.5f));

        // Without effect, should still pass through
        float input = 0.5f;
        float output = slot.process(input);
        CHECK(output == doctest::Approx(input).epsilon(0.0001f));
    }

} // TEST_SUITE("EffectSlot")
