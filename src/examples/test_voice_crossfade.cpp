/**
 * @file test_voice_crossfade.cpp
 * @brief Test crossfade voice stealing to validate smooth transitions
 */

#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace synth;

// Audio callback generator for crossfade testing
class CrossfadeTestGenerator
{
private:
    PolyphonicOscillator &oscillator_;

public:
    CrossfadeTestGenerator(PolyphonicOscillator &osc) : oscillator_(osc) {}

    void audioCallback(float *buffer, int frameCount, int channelCount)
    {
        if (channelCount == 2)
        {
            oscillator_.generateStereoBufferOptimized(buffer, frameCount);
        }
        else
        {
            oscillator_.generateBufferOptimized(buffer, frameCount);
        }
    }
};

int main()
{
    std::cout << "Voice Crossfade Test\n";
    std::cout << "====================\n";

    try
    {
        // Initialize polyphonic oscillator
        PolyphonicOscillator oscillator(WaveType::SINE, 48000.0f);

        // Initialize audio generator
        CrossfadeTestGenerator generator(oscillator);

        // Initialize audio sink
        AudioSink audioSink;
#ifdef _WIN32
        audioSink.setBackend(AudioBackendType::DirectSound);
#elif defined(__linux__)
        audioSink.setBackend(AudioBackendType::ALSA);
#endif

        // Set callback using lambda that captures generator
        audioSink.setAudioCallback([&generator](float *buffer, int frames, int channels)
                                   { generator.audioCallback(buffer, frames, channels); });

#ifdef _WIN32
        std::cout << "Initializing DirectSound backend...\n";
        if (audioSink.init() != 0)
        {
            std::cout << "Failed to initialize DirectSound audio sink\n";
            return 1;
        }
#elif defined(__linux__)
        std::cout << "Initializing ALSA backend...\n";
        if (audioSink.init() != 0)
        {
            std::cout << "Failed to initialize ALSA audio sink\n";
            return 1;
        }
#endif

        if (audioSink.start() != 0)
        {
#ifdef _WIN32
            std::cout << "Failed to start DirectSound audio playback\n";
#elif defined(__linux__)
            std::cout << "Failed to start ALSA audio playback\n";
#endif
            return 1;
        }

        std::cout << "Testing voice stealing with crossfade...\n";
        std::cout << "You should hear smooth transitions without clicks\n\n";

        // Test 1: Fill all 8 voices
        std::cout << "Phase 1: Filling all 8 voices...\n";
        for (int i = 0; i < 8; ++i)
        {
            uint8_t note = static_cast<uint8_t>(60 + i); // C4 to G4
            float velocity = 0.6f;
            oscillator.noteOn(note, velocity);
            std::cout << "Added voice " << i << " (note " << (int)note << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        std::cout << "Active voices: " << oscillator.getActiveVoiceCount() << "/8\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Test 2: Trigger voice stealing with crossfade
        std::cout << "\nPhase 2: Triggering voice stealing (should crossfade smoothly)...\n";
        for (int i = 0; i < 5; ++i)
        {
            uint8_t note = static_cast<uint8_t>(72 + i); // C5 to E5 - these will steal voices
            float velocity = 0.7f;
            oscillator.noteOn(note, velocity);
            std::cout << "Stealing voice for note " << (int)note << " (should crossfade)\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Test 3: Rapid note changes (stress test crossfade)
        std::cout << "\nPhase 3: Rapid note changes (crossfade stress test)...\n";
        for (int i = 0; i < 20; ++i)
        {
            uint8_t note = static_cast<uint8_t>(48 + (i % 24)); // 2 octave range
            float velocity = 0.5f + (i % 3) * 0.15f;            // Varying velocity
            oscillator.noteOn(note, velocity);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Test 4: All notes off with crossfade
        std::cout << "\nPhase 4: All notes off (should crossfade to silence)...\n";
        oscillator.allNotesOff();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        audioSink.stop();

        std::cout << "\nCrossfade test completed!\n";
        std::cout << "Listen for:\n";
        std::cout << "- Smooth voice stealing without clicks\n";
        std::cout << "- Gradual fade-in of new notes\n";
        std::cout << "- Gradual fade-out of released notes\n";
        std::cout << "- No abrupt amplitude drops during voice stealing\n";
    }
    catch (const std::exception &e)
    {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
