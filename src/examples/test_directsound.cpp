/**
 * @file test_directsound.cpp
 * @brief Test program for DirectSound audio backend
 *
 * This program demonstrates the DirectSound audio backend by playing
 * a simple sine wave tone. It can be used to verify that the DirectSound
 * implementation is working correctly.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include "audio/audio_sink.hpp"
#include "oscillators/oscillator.hpp"

using namespace synth;

// Simple audio callback that generates a sine wave
class TestAudioGenerator
{
public:
    TestAudioGenerator() : oscillator_(440.0f, WaveType::SINE, 48000.0f) {}

    void audioCallback(float *buffer, int frameCount, int channelCount)
    {
        for (int frame = 0; frame < frameCount; ++frame)
        {
            float sample = oscillator_.getSample() * 0.3f; // Reduce volume to 30%

            // Fill both channels with the same sample (mono to stereo)
            for (int channel = 0; channel < channelCount; ++channel)
            {
                buffer[frame * channelCount + channel] = sample;
            }
        }
    }

private:
    Oscillator oscillator_;
};

int main()
{
    std::cout << "DirectSound Audio Backend Test\n";
    std::cout << "==============================\n\n";

    // Create audio sink and generator
    AudioSink audioSink;
    TestAudioGenerator generator;

    // Set DirectSound backend
    audioSink.setBackend(AudioBackendType::DirectSound);

    // Set up audio callback
    audioSink.setAudioCallback([&generator](float *buffer, int frameCount, int channelCount)
                               { generator.audioCallback(buffer, frameCount, channelCount); });

    // Initialize and start audio
    std::cout << "Initializing DirectSound backend...\n";
    if (audioSink.init() != 0)
    {
        std::cerr << "Failed to initialize DirectSound audio sink\n";
        return -1;
    }

    std::cout << "Starting audio playback...\n";
    if (audioSink.start() != 0)
    {
        std::cerr << "Failed to start DirectSound audio playback\n";
        return -1;
    }

    std::cout << "Playing 440Hz sine wave for 5 seconds using DirectSound...\n";
    std::cout << "You should hear a pure tone.\n\n";

    // Play for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    std::cout << "Stopping audio playback...\n";
    audioSink.stop();

    std::cout << "DirectSound test completed successfully!\n";

    return 0;
}
