/**
 * @file test_synth_core.cpp
 * @brief Unit tests for synth core module (Phase 1/2)
 *
 * Tests:
 * - BiquadFilter (LP/HP/BP/Notch)
 * - EffectSlot (dry/wet, bypass)
 * - ModMatrix (routing, amounts)
 * - OscillatorChain (full signal path)
 */

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

#include "synth/synth.hpp"
#include "../products/obs_synth/obs_synth_mod_source.hpp"
#include "../products/obs_synth/obs_synth_mod_dest.hpp"

using namespace synth;

// Test assertion macro
#define REQUIRE(cond)                                                               \
    do                                                                              \
    {                                                                               \
        if (!(cond))                                                                \
        {                                                                           \
            std::fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            return false;                                                           \
        }                                                                           \
    } while (0)

#define REQUIRE_NEAR(a, b, tol)                                                     \
    do                                                                              \
    {                                                                               \
        if (std::fabs((a) - (b)) > (tol))                                           \
        {                                                                           \
            std::fprintf(stderr, "FAIL: %f != %f (tol=%f) at %s:%d\n",              \
                         (double)(a), (double)(b), (double)(tol), __FILE__, __LINE__); \
            return false;                                                           \
        }                                                                           \
    } while (0)

// ============================================================================
// BiquadFilter Tests
// ============================================================================

bool test_biquad_construction()
{
    BiquadFilter filter(44100.0f, BiquadType::LOWPASS, 1000.0f, 0.5f);

    REQUIRE(filter.getType() == BiquadType::LOWPASS);
    REQUIRE_NEAR(filter.getCutoff(), 1000.0f, 1.0f);
    REQUIRE_NEAR(filter.getResonance(), 0.5f, 0.01f);

    return true;
}

bool test_biquad_lowpass_attenuation()
{
    const float sample_rate = 44100.0f;
    const float cutoff = 1000.0f;

    BiquadFilter filter(sample_rate, BiquadType::LOWPASS, cutoff, 0.0f);

    // Generate high frequency sine (5000 Hz - well above cutoff)
    const int num_samples = 1000;
    float high_freq_energy = 0.0f;

    for (int i = 0; i < num_samples; ++i)
    {
        float phase = (5000.0f / sample_rate) * i * 6.28318f;
        float input = std::sin(phase);
        float output = filter.process(input);
        high_freq_energy += output * output;
    }

    // Reset and test low frequency (200 Hz - well below cutoff)
    filter.reset();
    float low_freq_energy = 0.0f;

    for (int i = 0; i < num_samples; ++i)
    {
        float phase = (200.0f / sample_rate) * i * 6.28318f;
        float input = std::sin(phase);
        float output = filter.process(input);
        low_freq_energy += output * output;
    }

    // Low frequency should pass through with more energy than high frequency
    REQUIRE(low_freq_energy > high_freq_energy * 5.0f);

    return true;
}

bool test_biquad_highpass_attenuation()
{
    const float sample_rate = 44100.0f;
    const float cutoff = 2000.0f;

    BiquadFilter filter(sample_rate, BiquadType::HIGHPASS, cutoff, 0.0f);

    // Generate low frequency sine (200 Hz - well below cutoff)
    const int num_samples = 1000;
    float low_freq_energy = 0.0f;

    for (int i = 0; i < num_samples; ++i)
    {
        float phase = (200.0f / sample_rate) * i * 6.28318f;
        float input = std::sin(phase);
        float output = filter.process(input);
        low_freq_energy += output * output;
    }

    // Reset and test high frequency (8000 Hz - well above cutoff)
    filter.reset();
    float high_freq_energy = 0.0f;

    for (int i = 0; i < num_samples; ++i)
    {
        float phase = (8000.0f / sample_rate) * i * 6.28318f;
        float input = std::sin(phase);
        float output = filter.process(input);
        high_freq_energy += output * output;
    }

    // High frequency should pass through with more energy than low frequency
    REQUIRE(high_freq_energy > low_freq_energy * 5.0f);

    return true;
}

bool test_biquad_type_switching()
{
    BiquadFilter filter(44100.0f);

    filter.setType(BiquadType::LOWPASS);
    REQUIRE(filter.getType() == BiquadType::LOWPASS);

    filter.setType(BiquadType::HIGHPASS);
    REQUIRE(filter.getType() == BiquadType::HIGHPASS);

    filter.setType(BiquadType::BANDPASS);
    REQUIRE(filter.getType() == BiquadType::BANDPASS);

    filter.setType(BiquadType::NOTCH);
    REQUIRE(filter.getType() == BiquadType::NOTCH);

    return true;
}

bool test_biquad_parameter_smoothing()
{
    BiquadFilter filter(44100.0f, BiquadType::LOWPASS, 1000.0f, 0.0f);

    // Set new cutoff (smoothed)
    filter.setCutoff(5000.0f);

    // Process some samples - cutoff should gradually change
    for (int i = 0; i < 100; ++i)
    {
        filter.process(0.0f);
    }

    // Should be moving towards target but not there yet after only 100 samples
    float current = filter.getCutoff();
    REQUIRE(current > 1000.0f);
    REQUIRE(current < 5000.0f);

    return true;
}

// ============================================================================
// ModMatrix Tests
// ============================================================================

bool test_modmatrix_construction()
{
    ModMatrix matrix;

    REQUIRE(matrix.getActiveSlotCount() == 0);

    return true;
}

bool test_modmatrix_add_route()
{
    ModMatrix matrix;

    int slot = matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_CUTOFF, 0.5f);

    REQUIRE(slot >= 0);
    REQUIRE(matrix.getActiveSlotCount() == 1);

    const ModSlot *s = matrix.getSlot(slot);
    REQUIRE(s != nullptr);
    REQUIRE(s->source == (int)ObsSynthSrc::LFO1);
    REQUIRE(s->destination == (int)ObsSynthDst::OSC1_CUTOFF);
    REQUIRE_NEAR(s->amount, 0.5f, 0.001f);
    REQUIRE(s->enabled == true);

    return true;
}

bool test_modmatrix_multiple_routes()
{
    ModMatrix matrix;

    matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 1.0f);
    matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_CUTOFF, 0.5f);
    matrix.addRoute((int)ObsSynthSrc::VELOCITY, (int)ObsSynthDst::OSC1_LEVEL, 0.8f);

    REQUIRE(matrix.getActiveSlotCount() == 3);

    return true;
}

bool test_modmatrix_process()
{
    ModMatrix matrix;

    // Add route: LFO1 -> OSC1_CUTOFF with amount 0.5
    matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_CUTOFF, 0.5f);

    // Set LFO1 value to 1.0
    matrix.setSourceValue((int)ObsSynthSrc::LFO1, 1.0f);

    // Process
    matrix.process();

    // Check result: 1.0 * 0.5 = 0.5
    float mod_value = matrix.getModValue((int)ObsSynthDst::OSC1_CUTOFF);
    REQUIRE_NEAR(mod_value, 0.5f, 0.001f);

    return true;
}

bool test_modmatrix_accumulation()
{
    ModMatrix matrix;

    // Two sources to same destination
    matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_CUTOFF, 0.3f);
    matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_CUTOFF, 0.4f);

    matrix.setSourceValue((int)ObsSynthSrc::LFO1, 1.0f);
    matrix.setSourceValue((int)ObsSynthSrc::LFO2, 1.0f);

    matrix.process();

    // Should accumulate: 0.3 + 0.4 = 0.7
    float mod_value = matrix.getModValue((int)ObsSynthDst::OSC1_CUTOFF);
    REQUIRE_NEAR(mod_value, 0.7f, 0.001f);

    return true;
}

bool test_modmatrix_negative_amount()
{
    ModMatrix matrix;

    matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, -1.0f);
    matrix.setSourceValue((int)ObsSynthSrc::LFO1, 0.5f);
    matrix.process();

    float mod_value = matrix.getModValue((int)ObsSynthDst::OSC1_PITCH);
    REQUIRE_NEAR(mod_value, -0.5f, 0.001f);

    return true;
}

bool test_modmatrix_remove_route()
{
    ModMatrix matrix;

    int slot = matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_CUTOFF, 0.5f);
    REQUIRE(matrix.getActiveSlotCount() == 1);

    matrix.removeRoute(slot);
    REQUIRE(matrix.getActiveSlotCount() == 0);

    return true;
}

bool test_modmatrix_clear()
{
    ModMatrix matrix;

    matrix.addRoute((int)ObsSynthSrc::LFO1, (int)ObsSynthDst::OSC1_PITCH, 1.0f);
    matrix.addRoute((int)ObsSynthSrc::LFO2, (int)ObsSynthDst::OSC1_CUTOFF, 0.5f);
    matrix.addRoute((int)ObsSynthSrc::VELOCITY, (int)ObsSynthDst::OSC1_LEVEL, 0.8f);

    REQUIRE(matrix.getActiveSlotCount() == 3);

    matrix.clear();

    REQUIRE(matrix.getActiveSlotCount() == 0);

    return true;
}

// ============================================================================
// ModSource/ModDest Tests
// ============================================================================

bool test_mod_source_properties()
{
    // Test bipolar sources
    REQUIRE(isBipolarSource(ObsSynthSrc::LFO1) == true);
    REQUIRE(isBipolarSource(ObsSynthSrc::PITCH_BEND) == true);

    // Test unipolar sources
    REQUIRE(isBipolarSource(ObsSynthSrc::VELOCITY) == false);
    REQUIRE(isBipolarSource(ObsSynthSrc::MOD_WHEEL) == false);

    // Test per-voice sources
    REQUIRE(isPerVoiceSource(ObsSynthSrc::VELOCITY) == true);
    REQUIRE(isPerVoiceSource(ObsSynthSrc::FILTER_ENV1) == true);

    // Test global sources
    REQUIRE(isPerVoiceSource(ObsSynthSrc::LFO1) == false);
    REQUIRE(isPerVoiceSource(ObsSynthSrc::XY_X) == false);

    return true;
}

bool test_mod_dest_helpers()
{
    // Test oscillator index extraction
    REQUIRE(getOscIndex(ObsSynthDst::OSC1_PITCH) == 0);
    REQUIRE(getOscIndex(ObsSynthDst::OSC2_CUTOFF) == 1);
    REQUIRE(getOscIndex(ObsSynthDst::OSC3_LEVEL) == 2);
    REQUIRE(getOscIndex(ObsSynthDst::OSC4_PAN) == 3);
    REQUIRE(getOscIndex(ObsSynthDst::MIX_CH1_LVL) == -1);

    // Test parameter index extraction
    REQUIRE(getOscParamIndex(ObsSynthDst::OSC1_PITCH) == 0);
    REQUIRE(getOscParamIndex(ObsSynthDst::OSC1_CUTOFF) == 7);

    // Test getOscDest
    REQUIRE(getOscDest(0, 0) == ObsSynthDst::OSC1_PITCH);
    REQUIRE(getOscDest(1, 7) == ObsSynthDst::OSC2_CUTOFF);

    return true;
}

// ============================================================================
// OscillatorChain Tests
// ============================================================================

bool test_oscchain_construction()
{
    OscillatorChain chain(44100.0f);

    REQUIRE(chain.isActive() == false);
    REQUIRE(chain.getNote() == -1);

    return true;
}

bool test_oscchain_note_on_off()
{
    OscillatorChain chain(44100.0f);

    chain.noteOn(60, 0.8f); // Middle C

    REQUIRE(chain.isActive() == true);
    REQUIRE(chain.getNote() == 60);
    REQUIRE_NEAR(chain.getVelocity(), 0.8f, 0.001f);

    chain.noteOff();

    // Should still be active during release phase
    // (depends on release time, but should not immediately go idle)

    return true;
}

bool test_oscchain_generates_audio()
{
    OscillatorChain chain(44100.0f);

    chain.setWaveType(WaveType::SINE);
    chain.noteOn(69, 1.0f); // A4 = 440 Hz

    // Process some samples
    float energy = 0.0f;
    for (int i = 0; i < 1000; ++i)
    {
        float sample = chain.process();
        energy += sample * sample;
    }

    // Should have generated audio (non-zero energy)
    REQUIRE(energy > 0.0f);

    return true;
}

bool test_oscchain_filter_affects_sound()
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

    // Process and calculate energy
    float energy_bright = 0.0f;
    float energy_dark = 0.0f;

    for (int i = 0; i < 1000; ++i)
    {
        float s1 = chain_bright.process();
        float s2 = chain_dark.process();
        energy_bright += s1 * s1;
        energy_dark += s2 * s2;
    }

    // Bright should have more high-frequency content (more energy for sawtooth)
    // Actually, filtered sawtooth with low cutoff will have less overall energy
    REQUIRE(energy_bright > energy_dark);

    return true;
}

bool test_oscchain_stereo_output()
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
    REQUIRE_NEAR(left_energy, right_energy, left_energy * 0.1f);

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
    REQUIRE(left_energy > right_energy * 2.0f);

    return true;
}

bool test_oscchain_modulation_inputs()
{
    OscillatorChain chain(44100.0f);

    chain.noteOn(60, 1.0f);

    // Test pitch modulation
    chain.setPitchMod(12.0f); // +1 octave
    // Frequency should be doubled (approximately)

    // Test cutoff modulation
    chain.setCutoffMod(0.5f);

    // Test level modulation
    chain.setLevelMod(-0.5f);

    // Should still process without crashing
    float sample = chain.process();
    REQUIRE(std::isfinite(sample));

    return true;
}

// ============================================================================
// EffectSlot Tests
// ============================================================================

bool test_effectslot_construction()
{
    EffectSlot slot(44100.0f);

    REQUIRE(slot.hasEffect() == false);
    REQUIRE(slot.getEffectType() == EffectType::NONE);
    REQUIRE(slot.isBypassed() == false);
    REQUIRE_NEAR(slot.getMix(), 1.0f, 0.001f);

    return true;
}

bool test_effectslot_bypass()
{
    EffectSlot slot(44100.0f);

    slot.setBypassed(true);
    REQUIRE(slot.isBypassed() == true);

    // Bypassed slot should pass through unchanged
    float input = 0.5f;
    float output = slot.process(input);
    REQUIRE_NEAR(output, input, 0.0001f);

    return true;
}

bool test_effectslot_mix()
{
    EffectSlot slot(44100.0f);

    slot.setMix(0.5f);
    REQUIRE_NEAR(slot.getMix(), 0.5f, 0.001f);

    // Without effect, should still pass through
    float input = 0.5f;
    float output = slot.process(input);
    REQUIRE_NEAR(output, input, 0.0001f);

    return true;
}

// ============================================================================
// Test Runner
// ============================================================================

struct TestCase
{
    const char *name;
    bool (*func)();
};

int main()
{
    TestCase tests[] = {
        // BiquadFilter tests
        {"biquad_construction", test_biquad_construction},
        {"biquad_lowpass_attenuation", test_biquad_lowpass_attenuation},
        {"biquad_highpass_attenuation", test_biquad_highpass_attenuation},
        {"biquad_type_switching", test_biquad_type_switching},
        {"biquad_parameter_smoothing", test_biquad_parameter_smoothing},

        // ModMatrix tests
        {"modmatrix_construction", test_modmatrix_construction},
        {"modmatrix_add_route", test_modmatrix_add_route},
        {"modmatrix_multiple_routes", test_modmatrix_multiple_routes},
        {"modmatrix_process", test_modmatrix_process},
        {"modmatrix_accumulation", test_modmatrix_accumulation},
        {"modmatrix_negative_amount", test_modmatrix_negative_amount},
        {"modmatrix_remove_route", test_modmatrix_remove_route},
        {"modmatrix_clear", test_modmatrix_clear},

        // ModSource/ModDest tests
        {"mod_source_properties", test_mod_source_properties},
        {"mod_dest_helpers", test_mod_dest_helpers},

        // OscillatorChain tests
        {"oscchain_construction", test_oscchain_construction},
        {"oscchain_note_on_off", test_oscchain_note_on_off},
        {"oscchain_generates_audio", test_oscchain_generates_audio},
        {"oscchain_filter_affects_sound", test_oscchain_filter_affects_sound},
        {"oscchain_stereo_output", test_oscchain_stereo_output},
        {"oscchain_modulation_inputs", test_oscchain_modulation_inputs},

        // EffectSlot tests
        {"effectslot_construction", test_effectslot_construction},
        {"effectslot_bypass", test_effectslot_bypass},
        {"effectslot_mix", test_effectslot_mix},
    };

    int passed = 0;
    int failed = 0;

    std::printf("\n=== Synth Core Unit Tests (Phase 1/2) ===\n\n");

    for (const auto &test : tests)
    {
        std::printf("  %-35s ", test.name);

        bool result = test.func();

        if (result)
        {
            std::printf("[PASS]\n");
            passed++;
        }
        else
        {
            std::printf("[FAIL]\n");
            failed++;
        }
    }

    std::printf("\n-----------------------------------------\n");
    std::printf("Results: %d passed, %d failed\n", passed, failed);
    std::printf("-----------------------------------------\n\n");

    if (failed > 0)
    {
        std::printf("SOME TESTS FAILED!\n");
        return 1;
    }

    std::printf("All synth core tests passed.\n");
    return 0;
}
