#include "../lib/effects/overdrive.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include <iostream>
#include <vector>

using namespace synth;

int main()
{
    const float sample_rate = 48000.0f;
    const float duration = 3.0f; // 3 seconds
    const size_t sample_count = static_cast<size_t>(sample_rate * duration);

    // Create oscillator for test signal (440Hz A note)
    Oscillator osc(440.0f, WaveType::SINE, sample_rate);
    osc.setAmplitude(0.8f); // Strong input signal to trigger overdrive

    // Create overdrive effect instances with different settings
    Overdrive light_overdrive(2.0f, 0.7f, 1.0f); // Light overdrive
    Overdrive heavy_overdrive(5.0f, 0.5f, 1.0f); // Heavy overdrive
    Overdrive mixed_overdrive(3.0f, 0.6f, 0.5f); // 50% dry/wet mix

    // Generate clean signal
    std::vector<float> clean_signal(sample_count);
    for (size_t i = 0; i < sample_count; ++i)
    {
        clean_signal[i] = osc.getSample();
    }

    // Process with different overdrive settings
    std::vector<float> light_output(sample_count);
    std::vector<float> heavy_output(sample_count);
    std::vector<float> mixed_output(sample_count);

    light_overdrive.process(clean_signal.data(), light_output.data(), sample_count);
    heavy_overdrive.process(clean_signal.data(), heavy_output.data(), sample_count);
    mixed_overdrive.process(clean_signal.data(), mixed_output.data(), sample_count);

    // Write WAV files
    std::cout << "Writing clean signal to clean_signal.wav..." << std::endl;
    WAVWriter::writeFileSimple("clean_signal.wav", clean_signal, static_cast<int>(sample_count), static_cast<int>(sample_rate));

    std::cout << "Writing light overdrive to light_overdrive.wav..." << std::endl;
    WAVWriter::writeFileSimple("light_overdrive.wav", light_output, static_cast<int>(sample_count), static_cast<int>(sample_rate));

    std::cout << "Writing heavy overdrive to heavy_overdrive.wav..." << std::endl;
    WAVWriter::writeFileSimple("heavy_overdrive.wav", heavy_output, static_cast<int>(sample_count), static_cast<int>(sample_rate));

    std::cout << "Writing mixed overdrive to mixed_overdrive.wav..." << std::endl;
    WAVWriter::writeFileSimple("mixed_overdrive.wav", mixed_output, static_cast<int>(sample_count), static_cast<int>(sample_rate));

    // Display some statistics
    std::cout << "\nOverdrive Effect Demo Complete!" << std::endl;
    std::cout << "Settings used:" << std::endl;
    std::cout << "Light Overdrive - Drive: " << light_overdrive.get_drive()
              << ", Output: " << light_overdrive.get_output_level()
              << ", Mix: " << light_overdrive.get_mix() << std::endl;
    std::cout << "Heavy Overdrive - Drive: " << heavy_overdrive.get_drive()
              << ", Output: " << heavy_overdrive.get_output_level()
              << ", Mix: " << heavy_overdrive.get_mix() << std::endl;
    std::cout << "Mixed Overdrive - Drive: " << mixed_overdrive.get_drive()
              << ", Output: " << mixed_overdrive.get_output_level()
              << ", Mix: " << mixed_overdrive.get_mix() << std::endl;

    return 0;
}
