#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/polyphonic_adsr_oscillator.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <thread>

using namespace synth;
using namespace examples;

// Global variables for polyphonic ADSR control
static std::unique_ptr<PolyphonicADSROscillator> g_adsr_oscillator = nullptr;
static std::atomic<bool> g_running{true};

// Audio generation callback - this runs in the audio thread
void adsrAudioCallback(float *buffer, int bufferSize, int channels)
{
    if (g_adsr_oscillator && g_running.load())
    {
        // Generate polyphonic ADSR audio samples
        g_adsr_oscillator->generateBuffer(buffer, bufferSize);
    }
    else
    {
        // Fill with silence if no oscillator or not running
        std::fill(buffer, buffer + bufferSize, 0.0f);
    }
}

// Demo function to play a chord with ADSR envelope
void playADSRDemo()
{
    if (!g_adsr_oscillator)
        return;

    std::cout << "\n=== ADSR Demo Sequence ===\n";

    // Demo 1: Fast attack, medium decay, high sustain, medium release
    std::cout << "1. Fast attack, medium decay, high sustain, medium release\n";
    g_adsr_oscillator->setADSR(0.001f, 0.2f, 0.8f, 0.4f);

    // Play C major chord
    g_adsr_oscillator->noteOn(60, 0.8f); // C4
    g_adsr_oscillator->noteOn(64, 0.8f); // E4
    g_adsr_oscillator->noteOn(67, 0.8f); // G4

    PlatformUtils::sleep(2000);
    g_adsr_oscillator->allNotesOff();
    PlatformUtils::sleep(1000);

    // Demo 2: Slow attack, fast decay, low sustain, long release
    std::cout << "2. Slow attack, fast decay, low sustain, long release\n";
    g_adsr_oscillator->setADSR(0.5f, 0.05f, 0.3f, 1.0f);

    // Play F major chord
    g_adsr_oscillator->noteOn(65, 0.8f); // F4
    g_adsr_oscillator->noteOn(69, 0.8f); // A4
    g_adsr_oscillator->noteOn(72, 0.8f); // C5

    PlatformUtils::sleep(3000);
    g_adsr_oscillator->allNotesOff();
    PlatformUtils::sleep(1500);

    // Demo 3: Organ-like (no attack, no decay, full sustain, quick release)
    std::cout << "3. Organ-like (instant attack, no decay, full sustain, quick release)\n";
    g_adsr_oscillator->setADSR(0.0f, 0.0f, 1.0f, 0.1f);

    // Play G major chord
    g_adsr_oscillator->noteOn(67, 0.8f); // G4
    g_adsr_oscillator->noteOn(71, 0.8f); // B4
    g_adsr_oscillator->noteOn(74, 0.8f); // D5

    PlatformUtils::sleep(2000);
    g_adsr_oscillator->allNotesOff();
    PlatformUtils::sleep(500);

    // Demo 4: Percussion-like (fast attack, fast decay, no sustain, short release)
    std::cout << "4. Percussion-like (fast attack, fast decay, no sustain, short release)\n";
    g_adsr_oscillator->setADSR(0.001f, 0.3f, 0.0f, 0.1f);

    // Play individual notes like percussion hits
    for (int note = 60; note <= 67; note += 2)
    {
        g_adsr_oscillator->noteOn(static_cast<uint8_t>(note), 0.9f);
        PlatformUtils::sleep(400);
        g_adsr_oscillator->noteOff(static_cast<uint8_t>(note));
        PlatformUtils::sleep(100);
    }

    std::cout << "=== ADSR Demo Complete ===\n\n";

    // Reset to default ADSR
    g_adsr_oscillator->setADSR(0.01f, 0.1f, 0.7f, 0.3f);
}

int main()
{
    std::cout << "Polyphonic ADSR Synthesizer Test\n";
    std::cout << "================================\n\n";

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

    // Initialize polyphonic ADSR oscillator - create with sine waves
    g_adsr_oscillator = std::make_unique<PolyphonicADSROscillator>(WaveType::SINE, static_cast<float>(SAMPLE_RATE));

    // Start audio output with callback
    if (sink.startWithCallback(adsrAudioCallback) != 0)
    {
        std::cout << "Failed to start audio output\n";
        return -1;
    }

    std::cout << "\n🎵 Polyphonic ADSR Synthesizer Ready! (8 voices with envelopes)\n";
    std::cout << "Current wave type: Sine\n";
    std::cout << "Current ADSR: A=0.01s, D=0.1s, S=0.7, R=0.3s\n";
    std::cout << "Controls:\n";
    std::cout << "  1, 2, 3 - Change wave type (Sine, Square, Sawtooth)\n";
    std::cout << "  d - Play ADSR demonstration sequence\n";
    std::cout << "  c - Play chord (C major)\n";
    std::cout << "  a - All notes off\n";
    std::cout << "  s - Show voice status\n";
    std::cout << "  e - Toggle envelope on/off\n";
    std::cout << "  ADSR parameter controls:\n";
    std::cout << "    z/x - Decrease/Increase Attack (0.001-1.0s)\n";
    std::cout << "    v/b - Decrease/Increase Decay (0.001-1.0s)\n";
    std::cout << "    n/m - Decrease/Increase Sustain (0.0-1.0)\n";
    std::cout << "    ,/. - Decrease/Increase Release (0.001-2.0s)\n";
    std::cout << "  4-9 - Play individual notes (C4-F#4)\n";
    std::cout << "  q - Quit\n\n";

    // Play initial demo
    std::cout << "Playing initial ADSR demo in 2 seconds...\n";
    PlatformUtils::sleep(2000);
    playADSRDemo();

    // Current ADSR values for interactive control
    float current_attack = 0.01f;
    float current_decay = 0.1f;
    float current_sustain = 0.7f;
    float current_release = 0.3f;

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
                g_adsr_oscillator->setWaveType(WaveType::SINE);
                std::cout << "Wave type changed to: Sine\n";
                break;
            case '2':
                g_adsr_oscillator->setWaveType(WaveType::SQUARE);
                std::cout << "Wave type changed to: Square\n";
                break;
            case '3':
                g_adsr_oscillator->setWaveType(WaveType::SAW);
                std::cout << "Wave type changed to: Sawtooth\n";
                break;
            case 'd':
            case 'D':
                std::cout << "Playing ADSR demonstration...\n";
                playADSRDemo();
                break;
            case 'c':
            case 'C':
                std::cout << "Playing C major chord...\n";
                g_adsr_oscillator->noteOn(60, 0.8f); // C4
                g_adsr_oscillator->noteOn(64, 0.8f); // E4
                g_adsr_oscillator->noteOn(67, 0.8f); // G4
                break;
            case 'a':
            case 'A':
                g_adsr_oscillator->allNotesOff();
                std::cout << "All notes stopped\n";
                break;
            case 's':
            case 'S':
            {
                int active = g_adsr_oscillator->getActiveVoiceCount();
                int releasing = g_adsr_oscillator->getReleasingVoiceCount();
                std::cout << "Active voices: " << active << "/8, Releasing: " << releasing << "\n";
                std::cout << "Current ADSR: A=" << current_attack << "s, D=" << current_decay
                          << "s, S=" << current_sustain << ", R=" << current_release << "s\n";
            }
            break;
            case 'e':
            case 'E':
            {
                static bool envelopes_enabled = true;
                envelopes_enabled = !envelopes_enabled;
                g_adsr_oscillator->enableEnvelopes(envelopes_enabled);
                std::cout << "Envelopes " << (envelopes_enabled ? "enabled" : "disabled") << "\n";
            }
            break;

            // ADSR parameter controls
            case 'z':
                current_attack = std::max(0.001f, current_attack - 0.01f);
                g_adsr_oscillator->setAttackTime(current_attack);
                std::cout << "Attack: " << current_attack << "s\n";
                break;
            case 'x':
                current_attack = std::min(1.0f, current_attack + 0.01f);
                g_adsr_oscillator->setAttackTime(current_attack);
                std::cout << "Attack: " << current_attack << "s\n";
                break;
            case 'v':
                current_decay = std::max(0.001f, current_decay - 0.02f);
                g_adsr_oscillator->setDecayTime(current_decay);
                std::cout << "Decay: " << current_decay << "s\n";
                break;
            case 'b':
                current_decay = std::min(1.0f, current_decay + 0.02f);
                g_adsr_oscillator->setDecayTime(current_decay);
                std::cout << "Decay: " << current_decay << "s\n";
                break;
            case 'n':
                current_sustain = std::max(0.0f, current_sustain - 0.1f);
                g_adsr_oscillator->setSustainLevel(current_sustain);
                std::cout << "Sustain: " << current_sustain << "\n";
                break;
            case 'm':
                current_sustain = std::min(1.0f, current_sustain + 0.1f);
                g_adsr_oscillator->setSustainLevel(current_sustain);
                std::cout << "Sustain: " << current_sustain << "\n";
                break;
            case ',':
                current_release = std::max(0.001f, current_release - 0.05f);
                g_adsr_oscillator->setReleaseTime(current_release);
                std::cout << "Release: " << current_release << "s\n";
                break;
            case '.':
                current_release = std::min(2.0f, current_release + 0.05f);
                g_adsr_oscillator->setReleaseTime(current_release);
                std::cout << "Release: " << current_release << "s\n";
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
                              << PolyphonicADSROscillator::noteToFrequency(static_cast<uint8_t>(note)) << " Hz)\n";
                    g_adsr_oscillator->noteOn(static_cast<uint8_t>(note), 0.7f);

                    // Auto-release after 1.5 seconds
                    std::thread([note]()
                                {
                        PlatformUtils::sleep(1500);
                        g_adsr_oscillator->noteOff(static_cast<uint8_t>(note)); })
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
    if (g_adsr_oscillator)
    {
        g_adsr_oscillator->allNotesOff();
    }

    // Stop audio (this will join the audio thread)
    sink.stop();

    // Cleanup (RAII handles most cleanup automatically)
    g_adsr_oscillator.reset();

    std::cout << "Polyphonic ADSR Synthesizer stopped.\n";

    return 0;
}
