#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>

using namespace synth;
using namespace examples;

// Global variables for polyphonic MIDI control
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

// MIDI note callback function for polyphonic playback
void polyMidiNoteCallback(const MidiNote &note)
{
    if (!g_poly_oscillator)
        return;

    if (note.is_note_on && note.velocity > 0)
    {
        // Note ON - start playing on available voice
        float velocity = static_cast<float>(note.velocity) / 127.0f;
        g_poly_oscillator->noteOn(note.note, velocity);

        std::cout << "♪ Polyphonic Note ON: " << static_cast<int>(note.note)
                  << " (" << PolyphonicOscillator::noteToFrequency(note.note) << " Hz)"
                  << " velocity: " << velocity
                  << " [Active voices: " << g_poly_oscillator->getActiveVoiceCount() << "]\n";
    }
    else
    {
        // Note OFF - stop playing this note
        g_poly_oscillator->noteOff(note.note);

        std::cout << "♪ Polyphonic Note OFF: " << static_cast<int>(note.note)
                  << " [Active voices: " << g_poly_oscillator->getActiveVoiceCount() << "]\n";
    }
}

int main()
{
    PlatformUtils::initialize();

    const char *controls[] = {
        "MIDI Notes - Play up to 8 simultaneous voices",
        "1 - Switch to Sine wave",
        "2 - Switch to Square wave",
        "3 - Switch to Sawtooth wave",
        "q - Quit",
        nullptr};

    PlatformUtils::printHeader("Polyphonic MIDI Synthesizer Test (8 voices)",
                               "Real-time polyphonic MIDI input with 8-voice capability");
    PlatformUtils::printControls(controls);

    // List available MIDI devices
    std::cout << "Scanning MIDI input devices...\n";
    int num_midi_devices = MidiInterface::getDeviceCount();

    if (num_midi_devices == 0)
    {
        std::cout << "No MIDI input devices found!\n";
        std::cout << "Please connect a MIDI device or use a virtual MIDI port.\n";
        PlatformUtils::waitForInput();
        PlatformUtils::cleanup();
        return -1;
    }

    // List available audio devices
    std::cout << "Scanning audio output devices...\n";
    AudioSink::listDevices();

    // Initialize audio sink
    AudioSink sink;
    if (sink.init() != 0)
    {
        std::cout << "Failed to initialize audio sink\n";
        PlatformUtils::cleanup();
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

    // Initialize MIDI reader
    MidiInterface midi_reader;
    if (midi_reader.init() != 0)
    {
        std::cout << "Failed to initialize MIDI reader\n";
        return -1;
    }

    // Set MIDI callback
    midi_reader.setNoteCallback(polyMidiNoteCallback);

    // Ask user which MIDI device to use
    int midi_device = 0;
    if (num_midi_devices > 1)
    {
        std::cout << "\nSelect MIDI input device (0-" << (num_midi_devices - 1) << "): ";
        std::cin >> midi_device;

        if (midi_device < 0 || midi_device >= num_midi_devices)
        {
            std::cout << "Invalid device selection, using device 0\n";
            midi_device = 0;
        }
    }

    // Start MIDI input
    if (midi_reader.open(midi_device) != 0)
    {
        std::cout << "Failed to start MIDI input\n";
        return -1;
    }

    std::cout << "\n🎹 Polyphonic MIDI Synthesizer Ready! (8 voices max)\n";
    std::cout << "Play chords and multiple notes on your MIDI device.\n";
    std::cout << "Current wave type: Sine\n";
    std::cout << "Controls:\n";
    std::cout << "  1, 2, 3 - Change wave type (Sine, Square, Sawtooth)\n";
    std::cout << "  a - All notes off\n";
    std::cout << "  s - Show active voice count\n";
    std::cout << "  q - Quit\n\n";

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
            case 'a':
            case 'A':
                g_poly_oscillator->allNotesOff();
                std::cout << "All notes stopped\n";
                break;
            case 's':
            case 'S':
                std::cout << "Active voices: " << g_poly_oscillator->getActiveVoiceCount() << "/8\n";
                break;
            case 'q':
            case 'Q':
                g_running.store(false);
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

    PlatformUtils::cleanup();
    std::cout << "Polyphonic MIDI Synthesizer stopped.\n";

    return 0;
}
