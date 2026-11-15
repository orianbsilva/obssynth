#include "../lib/oscillators/oscillator.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace synth;
using namespace examples;

std::unique_ptr<Oscillator> g_oscillator = nullptr;

void stereoAudioCallback(float *buffer, int bufferSize, int channels)
{
    if (g_oscillator && channels == 2)
    {
        // Generate stereo audio directly
        g_oscillator->generateStereoBuffer(buffer, bufferSize);
    }
    else
    {
        // Fill with silence
        int totalSamples = (channels == 2) ? bufferSize * 2 : bufferSize;
        std::fill(buffer, buffer + totalSamples, 0.0f);
    }
}

int main()
{
    PlatformUtils::initialize();
    std::cout << "=== Stereo Panning Demonstration ===\n";

    // Initialize polyphonic oscillator
    g_oscillator = std::make_unique<Oscillator>(440.0f, WaveType::SINE, 48000.0f);
    g_oscillator->setAmplitude(0.5f); // Moderate volume

    // Initialize audio
    AudioSink sink;
#ifdef _WIN32
    sink.setBackend(AudioBackendType::DirectSound);
    if (sink.init() != 0)
    {
        std::cout << "DirectSound failed, trying WASAPI...\n";
        sink.setBackend(AudioBackendType::WASAPI);
        if (sink.init() != 0)
        {
            std::cout << "WASAPI failed, trying WaveOut...\n";
            sink.setBackend(AudioBackendType::WaveOut);
            if (sink.init() != 0)
            {
                std::cout << "Failed to initialize audio\n";
                return -1;
            }
        }
    }
#elif defined(__linux__)
    sink.setBackend(AudioBackendType::ALSA);
    if (sink.init() != 0)
    {
        std::cout << "Failed to initialize ALSA audio\n";
        return -1;
    }
#endif

    if (sink.startWithCallback(stereoAudioCallback) != 0)
    {
        std::cout << "Failed to start audio\n";
        return -1;
    }

    std::cout << "Playing stereo panning demonstration...\n";
    std::cout << "Listen for notes panning from left to right!\n\n";

    // Demo sequence with panning
    struct NoteDemo
    {
        uint8_t note;
        float pan;
        const char *desc;
    };

    NoteDemo notes[] = {
        {60, -1.0f, "Full Left"}, // C4 - Full Left
        {64, -0.5f, "Left"},      // E4 - Left
        {67, 0.0f, "Center"},     // G4 - Center
        {72, 0.5f, "Right"},      // C5 - Right
        {76, 1.0f, "Full Right"}  // E5 - Full Right
    };

    for (int cycle = 0; cycle < 3; cycle++)
    {
        std::cout << "Cycle " << (cycle + 1) << "/3\n";

        for (int i = 0; i < 5; i++)
        {
            uint8_t noteNum = notes[i].note;
            float pan = notes[i].pan;
            const char *panDesc = notes[i].desc;

            std::cout << "  Playing note " << (int)noteNum << " (" << panDesc << ")\n";

            // Start note with specific pan position
            g_oscillator->setPan(pan);
            g_oscillator->setFrequency(440.0f * powf(2.0f, (noteNum - 69) / 12.0f));
            g_oscillator->setAmplitude(0.5f);
            std::this_thread::sleep_for(std::chrono::milliseconds(800));

            // Stop note
            g_oscillator->setAmplitude(0.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        std::cout << "\n";
    }

    sink.stop();
    std::cout << "Stereo demonstration complete!\n";

    return 0;
}
