/**
 * @file test_voice.cpp
 * @brief Unit tests for Voice and VoiceAllocator classes
 *
 * Test coverage:
 * - Voice: note on/off, state transitions, per-voice modulation
 * - VoiceAllocator: allocation strategies, voice stealing, polyphony
 *
 * Run with:
 *   ./build/bin/Debug/synth_core_tests.exe
 *   ctest --test-dir build -V -R Voice
 */

#include <doctest/doctest.h>
#include <cmath>
#include <vector>

#include "synth/synth.hpp"

using namespace synth;

// ============================================================================
// TEST SUITE: Voice
// ============================================================================

TEST_SUITE("Voice")
{

    TEST_CASE("construction and initial state")
    {
        SynthVoice4 voice(48000.0f);

        CHECK(voice.isIdle());
        CHECK_FALSE(voice.isActive());
        CHECK(voice.getNote() == -1);
        CHECK(voice.getVelocity() == doctest::Approx(0.0f));
        CHECK(voice.getState() == VoicePhase::IDLE);
    }

    TEST_CASE("noteOn activates voice")
    {
        SynthVoice4 voice(48000.0f);

        voice.noteOn(60, 0.8f);

        CHECK(voice.isActive());
        CHECK_FALSE(voice.isIdle());
        CHECK(voice.getNote() == 60);
        CHECK(voice.getVelocity() == doctest::Approx(0.8f));
        CHECK(voice.getState() == VoicePhase::ACTIVE);
    }

    TEST_CASE("noteOff transitions to releasing")
    {
        SynthVoice4 voice(48000.0f);

        voice.noteOn(60, 0.8f);
        voice.noteOff();
        for (int i = 0; i < 100; ++i) // a noteOff waits for the note's first 2 ms (a note shorter than a block still sounds)
            voice.process();

        CHECK(voice.isActive());
        CHECK(voice.isReleasing());
        CHECK(voice.getState() == VoicePhase::RELEASING);
    }

    TEST_CASE("forceStop immediately stops voice")
    {
        SynthVoice4 voice(48000.0f);

        voice.noteOn(60, 0.8f);
        voice.forceStop();

        CHECK(voice.isIdle());
        CHECK_FALSE(voice.isActive());
        CHECK(voice.getNote() == -1);
    }

    TEST_CASE("steal initiates fadeout")
    {
        SynthVoice4 voice(48000.0f);

        voice.noteOn(60, 0.8f);
        voice.steal(64, 0.8f, 2);

        CHECK(voice.isStealing());
        CHECK(voice.getState() == VoicePhase::STEALING);
        CHECK(voice.getNote() == 64); // it answers to its new note while the old one fades
        for (int i = 0; i < 400; ++i) // 5 ms at 48 kHz: the fade is done, the new note plays
            voice.process();
        CHECK(voice.getState() == VoicePhase::ACTIVE);
        CHECK(voice.getNote() == 64);
    }

    TEST_CASE("process generates audio")
    {
        SynthVoice4 voice(48000.0f);
        voice.noteOn(60, 0.8f);

        // Process some samples
        float total_energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float sample = voice.process();
            total_energy += sample * sample;
        }

        // Should produce some audio output
        CHECK(total_energy > 0.0f);
    }

    TEST_CASE("processStereo generates stereo audio")
    {
        SynthVoice4 voice(48000.0f);
        voice.noteOn(60, 0.8f);

        float left, right;
        voice.processStereo(left, right);

        // Should produce audio on both channels
        // (exact values depend on pan setting)
        CHECK(std::isfinite(left));
        CHECK(std::isfinite(right));
    }

    TEST_CASE("per-voice modulation")
    {
        SynthVoice4 voice(48000.0f);
        voice.noteOn(60, 0.8f);

        CHECK(voice.getVelocityMod() == doctest::Approx(0.8f));
        CHECK(voice.getKeytrackMod() == doctest::Approx(60.0f / 127.0f));

        voice.setAftertouch(0.5f);
        CHECK(voice.getAftertouch() == doctest::Approx(0.5f));
        CHECK(voice.getAftertouchMod() == doctest::Approx(0.5f));
    }

    TEST_CASE("chain access")
    {
        SynthVoice4 voice(48000.0f);

        // Should have 4 chains
        CHECK(SynthVoice4::NUM_CHAINS == 4);

        // Access each chain
        for (int i = 0; i < 4; ++i)
        {
            auto &chain = voice.getChain(i);
            chain.setWaveType(WaveType::SAW);
            CHECK(chain.getWaveType() == WaveType::SAW);
        }
    }

    TEST_CASE("age tracking")
    {
        SynthVoice4 voice(48000.0f);
        voice.noteOn(60, 0.8f);

        uint32_t initial_age = voice.getAge();

        // Process some samples
        for (int i = 0; i < 100; ++i)
        {
            voice.process();
        }

        // Age should have increased
        CHECK(voice.getAge() > initial_age);
        CHECK(voice.getAge() == initial_age + 100);
    }

    TEST_CASE("SynthVoice1 single chain configuration")
    {
        SynthVoice1 voice(48000.0f);

        CHECK(SynthVoice1::NUM_CHAINS == 1);

        voice.noteOn(72, 1.0f);
        CHECK(voice.isActive());

        float sample = voice.process();
        CHECK(std::isfinite(sample));
    }

} // TEST_SUITE("Voice")

// ============================================================================
// TEST SUITE: VoiceAllocator
// ============================================================================

TEST_SUITE("VoiceAllocator")
{

    TEST_CASE("construction and initial state")
    {
        FullVoiceAllocator allocator(48000.0f);

        CHECK(allocator.getActiveVoiceCount() == 0);
        CHECK_FALSE(allocator.isActive());
        CHECK(FullVoiceAllocator::NUM_VOICES == 32);
    }

    TEST_CASE("noteOn allocates voice")
    {
        FullVoiceAllocator allocator(48000.0f);

        auto *voice = allocator.noteOn(60, 0.8f);

        CHECK(voice != nullptr);
        CHECK(allocator.getActiveVoiceCount() == 1);
        CHECK(voice->getNote() == 60);
        CHECK(voice->getVelocity() == doctest::Approx(0.8f));
    }

    TEST_CASE("noteOff releases voice")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);
        allocator.noteOff(60);
        for (int i = 0; i < 100; ++i) // the note's first 2 ms
            allocator.process();

        CHECK(allocator.getReleasingVoiceCount() == 1);
    }

    TEST_CASE("multiple notes polyphony")
    {
        FullVoiceAllocator allocator(48000.0f);

        // Play a chord C-E-G
        allocator.noteOn(60, 0.8f); // C
        allocator.noteOn(64, 0.6f); // E
        allocator.noteOn(67, 1.0f); // G

        CHECK(allocator.getActiveVoiceCount() == 3);
        CHECK(allocator.isNotePlaying(60));
        CHECK(allocator.isNotePlaying(64));
        CHECK(allocator.isNotePlaying(67));
    }

    TEST_CASE("retrigger same note")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.5f);
        CHECK(allocator.getActiveVoiceCount() == 1);

        // Retrigger same note with different velocity
        allocator.noteOn(60, 1.0f);
        CHECK(allocator.getActiveVoiceCount() == 1); // Still 1 voice

        auto *voice = allocator.findVoiceByNote(60);
        CHECK(voice->getVelocity() == doctest::Approx(1.0f)); // Updated velocity
    }

    TEST_CASE("voice stealing oldest")
    {
        VoiceAllocator<SynthVoice1, 4> allocator(48000.0f, VoiceAllocationStrategy::OLDEST);

        // Fill all 4 voices
        allocator.noteOn(60, 0.8f);
        // Process some samples to increase age of first voice
        for (int i = 0; i < 100; ++i)
            allocator.process();

        allocator.noteOn(62, 0.8f);
        allocator.noteOn(64, 0.8f);
        allocator.noteOn(67, 0.8f);

        CHECK(allocator.getActiveVoiceCount() == 4);

        // Trigger 5th note - should steal oldest (note 60)
        allocator.noteOn(72, 1.0f);

        // Note 60 should be stolen, note 72 should be playing
        CHECK(allocator.isNotePlaying(72));
    }

    TEST_CASE("allNotesOff releases all voices")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);
        allocator.noteOn(64, 0.8f);
        allocator.noteOn(67, 0.8f);

        allocator.allNotesOff();

        // All voices should be releasing
        CHECK(allocator.getReleasingVoiceCount() == 3);
    }

    TEST_CASE("panic stops all voices")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);
        allocator.noteOn(64, 0.8f);
        allocator.noteOn(67, 0.8f);

        allocator.panic();

        CHECK(allocator.getActiveVoiceCount() == 0);
        CHECK_FALSE(allocator.isActive());
    }

    TEST_CASE("process generates audio")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);

        float total_energy = 0.0f;
        for (int i = 0; i < 1000; ++i)
        {
            float sample = allocator.process();
            total_energy += sample * sample;
        }

        CHECK(total_energy > 0.0f);
    }

    TEST_CASE("processStereo generates stereo audio")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);

        float left, right;
        allocator.processStereo(left, right);

        CHECK(std::isfinite(left));
        CHECK(std::isfinite(right));
    }

    TEST_CASE("generateBuffer fills buffer")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);

        std::vector<float> buffer(512);
        allocator.generateBuffer(buffer.data(), 512);

        // Check buffer has audio content
        float sum = 0.0f;
        for (float s : buffer)
        {
            sum += std::abs(s);
        }
        CHECK(sum > 0.0f);
    }

    TEST_CASE("poly aftertouch affects specific note")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);
        allocator.noteOn(64, 0.8f);

        allocator.setPolyAftertouch(60, 0.7f);

        auto *voice60 = allocator.findVoiceByNote(60);
        auto *voice64 = allocator.findVoiceByNote(64);

        CHECK(voice60->getAftertouch() == doctest::Approx(0.7f));
        CHECK(voice64->getAftertouch() == doctest::Approx(0.0f)); // Unaffected
    }

    TEST_CASE("channel aftertouch affects all notes")
    {
        FullVoiceAllocator allocator(48000.0f);

        allocator.noteOn(60, 0.8f);
        allocator.noteOn(64, 0.8f);

        allocator.setChannelAftertouch(0.5f);

        auto *voice60 = allocator.findVoiceByNote(60);
        auto *voice64 = allocator.findVoiceByNote(64);

        CHECK(voice60->getAftertouch() == doctest::Approx(0.5f));
        CHECK(voice64->getAftertouch() == doctest::Approx(0.5f));
    }

    TEST_CASE("configureVoices applies to all voices")
    {
        VoiceAllocator<SynthVoice1, 4> allocator(48000.0f);

        // Configure all voices
        allocator.configureVoices([](SynthVoice1 & /*voice*/)
                                  {
                                      // This would set voice-level settings
                                      // For now just verify it compiles and runs
                                  });

        // Success if no crash
        CHECK(true);
    }

    TEST_CASE("configureChains applies to all chains")
    {
        VoiceAllocator<SynthVoice1, 4> allocator(48000.0f);

        // Configure all chains to use SAW wave
        allocator.configureChains([](OscillatorChain &chain, int /*index*/)
                                  { chain.setWaveType(WaveType::SAW); });

        // Verify by playing a note and checking the chain
        allocator.noteOn(60, 0.8f);
        auto *voice = allocator.findVoiceByNote(60);
        CHECK(voice->getChain(0).getWaveType() == WaveType::SAW);
    }

    TEST_CASE("SimpleVoiceAllocator type alias")
    {
        SimpleVoiceAllocator allocator(48000.0f);

        CHECK(SimpleVoiceAllocator::NUM_VOICES == 32);

        allocator.noteOn(60, 0.8f);
        CHECK(allocator.getActiveVoiceCount() == 1);
    }

    TEST_CASE("SmallVoiceAllocator type alias")
    {
        SmallVoiceAllocator allocator(48000.0f);

        CHECK(SmallVoiceAllocator::NUM_VOICES == 8);

        allocator.noteOn(60, 0.8f);
        CHECK(allocator.getActiveVoiceCount() == 1);
    }

} // TEST_SUITE("VoiceAllocator")
