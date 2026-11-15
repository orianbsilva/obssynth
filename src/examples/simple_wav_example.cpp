#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include <iostream>
#include <vector>
#include <memory>

using namespace synth;

int main()
{
  std::cout << "Simple WAV File Generator\n";
  std::cout << "========================\n\n";

  // Configuration
  constexpr float sample_rate = 48000.0f;
  constexpr int duration_seconds = 3;
  constexpr int total_samples = static_cast<int>(sample_rate * duration_seconds);

  std::cout << "Generating " << duration_seconds << " seconds of audio...\n";
  std::cout << "Sample rate: " << sample_rate << " Hz\n";
  std::cout << "Total samples: " << total_samples << "\n\n";

  // Create different wave types
  WaveType wave_types[] = {WaveType::SINE, WaveType::SAW, WaveType::SQUARE};
  const char *wave_names[] = {"sine", "sawtooth", "square"};
  const char *filenames[] = {"sine_wave.wav", "sawtooth_wave.wav", "square_wave.wav"};

  for (int wave_idx = 0; wave_idx < 3; wave_idx++)
  {
    std::cout << "Generating " << wave_names[wave_idx] << " wave at 440 Hz...\n";

    // Create oscillator
    auto oscillator = std::make_unique<PolyphonicOscillator>(wave_types[wave_idx], sample_rate);
    oscillator->setMasterAmplitude(0.7f); // 70% volume

    // Play a C Major chord (C4, E4, G4) - MIDI notes 60, 64, 67
    oscillator->noteOn(60, 1.0f); // C4 = 261.63 Hz
    oscillator->noteOn(63, 0.8f); // Eb4 = 329.63 Hz
    oscillator->noteOn(67, 0.9f); // G4 = 392.00 Hz

    // Generate samples
    std::vector<int16_t> samples;
    samples.reserve(total_samples);

    for (int i = 0; i < total_samples; i++)
    {
      float sample = oscillator->getSample();
      // Convert float sample to 16-bit integer
      int16_t sample_16bit = static_cast<int16_t>(sample * 32767.0f);
      samples.push_back(sample_16bit);
    }

    // Write to WAV file
    std::cout << "Writing to " << filenames[wave_idx] << "...\n";

    WAVConfig config(static_cast<uint32_t>(sample_rate), 1, 16);
    int result = WAVWriter::writeFile(filenames[wave_idx], samples, total_samples, config);

    if (result == 0)
    {
      std::cout << "✓ Successfully created " << filenames[wave_idx] << "\n";

      // Calculate file size
      int file_size = WAVWriter::calculateFileSize(total_samples, config);
      float duration = WAVWriter::calculateDuration(total_samples, static_cast<int>(sample_rate));

      std::cout << "  File size: " << file_size << " bytes\n";
      std::cout << "  Duration: " << duration << " seconds\n";
    }
    else
    {
      std::cout << "✗ Failed to create " << filenames[wave_idx] << " (error: " << result << ")\n";
    }

    // Generate stereo version with the same oscillator
    std::vector<int16_t> stereo_samples;
    stereo_samples.reserve(total_samples * 2); // Stereo has twice the samples

    // Reset oscillator for clean generation
    oscillator->reset();
    oscillator->noteOn(60, 1.0f);
    oscillator->noteOn(63, 0.8f);
    oscillator->noteOn(67, 0.9f);

    // Set panning for better stereo separation
    oscillator->setVoicePan(60, -0.7f); // C more to the left
    oscillator->setVoicePan(63, 0.0f);  // Eb center
    oscillator->setVoicePan(67, 0.7f);  // G more to the right

    for (int i = 0; i < total_samples; i++)
    {
      StereoSample stereo_sample = oscillator->getStereoSample();

      // Convert float samples to 16-bit integers
      int16_t left_16bit = static_cast<int16_t>(stereo_sample.left * 32767.0f);
      int16_t right_16bit = static_cast<int16_t>(stereo_sample.right * 32767.0f);

      // Interleave left and right channels (LRLRLR format)
      stereo_samples.push_back(left_16bit);
      stereo_samples.push_back(right_16bit);
    }

    // Write stereo WAV file
    std::string stereo_filename = std::string("stereo_") + filenames[wave_idx];
    std::cout << "Writing stereo version to " << stereo_filename << "...\n";

    WAVConfig stereo_config(static_cast<uint32_t>(sample_rate), 2, 16);
    result = WAVWriter::writeFile(stereo_filename.c_str(), stereo_samples, total_samples, stereo_config);

    if (result == 0)
    {
      std::cout << "✓ Successfully created " << stereo_filename << "\n";

      // Calculate file size for stereo
      int stereo_file_size = WAVWriter::calculateFileSize(total_samples, stereo_config);
      float stereo_duration = WAVWriter::calculateDuration(total_samples, static_cast<int>(sample_rate));

      std::cout << "  File size: " << stereo_file_size << " bytes\n";
      std::cout << "  Duration: " << stereo_duration << " seconds\n";
    }
    else
    {
      std::cout << "✗ Failed to create " << stereo_filename << " (error: " << result << ")\n";
    }

    std::cout << "\n";
  }

  std::cout << "WAV file generation completed!\n";
  std::cout << "Generated files:\n";
  std::cout << "  - sine_wave.wav (smooth sine wave)\n";
  std::cout << "  - sawtooth_wave.wav (linear ramp wave)\n";
  std::cout << "  - square_wave.wav (digital on/off wave)\n\n";

  std::cout << "You can play these files with any audio player.\n";
  return 0;
}
