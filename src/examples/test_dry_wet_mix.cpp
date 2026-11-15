#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>

using namespace synth;

int main()
{
    std::cout << "Dry/Wet Mix Test\n";
    std::cout << "================\n";

    const float SAMPLE_RATE = 48000.0f;
    const int TEST_DURATION_SECONDS = 6; // 6 seconds total
    const int TOTAL_SAMPLES = static_cast<int>(SAMPLE_RATE * TEST_DURATION_SECONDS);

    // Create polyphonic oscillator
    auto oscillator = std::make_unique<PolyphonicOscillator>(WaveType::SINE, SAMPLE_RATE);

    // Set ADSR parameters for clear envelope effect
    oscillator->setADSR(0.2f, 0.3f, 0.7f, 0.8f); // Pronounced envelope
    std::cout << "ADSR set: Attack=0.2s, Decay=0.3s, Sustain=70%, Release=0.8s\n\n";

    // Prepare audio buffer
    std::vector<float> audioBuffer(TOTAL_SAMPLES * 2, 0.0f); // Stereo
    int sampleIndex = 0;

    // Test different dry/wet mix settings with the same note sequence
    std::vector<std::pair<float, std::string>> mixSettings = {
        {0.0f, "100% Dry (no envelope)"},
        {0.33f, "33% Wet (subtle envelope)"},
        {0.66f, "66% Wet (pronounced envelope)"},
        {1.0f, "100% Wet (full envelope)"}};

    for (size_t settingIndex = 0; settingIndex < mixSettings.size(); ++settingIndex)
    {
        float mixValue = mixSettings[settingIndex].first;
        const std::string &description = mixSettings[settingIndex].second;

        std::cout << "Section " << (settingIndex + 1) << ": " << description << " (mix=" << mixValue << ")\n";
        oscillator->setDryWetMix(mixValue);

        // Generate 1.5 seconds of audio for this setting
        int sectionSamples = static_cast<int>(SAMPLE_RATE * 1.5f);

        // Play a note sequence to demonstrate the mix effect
        std::vector<std::pair<float, uint8_t>> noteEvents = {
            {0.0f, static_cast<uint8_t>(60)}, // C4 at start
            {0.5f, static_cast<uint8_t>(64)}, // E4 at 0.5s
            {1.0f, static_cast<uint8_t>(67)}  // G4 at 1.0s
        };

        for (int sample = 0; sample < sectionSamples; ++sample)
        {
            float timeInSection = static_cast<float>(sample) / SAMPLE_RATE;

            // Trigger note events
            for (const auto &event : noteEvents)
            {
                if (std::abs(timeInSection - event.first) < (1.0f / SAMPLE_RATE))
                {
                    oscillator->noteOn(event.second, 0.8f, 0.0f);
                    std::cout << "  t=" << timeInSection << "s: Note ON - " << static_cast<int>(event.second) << "\n";
                }
            }

            // Stop all notes at 1.2s to hear release
            if (std::abs(timeInSection - 1.2f) < (1.0f / SAMPLE_RATE))
            {
                oscillator->allNotesOff();
                std::cout << "  t=" << timeInSection << "s: All notes OFF (release phase)\n";
            }

            // Generate stereo sample
            StereoSample sample_data = oscillator->getStereoSample();

            if (sampleIndex < TOTAL_SAMPLES)
            {
                audioBuffer[sampleIndex * 2] = sample_data.left;      // Left channel
                audioBuffer[sampleIndex * 2 + 1] = sample_data.right; // Right channel
                sampleIndex++;
            }
        }

        std::cout << "\n";
    }

    // Write to WAV file
    WAVConfig config(static_cast<uint32_t>(SAMPLE_RATE), 2, 16); // Stereo, 16-bit

    int result = WAVWriter::writeFile("dry_wet_mix_test.wav", audioBuffer, TOTAL_SAMPLES, config);

    if (result == 0)
    {
        std::cout << "✓ Successfully wrote 'dry_wet_mix_test.wav'\n";
        std::cout << "  Duration: " << TEST_DURATION_SECONDS << " seconds\n";
        std::cout << "  Sample rate: " << SAMPLE_RATE << " Hz\n";
        std::cout << "  Channels: Stereo\n";
        std::cout << "  File size: " << (audioBuffer.size() * sizeof(int16_t)) << " bytes\n\n";

        std::cout << "Listen to the file to hear:\n";
        std::cout << "  - Section 1 (0-1.5s): 100% Dry - constant volume, no envelope effect\n";
        std::cout << "  - Section 2 (1.5-3s): 33% Wet - subtle envelope shaping\n";
        std::cout << "  - Section 3 (3-4.5s): 66% Wet - pronounced envelope shaping\n";
        std::cout << "  - Section 4 (4.5-6s): 100% Wet - full ADSR envelope control\n\n";

        std::cout << "Each section plays C4-E4-G4 chord progression with release at 1.2s\n";
    }
    else
    {
        std::cout << "✗ Failed to write WAV file\n";
        return 1;
    }

    std::cout << "\n✓ Dry/Wet Mix test completed successfully!\n";
    std::cout << "The polyphonic oscillator now supports:\n";
    std::cout << "  - Dry/wet mix control (0.0f = 100% dry, 1.0f = 100% wet)\n";
    std::cout << "  - Seamless blending between raw and envelope-processed signals\n";
    std::cout << "  - Per-voice envelope processing with global mix control\n";
    std::cout << "  - Real-time mix parameter adjustment\n";

    return 0;
}
