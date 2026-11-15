#include "../lib/effects/reverb.hpp"
#include "../app/common/audio_constants.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include "../lib/math/synt_math.hpp"
#include <iostream>
#include <memory>

using namespace synth;

int main()
{
    std::cout << "Reverb Effect Test - Generating demonstration WAV files\n";

    const int duration_samples = SAMPLE_RATE * 2; // 2 seconds
    const float frequency = 440.0f;               // A4

    // Create test signal (sine wave with exponential decay)
    std::vector<float> dry_signal(duration_samples);
    std::vector<float> wet_signal(duration_samples);

    // Generate a short burst of sine wave (like a struck bell)
    for (int i = 0; i < duration_samples; ++i)
    {
        float t = static_cast<float>(i) / SAMPLE_RATE;
        float envelope = math::exp(-t * 3.0f); // Exponential decay
        float sample = math::sin(2.0f * 3.14159265f * frequency * t) * envelope * 0.5f;

        // Only play for first 0.1 seconds (like a bell strike)
        if (t < 0.1f)
        {
            dry_signal[i] = sample;
        }
        else
        {
            dry_signal[i] = 0.0f;
        }
    }

    // Create reverb processor
    auto reverb = std::make_unique<SchroederReverb>(static_cast<float>(SAMPLE_RATE));

    // Test different presets
    std::vector<std::pair<std::string, SchroederReverb::Preset>> presets = {
        {"small_room", SchroederReverb::createSmallRoomPreset()},
        {"medium_room", SchroederReverb::createMediumRoomPreset()},
        {"large_room", SchroederReverb::createLargeRoomPreset()},
        {"hall", SchroederReverb::createHallPreset()}};

    // WAV configuration
    WAVConfig config = WAVWriter::createConfig(SAMPLE_RATE, WAV_CHANNELS_MONO, WAV_BITS_PER_SAMPLE_16);

    // Save dry signal
    WAVWriter::writeFile("reverb_test_dry.wav", dry_signal, duration_samples, config);
    std::cout << "Generated: reverb_test_dry.wav (dry signal)\n";

    // Process and save each preset
    for (const auto &preset_pair : presets)
    {
        const std::string &name = preset_pair.first;
        const SchroederReverb::Preset &preset = preset_pair.second;

        // Apply preset
        reverb->applyPreset(preset);
        reverb->clear(); // Clear any previous state

        // Process the signal
        wet_signal = dry_signal; // Copy dry signal
        reverb->processBuffer(wet_signal.data(), duration_samples);

        // Save to file
        std::string filename = "reverb_test_" + name + ".wav";
        WAVWriter::writeFile(filename, wet_signal, duration_samples, config);

        std::cout << "Generated: " << filename
                  << " (Room: " << (preset.room_size * 100.0f) << "%, "
                  << "Damping: " << (preset.damping * 100.0f) << "%, "
                  << "Wet: " << (preset.wet_level * 100.0f) << "%)\n";
    }

    // Test pure wet signal (no dry)
    reverb->applyPreset(SchroederReverb::createLargeRoomPreset());
    reverb->setWetLevel(1.0f);
    reverb->setDryLevel(0.0f);
    reverb->clear();

    wet_signal = dry_signal;
    reverb->processBuffer(wet_signal.data(), duration_samples);
    WAVWriter::writeFile("reverb_test_wet_only.wav", wet_signal, duration_samples, config);
    std::cout << "Generated: reverb_test_wet_only.wav (wet signal only)\n";

    std::cout << "\nReverb test complete! Listen to the generated WAV files to hear the effect.\n";
    std::cout << "You should hear:\n";
    std::cout << "- dry.wav: Short bell-like tone\n";
    std::cout << "- small_room.wav: Tight, controlled reverb\n";
    std::cout << "- medium_room.wav: Balanced, musical reverb\n";
    std::cout << "- large_room.wav: Spacious, open reverb\n";
    std::cout << "- hall.wav: Cathedral-like, long decay\n";
    std::cout << "- wet_only.wav: Pure reverb tail without direct signal\n";

    return 0;
}
