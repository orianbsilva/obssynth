#include "../lib/effects/chorus.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include "../app/common/audio_constants.hpp"
#include <iostream>
#include <vector>
#include <memory>

using namespace synth;

int main()
{
    std::cout << "Chorus Effect Test\n";
    std::cout << "================\n\n";

    // Test parameters
    const float sample_rate = static_cast<float>(SAMPLE_RATE);
    const float frequency = 440.0f;                                    // A4
    const int duration_samples = static_cast<int>(sample_rate * 3.0f); // 3 seconds

    // Create oscillator and chorus
    auto oscillator = std::make_unique<Oscillator>(frequency, WaveType::SINE, sample_rate);
    auto chorus = std::make_unique<Chorus>(sample_rate);

    // Configure chorus with medium settings
    chorus->setMix(0.5f);      // 50% wet
    chorus->setDepth(0.6f);    // 60% depth
    chorus->setRate(0.5f);     // 0.5 Hz rate
    chorus->setFeedback(0.3f); // 30% feedback

    std::cout << "Generating test audio...\n";
    std::cout << "Duration: 3 seconds\n";
    std::cout << "Sample Rate: " << sample_rate << " Hz\n";
    std::cout << "Frequency: " << frequency << " Hz\n\n";

    std::cout << "Chorus Settings:\n";
    std::cout << "  Mix: " << (chorus->getMix() * 100.0f) << "%\n";
    std::cout << "  Depth: " << (chorus->getDepth() * 100.0f) << "%\n";
    std::cout << "  Rate: " << chorus->getRate() << " Hz\n";
    std::cout << "  Feedback: " << (chorus->getFeedback() * 100.0f) << "%\n\n";

    // Generate audio buffers
    std::vector<float> dry_buffer(duration_samples);
    std::vector<float> chorus_buffer(duration_samples);

    // Generate dry signal
    oscillator->generateBuffer(dry_buffer.data(), duration_samples);

    // Copy dry signal for chorus processing
    std::copy(dry_buffer.begin(), dry_buffer.end(), chorus_buffer.begin());

    // Apply chorus effect
    chorus->processBuffer(chorus_buffer.data(), duration_samples);

    // Create WAV configuration
    WAVConfig config = WAVWriter::createConfig(static_cast<int>(sample_rate), WAV_CHANNELS_MONO, WAV_BITS_PER_SAMPLE_16);

    // Write dry signal
    std::string dry_filename = "chorus_test_dry.wav";
    int dry_result = WAVWriter::writeFile(dry_filename, dry_buffer, duration_samples, config);

    // Write chorus-processed signal
    std::string chorus_filename = "chorus_test_chorus.wav";
    int chorus_result = WAVWriter::writeFile(chorus_filename, chorus_buffer, duration_samples, config);

    // Report results
    if (dry_result == 0 && chorus_result == 0)
    {
        float duration = WAVWriter::calculateDuration(duration_samples, static_cast<int>(sample_rate));
        int dry_file_size = WAVWriter::calculateFileSize(duration_samples, config);
        int chorus_file_size = WAVWriter::calculateFileSize(duration_samples, config);

        std::cout << "✅ Audio files generated successfully!\n\n";
        std::cout << "Files created:\n";
        std::cout << "  " << dry_filename << " (dry signal)\n";
        std::cout << "    Duration: " << duration << " seconds\n";
        std::cout << "    Samples: " << duration_samples << "\n";
        std::cout << "    File size: " << dry_file_size << " bytes\n\n";

        std::cout << "  " << chorus_filename << " (with chorus effect)\n";
        std::cout << "    Duration: " << duration << " seconds\n";
        std::cout << "    Samples: " << duration_samples << "\n";
        std::cout << "    File size: " << chorus_file_size << " bytes\n\n";

        std::cout << "Compare the files to hear the difference:\n";
        std::cout << "• Dry signal: Pure sine wave\n";
        std::cout << "• Chorus signal: Wider, richer sound with modulation\n\n";

        std::cout << "Test different chorus settings by modifying the parameters and re-running.\n";
    }
    else
    {
        std::cout << "❌ Failed to write audio files!\n";
        if (dry_result != 0)
            std::cout << "  Dry file error: " << dry_result << "\n";
        if (chorus_result != 0)
            std::cout << "  Chorus file error: " << chorus_result << "\n";
        return -1;
    }

    return 0;
}
