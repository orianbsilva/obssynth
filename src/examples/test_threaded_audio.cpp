#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <string>
#include <algorithm>

using namespace synth;

// Global variables for audio generation
static std::unique_ptr<Oscillator> g_oscillator = nullptr;
static std::atomic<bool> g_running{true};

// Audio generation callback - this runs in the audio thread
void audioCallback(float *buffer, int bufferSize, int /*numChannels*/)
{
    if (g_oscillator && g_running.load())
    {
        // Generate audio samples
        g_oscillator->generateBuffer(buffer, bufferSize);
    }
    else
    {
        // Fill with silence if no oscillator or not running
        std::fill(buffer, buffer + bufferSize, 0.0f);
    }
}

int main()
{
    std::cout << "Threaded Audio Sink Test\n";
    std::cout << "========================\n\n";

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

    // Initialize oscillator - create a sine oscillator
    g_oscillator = std::make_unique<Oscillator>(440.0f, WaveType::SINE, static_cast<float>(SAMPLE_RATE));
    g_oscillator->setAmplitude(0.3f); // Set a reasonable volume

    std::cout << "\n🎵 Starting threaded audio playback...\n";
    std::cout << "You should hear a 440Hz sine wave.\n";
    std::cout << "Press Enter to change frequency, 'q' + Enter to quit.\n\n";

    // Start audio output with callback
    if (sink.startWithCallback(audioCallback) != 0)
    {
        std::cout << "Failed to start audio output\n";
        return -1;
    }

    // Interactive loop
    std::string input;
    float frequency = 440.0f;

    while (g_running.load())
    {
        std::cout << "Current frequency: " << frequency << " Hz\n";
        std::cout << "Commands: [frequency] to set frequency, 'q' to quit: ";

        std::getline(std::cin, input);

        if (input.empty())
        {
            // Cycle through some common frequencies
            static float frequencies[] = {440.0f, 523.25f, 659.25f, 783.99f}; // A4, C5, E5, G5
            static int freq_index = 0;
            freq_index = (freq_index + 1) % 4;
            frequency = frequencies[freq_index];
            g_oscillator->setFrequency(frequency);
        }
        else if (input == "q" || input == "quit")
        {
            break;
        }
        else
        {
            try
            {
                float new_freq = std::stof(input);
                if (new_freq > 20.0f && new_freq < 20000.0f) // Reasonable audio range
                {
                    frequency = new_freq;
                    g_oscillator->setFrequency(frequency);
                    std::cout << "Frequency set to " << frequency << " Hz\n";
                }
                else
                {
                    std::cout << "Frequency out of range (20-20000 Hz)\n";
                }
            }
            catch (const std::exception &)
            {
                std::cout << "Invalid frequency. Please enter a number or 'q' to quit.\n";
            }
        }
    }

    std::cout << "\nShutting down...\n";
    g_running.store(false);

    // Stop audio (this will join the audio thread)
    sink.stop();

    // Cleanup
    g_oscillator.reset();

    std::cout << "Threaded audio test completed.\n";
    return 0;
}
