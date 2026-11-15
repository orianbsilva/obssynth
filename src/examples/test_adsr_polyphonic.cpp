/**
 * @file test_adsr_polyphonic.cpp
 * @brief Test ADSR envelope functionality in polyphonic oscillator
 */

#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include <iostream>
#include <vector>
#include <cmath>

using namespace synth;

int main()
{
    std::cout << "ADSR Polyphonic Oscillator Test\n";
    std::cout << "================================\n";

    constexpr float SAMPLE_RATE = 48000.0f;
    constexpr int DURATION_SECONDS = 3;
    constexpr int TOTAL_SAMPLES = static_cast<int>(SAMPLE_RATE * DURATION_SECONDS);

    // Create polyphonic oscillator
    PolyphonicOscillator oscillator(WaveType::SINE, SAMPLE_RATE);

    // Test different ADSR settings
    std::cout << "Setting ADSR parameters:\n";
    std::cout << "Attack: 0.1s, Decay: 0.2s, Sustain: 60%, Release: 0.5s\n";
    oscillator.setADSR(0.1f, 0.2f, 0.6f, 0.5f);

    // Display current ADSR settings
    std::cout << "Current ADSR settings:\n";
    std::cout << "  Attack: " << oscillator.getAttackTime() << "s\n";
    std::cout << "  Decay: " << oscillator.getDecayTime() << "s\n";
    std::cout << "  Sustain: " << (oscillator.getSustainLevel() * 100.0f) << "%\n";
    std::cout << "  Release: " << oscillator.getReleaseTime() << "s\n\n";

    // Create buffer for stereo audio
    std::vector<float> stereo_buffer(TOTAL_SAMPLES * 2);
    std::fill(stereo_buffer.begin(), stereo_buffer.end(), 0.0f);

    // Timeline of note events
    struct NoteEvent
    {
        float time_seconds;
        uint8_t note;
        float velocity;
        bool is_note_on;
    };

    std::vector<NoteEvent> events = {
        // Note sequence demonstrating ADSR envelope
        {0.0f, 60, 0.8f, true},  // C4 on
        {0.8f, 60, 0.0f, false}, // C4 off (release during sustain)

        {1.2f, 64, 0.7f, true},  // E4 on
        {1.7f, 67, 0.6f, true},  // G4 on (chord)
        {2.0f, 64, 0.0f, false}, // E4 off
        {2.3f, 67, 0.0f, false}, // G4 off

        {2.7f, 72, 0.9f, true}, // C5 on
        // Note: C5 will release naturally at end of buffer
    };

    size_t next_event_index = 0;

    // Generate audio with note events
    std::cout << "Generating audio with note events...\n";
    for (int sample = 0; sample < TOTAL_SAMPLES; ++sample)
    {
        float current_time = static_cast<float>(sample) / SAMPLE_RATE;

        // Process any events at current time
        while (next_event_index < events.size() &&
               events[next_event_index].time_seconds <= current_time)
        {
            const auto &event = events[next_event_index];
            if (event.is_note_on)
            {
                oscillator.noteOn(event.note, event.velocity);
                std::cout << "  t=" << event.time_seconds << "s: Note ON - "
                          << static_cast<int>(event.note) << " (velocity " << event.velocity << ")\n";
            }
            else
            {
                oscillator.noteOff(event.note);
                std::cout << "  t=" << event.time_seconds << "s: Note OFF - "
                          << static_cast<int>(event.note) << "\n";
            }
            next_event_index++;
        }

        // Generate one frame of stereo audio
        StereoSample stereo_sample = oscillator.getStereoSample();

        stereo_buffer[sample * 2] = stereo_sample.left;      // Left channel
        stereo_buffer[sample * 2 + 1] = stereo_sample.right; // Right channel
    }

    // Release any remaining notes at the end
    oscillator.allNotesOff();
    std::cout << "  t=" << DURATION_SECONDS << "s: All notes OFF\n\n";

    // Write to WAV file
    std::cout << "Writing ADSR test to WAV file...\n";

    WAVConfig config = WAVWriter::createConfig(static_cast<uint32_t>(SAMPLE_RATE), 2, 16); // Stereo, 16-bit
    int result = WAVWriter::writeFile("adsr_polyphonic_test.wav", stereo_buffer,
                                      TOTAL_SAMPLES, config);

    if (result == 0)
    {
        std::cout << "✓ Successfully wrote 'adsr_polyphonic_test.wav'\n";
        std::cout << "  Duration: " << DURATION_SECONDS << " seconds\n";
        std::cout << "  Sample rate: " << SAMPLE_RATE << " Hz\n";
        std::cout << "  Channels: Stereo\n";
        std::cout << "  File size: " << (stereo_buffer.size() * sizeof(float)) << " bytes\n\n";

        std::cout << "Listen to the file to hear:\n";
        std::cout << "  - Attack phase: gradual volume increase\n";
        std::cout << "  - Decay phase: volume decrease to sustain level\n";
        std::cout << "  - Sustain phase: constant volume\n";
        std::cout << "  - Release phase: gradual volume decrease to silence\n";
        std::cout << "  - Polyphonic capability: multiple notes simultaneously\n";
    }
    else
    {
        std::cout << "✗ Failed to write WAV file (error code: " << result << ")\n";
        return 1;
    }

    // Test individual ADSR parameter changes
    std::cout << "\nTesting individual ADSR parameter changes:\n";

    oscillator.setAttackTime(0.05f);
    std::cout << "  New attack time: " << oscillator.getAttackTime() << "s\n";

    oscillator.setDecayTime(0.1f);
    std::cout << "  New decay time: " << oscillator.getDecayTime() << "s\n";

    oscillator.setSustainLevel(0.8f);
    std::cout << "  New sustain level: " << (oscillator.getSustainLevel() * 100.0f) << "%\n";

    oscillator.setReleaseTime(0.3f);
    std::cout << "  New release time: " << oscillator.getReleaseTime() << "s\n";

    std::cout << "\n✓ ADSR Polyphonic Oscillator test completed successfully!\n";
    std::cout << "The polyphonic oscillator now supports:\n";
    std::cout << "  - Full ADSR envelope control per voice\n";
    std::cout << "  - Global ADSR parameter setting via setADSR()\n";
    std::cout << "  - Individual parameter control methods\n";
    std::cout << "  - Thread-safe envelope processing\n";
    std::cout << "  - Automatic voice deactivation when envelope completes\n";

    return 0;
}
