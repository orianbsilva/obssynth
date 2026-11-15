#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <thread>

using namespace synth;
using namespace examples;

// Global variables for polyphonic control
static std::unique_ptr<PolyphonicOscillator> g_poly_oscillator = nullptr;
static std::atomic<bool> g_running{true};

// Audio generation callback - this runs in the audio thread
void polyAudioCallback(float *buffer, int bufferSize, int /*numChannels*/)
{
    if (g_poly_oscillator && g_running.load())
    {
        // Generate polyphonic audio samples
        g_poly_oscillator->generateBuffer(buffer, bufferSize);
    }
    else
    {
        // Fill with silence if no oscillator or not running
        std::fill(buffer, buffer + bufferSize, 0.0f);
    }
}

// Demo function to play a chord progression
void playDemoChord()
{
    if (!g_poly_oscillator)
        return;

    // Play a C major chord (C4, E4, G4)
    std::cout << "Playing C major chord (C4, E4, G4)...\n";
    g_poly_oscillator->noteOn(60, 0.8f); // C4
    g_poly_oscillator->noteOn(64, 0.8f); // E4
    g_poly_oscillator->noteOn(67, 0.8f); // G4

    PlatformUtils::sleep(2000);

    // Release the chord
    g_poly_oscillator->noteOff(60);
    g_poly_oscillator->noteOff(64);
    g_poly_oscillator->noteOff(67);

    PlatformUtils::sleep(500);

    // Play an F major chord (F4, A4, C5)
    std::cout << "Playing F major chord (F4, A4, C5)...\n";
    g_poly_oscillator->noteOn(65, 0.8f); // F4
    g_poly_oscillator->noteOn(69, 0.8f); // A4
    g_poly_oscillator->noteOn(72, 0.8f); // C5

    PlatformUtils::sleep(2000);

    // Release the chord
    g_poly_oscillator->noteOff(65);
    g_poly_oscillator->noteOff(69);
    g_poly_oscillator->noteOff(72);

    PlatformUtils::sleep(500);

    // Play a G major chord (G4, B4, D5)
    std::cout << "Playing G major chord (G4, B4, D5)...\n";
    g_poly_oscillator->noteOn(67, 0.8f); // G4
    g_poly_oscillator->noteOn(71, 0.8f); // B4
    g_poly_oscillator->noteOn(74, 0.8f); // D5

    PlatformUtils::sleep(2000);

    // Release the chord
    g_poly_oscillator->allNotesOff();

    std::cout << "Demo chord progression complete!\n";
}

int main()
{
    std::cout << "Polyphonic Oscillator Test (8 voices)\n";
    std::cout << "====================================\n\n";

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

    // Initialize polyphonic oscillator - create with sine waves
    g_poly_oscillator = std::make_unique<PolyphonicOscillator>(WaveType::SINE, static_cast<float>(SAMPLE_RATE));

    // Start audio output with callback
    if (sink.startWithCallback(polyAudioCallback) != 0)
    {
        std::cout << "Failed to start audio output\n";
        return -1;
    }

    std::cout << "\n🎵 Polyphonic Oscillator Ready! (8 voices max)\n";
    std::cout << "Current wave type: Sine\n";
    std::cout << "Controls:\n";
    std::cout << "  1, 2, 3 - Change wave type (Sine, Square, Sawtooth)\n";
    std::cout << "  c - Play demo chord progression\n";
    std::cout << "  a - All notes off\n";
    std::cout << "  s - Show active voice count\n";
    std::cout << "  t - Test individual notes (C4-B4)\n";
    std::cout << "  q - Quit\n\n";

    // Play initial demo
    std::cout << "Playing initial demo in 2 seconds...\n";
    PlatformUtils::sleep(2000);
    playDemoChord();

    // Main event loop
    while (g_running.load())
    {
        // Check for keyboard input (non-blocking)
        if (PlatformUtils::kbhit())
        {
            int key = PlatformUtils::getch();

            switch (key)
            {
            case '1':
                g_poly_oscillator->setWaveType(WaveType::SINE);
                std::cout << "Wave type changed to: Sine\n";
                break;
            case '2':
                g_poly_oscillator->setWaveType(WaveType::SQUARE);
                std::cout << "Wave type changed to: Square\n";
                break;
            case '3':
                g_poly_oscillator->setWaveType(WaveType::SAW);
                std::cout << "Wave type changed to: Sawtooth\n";
                break;
            case 'c':
            case 'C':
                std::cout << "Playing demo chord progression...\n";
                playDemoChord();
                break;
            case 'a':
            case 'A':
                g_poly_oscillator->allNotesOff();
                std::cout << "All notes stopped\n";
                break;
            case 's':
            case 'S':
                std::cout << "Active voices: " << g_poly_oscillator->getActiveVoiceCount() << "/8\n";
                break;
            case 't':
            case 'T':
            {
                std::cout << "Testing individual notes (C4-B4)...\n";
                for (int note = 60; note < 72; ++note) // C4 to B4
                {
                    std::cout << "Playing note " << note << " ("
                              << PolyphonicOscillator::noteToFrequency(static_cast<uint8_t>(note)) << " Hz)\n";
                    g_poly_oscillator->noteOn(static_cast<uint8_t>(note), 0.6f);
                    PlatformUtils::sleep(500);
                    g_poly_oscillator->noteOff(static_cast<uint8_t>(note));
                    PlatformUtils::sleep(100);
                }
                std::cout << "Note test complete!\n";
            }
            break;
            case 'q':
            case 'Q':
                g_running.store(false);
                break;
            default:
                // Check if it's a number key for playing specific notes
                if (key >= '4' && key <= '9')
                {
                    int note_offset = key - '4';
                    int note = 60 + note_offset; // C4 + offset
                    std::cout << "Playing note " << note << " ("
                              << PolyphonicOscillator::noteToFrequency(static_cast<uint8_t>(note)) << " Hz)\n";
                    g_poly_oscillator->noteOn(static_cast<uint8_t>(note), 0.7f);

                    // Auto-release after 1 second
                    std::thread([note]()
                                {
                        PlatformUtils::sleep(1000);
                        g_poly_oscillator->noteOff(static_cast<uint8_t>(note)); })
                        .detach();
                }
                break;
            }
        }

        // Small delay to prevent excessive CPU usage
        PlatformUtils::sleep(10);
    }

    std::cout << "\nShutting down...\n";
    g_running.store(false);

    // Stop all notes before shutting down
    if (g_poly_oscillator)
    {
        g_poly_oscillator->allNotesOff();
    }

    // Stop audio (this will join the audio thread)
    sink.stop();

    // Cleanup (RAII handles most cleanup automatically)
    g_poly_oscillator.reset();

    std::cout << "Polyphonic Oscillator Test stopped.\n";

    return 0;
}
