#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <cstring>

using namespace synth;

int main()
{
    std::cout << "A4 Pitch Test (440 Hz)\n";
    std::cout << "======================\n\n";

    // List available audio devices
    std::cout << "Scanning audio output devices...\n";
    AudioSink::listDevices();

    // Initialize audio sink
    AudioSink sink;
    if (sink.init() != 0)
    {
        std::cout << "Failed to initialize audio sink\n";
        return -1;
    }

    // Start audio output
    if (sink.start() != 0)
    {
        std::cout << "Failed to start audio output\n";
        return -1;
    }

    std::cout << "\n🎵 Playing A4 (440 Hz) in different wave forms:\n\n";

    // Test each wave type for 2 seconds each
    WaveType wave_types[] = {WaveType::SINE, WaveType::SAW, WaveType::SQUARE};
    const char *wave_names[] = {"Sine", "Sawtooth", "Square"};

    for (int i = 0; i < 3; i++)
    {
        std::cout << "Playing " << wave_names[i] << " wave for 2 seconds...\n";

        // Create oscillator for this wave type
        auto oscillator = std::make_unique<Oscillator>(440.0f, wave_types[i], static_cast<float>(SAMPLE_RATE));
        oscillator->setAmplitude(0.1f); // 10% volume

        // Set up audio callback to generate continuous audio
        sink.setAudioCallback([&oscillator](float *buffer, int buffer_size, int channels)
                              {
                                  if (oscillator && channels == 2)
                                  {
                                      oscillator->generateStereoBuffer(buffer, buffer_size);
                                  }
                                  else
                                  {
                                        // Fill with silence if no oscillator
                                        int totalSamples = (channels == 2) ? buffer_size * 2 : buffer_size;
                                        std::fill(buffer, buffer + totalSamples, 0.0f);
                                  } });

        // Play for 2 seconds
        auto start_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::seconds(2);

        // Just wait while the audio callback generates audio
        while (std::chrono::steady_clock::now() - start_time < duration)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::cout << "✓ " << wave_names[i] << " wave completed\n";

        // Brief pause between wave types
        if (i < 2)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // Stop audio
    sink.stop();

    std::cout << "\n🎵 A4 Pitch Test completed successfully!\n";
    std::cout << "All three wave types played at 440 Hz.\n";

    return 0;
}
