// Create file: test_audio_simple.cpp
#include "../lib/audio/audio_sink.hpp"
#include "../app/common/audio_constants.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <cmath>

using namespace synth;
using namespace examples;

bool audio_working = false;

void simpleAudioCallback(float *buffer, int bufferSize, int channels)
{
    static float phase = 0.0f;
    static int counter = 0;

    if (++counter % 1000 == 0)
    {
        std::cout << "Audio callback active (channels: " << channels << ")\n";
    }

    if (channels == 2)
    {
        // Generate stereo sine wave with slight panning
        for (int i = 0; i < bufferSize; i++)
        {
            float sample = 0.5f * sinf(phase);
            buffer[i * 2] = sample * 0.8f;     // Left channel (slightly quieter)
            buffer[i * 2 + 1] = sample * 1.0f; // Right channel (full volume)
            phase += 2.0f * 3.14159f * 440.0f / SAMPLE_RATE;
            if (phase > 2.0f * 3.14159f)
                phase -= 2.0f * 3.14159f;
        }
    }
    else
    {
        // Generate mono sine wave
        for (int i = 0; i < bufferSize; i++)
        {
            buffer[i] = 0.5f * sinf(phase);
            phase += 2.0f * 3.14159f * 440.0f / SAMPLE_RATE;
            if (phase > 2.0f * 3.14159f)
                phase -= 2.0f * 3.14159f;
        }
    }

    audio_working = true;
}

int main()
{
    PlatformUtils::initialize();

    AudioSink sink;
    AudioSink::listDevices();

    std::cout << "\nTesting default audio device..." << std::endl;

#ifdef _WIN32
    sink.setBackend(AudioBackendType::DirectSound);
#elif defined(__linux__)
    sink.setBackend(AudioBackendType::ALSA);
#endif

    if (sink.init() != 0)
    {
        std::cout << "Failed to init audio\n";
        return -1;
    }

    if (sink.startWithCallback(simpleAudioCallback) != 0)
    {
        std::cout << "Failed to start audio\n";
        return -1;
    }

    std::cout << "Playing 440Hz tone for 5 seconds...\n";
    PlatformUtils::sleep(5000);

    sink.stop();

    if (audio_working)
    {
        std::cout << "Audio system working!\n";
    }
    else
    {
        std::cout << "Audio callback never called!\n";
    }

    return 0;
}
