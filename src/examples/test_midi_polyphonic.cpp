#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include "../lib/oscillators/lfo.hpp"
#include "../lib/oscillators/polyphonic_adsr_oscillator.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "../lib/utils/wav_writer.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <vector>
#include <mutex>
// #include "../lib/utils/logger.hpp"

using namespace synth;
using namespace examples;

// Global variables for MIDI control
static std::unique_ptr<PolyphonicADSROscillator> g_oscillator = nullptr;
static std::unique_ptr<LFO> g_lfo = nullptr;
static std::atomic<bool> g_note_playing{false};
static std::atomic<bool> g_running{true};
static uint8_t g_current_note = 0; // Track the currently playing note for polyphonic aftertouch

// Recording variables
static std::atomic<bool> g_recording{false};
static std::vector<float> g_recording_buffer;
static std::mutex g_recording_mutex;
static std::string g_recording_filename = "midi_polyphonic_session.wav";

// static std::unique_ptr<Logger> g_logger = nullptr;

// Audio generation callback - this runs in the audio thread
void audioCallback(float *buffer, int bufferSize, int /*numChannels*/)
{
    if (g_oscillator && g_running.load())
    {
        // Generate audio samples - PolyphonicADSROscillator handles multiple voices
        g_oscillator->generateBuffer(buffer, bufferSize);

        // Apply LFO modulation if LFO is active
        // Note: PolyphonicADSROscillator doesn't expose individual oscillators for modulation
        // LFO modulation would need to be implemented differently for polyphonic operation
        // For now, we'll skip LFO modulation with polyphonic oscillator

        // Record audio if recording is enabled
        if (g_recording.load())
        {
            std::lock_guard<std::mutex> lock(g_recording_mutex);
            g_recording_buffer.insert(g_recording_buffer.end(), buffer, buffer + bufferSize);
        }
    }
    else
    {
        // Fill with silence if no oscillator or not running
        std::fill(buffer, buffer + bufferSize, 0.0f);
    }
}

// Start recording audio to WAV file
void startRecording()
{
    if (g_recording.load())
    {
        std::cout << "Already recording!\n";
        return;
    }

    std::lock_guard<std::mutex> lock(g_recording_mutex);
    g_recording_buffer.clear();
    g_recording.store(true);

    std::cout << "🔴 Recording started... (will save to " << g_recording_filename << ")\n";
}

// Stop recording and save WAV file
void stopRecording()
{
    if (!g_recording.load())
    {
        std::cout << "Not currently recording!\n";
        return;
    }

    g_recording.store(false);

    std::lock_guard<std::mutex> lock(g_recording_mutex);

    if (g_recording_buffer.empty())
    {
        std::cout << "No audio data recorded.\n";
        return;
    }

    // Create WAV configuration (mono, 16-bit, 44.1kHz)
    WAVConfig config = WAVWriter::createConfig(SAMPLE_RATE, WAV_CHANNELS_MONO, WAV_BITS_PER_SAMPLE_16);

    // Write WAV file
    int result = WAVWriter::writeFile(g_recording_filename, g_recording_buffer,
                                      static_cast<int>(g_recording_buffer.size()), config);

    if (result == 0)
    {
        float duration = WAVWriter::calculateDuration(static_cast<int>(g_recording_buffer.size()), SAMPLE_RATE);
        int file_size = WAVWriter::calculateFileSize(static_cast<int>(g_recording_buffer.size()), config);

        std::cout << "⏹️  Recording stopped and saved!\n";
        std::cout << "   File: " << g_recording_filename << "\n";
        std::cout << "   Duration: " << duration << " seconds\n";
        std::cout << "   Samples: " << g_recording_buffer.size() << "\n";
        std::cout << "   File size: " << file_size << " bytes\n";
    }
    else
    {
        std::cout << "❌ Failed to save recording to " << g_recording_filename << "\n";
    }

    g_recording_buffer.clear();
}

// MIDI note callback function
void midiNoteCallback(const MidiNote &note)
{
    if (!g_oscillator)
        return;

    if (note.is_note_on && note.velocity > 0)
    {
        // Note ON - start playing
        g_current_note = note.note; // Track current note for polyphonic aftertouch

        g_note_playing.store(true);

        // Set velocity (0-127 -> 0.0f-1.0f)
        float velocity = static_cast<float>(note.velocity) / 127.0f;

        // Use PolyphonicADSROscillator's noteOn method which takes note number and velocity
        g_oscillator->noteOn(note.note, velocity);

        float frequency = PolyphonicADSROscillator::noteToFrequency(note.note);
        // std::cout << "♪ Playing MIDI note " << static_cast<int>(note.note)
        //<< " (" << frequency << " Hz) at " << (velocity * 100.0f) << "% volume\n";
    }
    else
    {
        // Note OFF - stop playing
        g_note_playing.store(false);
        g_oscillator->noteOff(note.note); // Pass the specific note number to turn off
        // std::cout << "♪ Note OFF: " << static_cast<int>(note.note) << "\n";
    }
}

void midiControlCallBack(uint8_t channel, uint8_t controller, uint8_t value)
{
    // Suppress unused parameter warning
    (void)channel;

    if (controller == 21) // First Knob on Keyboard
    {
        float attack = MidiInterface::mapControlValue(value, 0.01f, 1.0f); // Map MIDI value to attack time
        g_oscillator->setAttackTime(attack);
        std::cout << "CC21: Attack time set to " << attack << "s\n";
    }
    if (controller == 22) // Second Knob on Keyboard
    {
        float decay = MidiInterface::mapControlValue(value, 0.01f, 5.0f); // Map MIDI value to decay time
        g_oscillator->setDecayTime(decay);
        std::cout << "CC22: Decay time set to " << decay << "s\n";
    }
    if (controller == 23) // Third Knob on Keyboard
    {
        float sustain = MidiInterface::mapControlValue(value, 0.01f, 0.7f); // Map MIDI value to sustain time
        g_oscillator->setSustainLevel(sustain);
        std::cout << "CC23: Sustain level set to " << (sustain * 100.0f) << "%\n";
    }
    if (controller == 24) // Fourth Knob on Keyboard
    {
        float release = MidiInterface::mapControlValue(value, 0.01f, 5.0f); // Map MIDI value to release time
        g_oscillator->setReleaseTime(release);
        std::cout << "CC24: Release time set to " << release << "s\n";
    }
}

// MIDI aftertouch callback function
void midiAftertouchCallback(uint8_t channel, uint8_t pressure)
{
    // Suppress unused parameter warning
    (void)channel;
    (void)pressure;

    if (!g_oscillator)
        return;

    // Note: PolyphonicADSROscillator doesn't expose individual oscillators for frequency modulation
    // This functionality would need to be implemented within the PolyphonicADSROscillator class
    // For now, we'll skip frequency modulation via aftertouch

    std::cout << "Channel Aftertouch: pressure=" << static_cast<int>(pressure) << " (frequency modulation not implemented for polyphonic oscillator)\n";
}

// MIDI polyphonic aftertouch callback function
void midiPolyphonicAftertouchCallback(uint8_t channel, uint8_t note, uint8_t pressure)
{
    // Suppress unused parameter warning
    (void)channel;
    (void)note;
    (void)pressure;

    if (!g_oscillator)
        return;

    // Note: PolyphonicADSROscillator doesn't expose individual oscillators for frequency modulation
    // This functionality would need to be implemented within the PolyphonicADSROscillator class
    // For now, we'll skip frequency modulation via polyphonic aftertouch

    std::cout << "Polyphonic Aftertouch: note=" << static_cast<int>(note) << " pressure=" << static_cast<int>(pressure) << " (frequency modulation not implemented for polyphonic oscillator)\n";
}

int main()
{
    PlatformUtils::initialize();

    const char *controls[] = {
        "MIDI Notes - Control polyphonic oscillator (8 voices max)",
        "Channel Aftertouch - (not implemented for polyphonic oscillator)",
        "Polyphonic Aftertouch - (not implemented for polyphonic oscillator)",
        "CC 21 (Knob 1) - Adjust Attack time for all voices",
        "CC 22 (Knob 2) - Adjust Decay time for all voices",
        "CC 23 (Knob 3) - Adjust Sustain level for all voices",
        "CC 24 (Knob 4) - Adjust Release time for all voices",
        "1 - Switch to Sine wave (all voices)",
        "2 - Switch to Square wave (all voices)",
        "3 - Switch to Sawtooth wave (all voices)",
        "R - Start recording to WAV file",
        "s - Stop recording and save WAV file",
        "l - Show voice status",
        "a - All notes off",
        "q - Quit",
        nullptr};

    PlatformUtils::printHeader("Polyphonic MIDI Synthesizer Test",
                               "Real-time MIDI input with polyphonic envelope control via CC messages");
    PlatformUtils::printControls(controls);

    // List available MIDI devices
    std::cout << "Scanning MIDI input devices...\n";
    int num_midi_devices = MidiInterface::getDeviceCount();

    if (num_midi_devices == 0)
    {
        std::cout << "No MIDI input devices found!\n";
        std::cout << "Please connect a MIDI device or use a virtual MIDI port.\n";
        PlatformUtils::waitForInput("Press any key to exit...");
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
        return -1;
    }

    // Initialize oscillator - create a polyphonic ADSR oscillator with sine waves
    g_oscillator = std::make_unique<PolyphonicADSROscillator>(WaveType::SINE, static_cast<float>(SAMPLE_RATE));
    g_oscillator->setMasterAmplitude(0.8f);        // Set master amplitude scaling
    g_oscillator->setADSR(0.1f, 0.3f, 0.7f, 0.5f); // Set default ADSR for all voices

    // Initialize LFO - start with a slow sine wave, but inactive
    g_lfo = std::make_unique<LFO>(2.0f, LFOWaveType::SINE, static_cast<float>(SAMPLE_RATE));
    g_lfo->setAmplitude(0.0f); // Start with LFO off

    // Start audio output with callback
    if (sink.startWithCallback(audioCallback) != 0)
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
    midi_reader.setNoteCallback(midiNoteCallback);
    midi_reader.setControlCallback(midiControlCallBack);
    // midi_reader.setChannelAftertouchCallback(midiAftertouchCallback);
    midi_reader.setPolyphonicAftertouchCallback(midiPolyphonicAftertouchCallback);

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
        PlatformUtils::cleanup();
        return -1;
    }

    std::cout << "🎹 Polyphonic MIDI Synthesizer Ready! (8 voices)\n";
    std::cout << "Play notes on your MIDI device to control the polyphonic oscillator.\n";
    std::cout << "Current wave type: Sine\n";
    std::cout << "Recording file: " << g_recording_filename << "\n";
    std::cout << "Voice Status: Use 'l' to check active voices, 'a' for all notes off\n\n";

    // Main event loop - no longer need to manually play buffers
    while (g_running.load())
    {
        // Check for keyboard input (non-blocking)
        if (PlatformUtils::kbhit())
        {
            int key = PlatformUtils::getch();

            switch (key)
            {
            case '1':
                g_oscillator->setWaveType(WaveType::SINE);
                std::cout << "Wave type changed to: Sine (all voices)\n";
                break;
            case '2':
                g_oscillator->setWaveType(WaveType::SQUARE);
                std::cout << "Wave type changed to: Square (all voices)\n";
                break;
            case '3':
                g_oscillator->setWaveType(WaveType::SAW);
                std::cout << "Wave type changed to: Sawtooth (all voices)\n";
                break;
            case 'R':
                startRecording();
                break;
            case 's':
            case 'S':
                stopRecording();
                break;
            case 'l':
            case 'L':
                // Show voice status
                {
                    int active_voices = g_oscillator->getActiveVoiceCount();
                    int releasing_voices = g_oscillator->getReleasingVoiceCount();
                    std::cout << "Voice Status: " << active_voices << " active, "
                              << releasing_voices << " releasing, "
                              << (8 - active_voices) << " free\n";
                }
                break;
            case 'a':
            case 'A':
                // All notes off
                g_oscillator->allNotesOff();
                std::cout << "All notes off - starting release for all active voices\n";
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

    // Stop and save any ongoing recording
    if (g_recording.load())
    {
        std::cout << "Saving recording before exit...\n";
        stopRecording();
    }

    // Stop audio (this will join the audio thread)
    sink.stop();

    // Cleanup (RAII handles most cleanup automatically)
    g_oscillator.reset();
    g_lfo.reset();

    PlatformUtils::cleanup();
    std::cout << "Polyphonic MIDI Synthesizer stopped.\n";

    return 0;
}
