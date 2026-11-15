/**
 * @file test_synth_engine.cpp
 * @brief Test application for SynthEngine with per-voice filtering
 *
 * Demonstrates the new SynthEngine features:
 * - Per-voice filter with velocity modulation
 * - 4 oscillator chains per voice
 * - Filter envelope
 *
 * Run with:
 *   ./build/bin/test_synth_engine
 */

#include "../app/common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/synth/synth.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

using namespace synth;
using namespace examples;

// Global synth engine
static std::unique_ptr<SynthEngine> g_synth = nullptr;
static std::atomic<bool> g_running{true};

// Audio callback
void audioCallback(float *buffer, int bufferSize, int channels)
{
    if (g_synth && g_running.load())
    {
        if (channels == 2)
        {
            g_synth->generateStereoBuffer(buffer, bufferSize);
        }
        else
        {
            g_synth->generateBuffer(buffer, bufferSize);
        }
    }
    else
    {
        int totalSamples = (channels == 2) ? bufferSize * 2 : bufferSize;
        std::fill(buffer, buffer + totalSamples, 0.0f);
    }
}

// MIDI callback
void midiCallback(const MidiMessage &msg)
{
    if (!g_synth)
        return;

    switch (msg.type)
    {
    case MidiMessageType::NOTE_ON:
        if (msg.data2 > 0)
        {
            float velocity = msg.data2 / 127.0f;
            g_synth->noteOn(msg.data1, velocity);
            std::cout << "Note ON: " << static_cast<int>(msg.data1)
                      << " vel=" << static_cast<int>(msg.data2) << "\n";
        }
        else
        {
            g_synth->noteOff(msg.data1);
        }
        break;

    case MidiMessageType::NOTE_OFF:
        g_synth->noteOff(msg.data1);
        std::cout << "Note OFF: " << static_cast<int>(msg.data1) << "\n";
        break;

    case MidiMessageType::CONTROL_CHANGE:
    {
        float value = msg.data2 / 127.0f;
        switch (msg.data1)
        {
        case 1: // Mod Wheel → Filter cutoff
            g_synth->setCutoff(200.0f + value * 10000.0f);
            std::cout << "Filter Cutoff: " << (200.0f + value * 10000.0f) << " Hz\n";
            break;

        case 71: // Attack
            g_synth->setAttackTime(0.001f + value * 2.0f);
            std::cout << "Attack: " << (0.001f + value * 2.0f) << "s\n";
            break;

        case 72: // Decay
            g_synth->setDecayTime(0.01f + value * 2.0f);
            std::cout << "Decay: " << (0.01f + value * 2.0f) << "s\n";
            break;

        case 73: // Sustain
            g_synth->setSustainLevel(value);
            std::cout << "Sustain: " << (value * 100.0f) << "%\n";
            break;

        case 74: // Release / Filter Resonance
            g_synth->setResonance(value);
            std::cout << "Resonance: " << (value * 100.0f) << "%\n";
            break;

        case 75:                                              // Filter Env Amount
            g_synth->setFilterEnvAmount(value * 2.0f - 1.0f); // -1 to +1
            std::cout << "Filter Env: " << ((value * 2.0f - 1.0f) * 100.0f) << "%\n";
            break;

        case 76: // Filter Velocity Sensitivity
            g_synth->setFilterVelocitySensitivity(value);
            std::cout << "Velocity→Filter: " << (value * 100.0f) << "%\n";
            break;
        }
        break;
    }

    case MidiMessageType::PITCH_BEND:
    {
        // Convert 14-bit pitch bend to semitones (-2 to +2)
        int bend = (msg.data2 << 7) | msg.data1;
        float semitones = ((bend - 8192) / 8192.0f) * 2.0f;
        g_synth->setPitchBend(semitones);
        break;
    }

    case MidiMessageType::CHANNEL_PRESSURE:
        g_synth->setChannelAftertouch(msg.data1 / 127.0f);
        break;

    case MidiMessageType::POLY_PRESSURE:
        g_synth->setPolyAftertouch(msg.data1, msg.data2 / 127.0f);
        break;

    default:
        break;
    }
}

void printUsage()
{
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           SYNTH ENGINE TEST - Per-Voice Filtering            ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Keyboard:                                                    ║\n";
    std::cout << "║   1-4  = Wave type (Sine, Saw, Square, Triangle)            ║\n";
    std::cout << "║   Q/A  = Cutoff up/down                                     ║\n";
    std::cout << "║   W/S  = Resonance up/down                                  ║\n";
    std::cout << "║   E/D  = Filter Env Amount up/down                          ║\n";
    std::cout << "║   R/F  = Velocity Sensitivity up/down                       ║\n";
    std::cout << "║   ESC  = Quit                                               ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ MIDI CC:                                                     ║\n";
    std::cout << "║   CC 1  = Filter Cutoff (Mod Wheel)                         ║\n";
    std::cout << "║   CC 71 = Attack                                            ║\n";
    std::cout << "║   CC 72 = Decay                                             ║\n";
    std::cout << "║   CC 73 = Sustain                                           ║\n";
    std::cout << "║   CC 74 = Resonance                                         ║\n";
    std::cout << "║   CC 75 = Filter Env Amount                                 ║\n";
    std::cout << "║   CC 76 = Velocity→Filter Sensitivity                       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
}

int main()
{
    PlatformUtils::initialize();

    std::cout << "Initializing SynthEngine Test...\n";

    // Create synth engine
    g_synth = std::make_unique<SynthEngine>(SAMPLE_RATE, WaveType::SAW);

    // Configure initial parameters
    g_synth->setCutoff(2000.0f);
    g_synth->setResonance(0.3f);
    g_synth->setFilterEnvAmount(0.5f);
    g_synth->setFilterVelocitySensitivity(0.5f);
    g_synth->setADSR(0.01f, 0.2f, 0.7f, 0.5f);

    // Filter envelope
    g_synth->setFilterAttack(0.01f);
    g_synth->setFilterDecay(0.3f);
    g_synth->setFilterSustain(0.3f);
    g_synth->setFilterRelease(0.5f);

    // Initialize audio
    auto audio_sink = AudioSink::create(SAMPLE_RATE, BUFFER_SIZE, audioCallback);
    if (!audio_sink || !audio_sink->initialize())
    {
        std::cerr << "Failed to initialize audio!\n";
        return 1;
    }

    // Initialize MIDI
    auto midi_reader = PlatformMidiReader::create();
    bool midi_ok = false;
    if (midi_reader)
    {
        midi_ok = midi_reader->initialize();
        if (midi_ok)
        {
            midi_reader->setCallback(midiCallback);
            midi_reader->startListening();
            std::cout << "MIDI input ready.\n";
        }
    }

    // Start audio
    audio_sink->start();
    std::cout << "Audio started.\n";

    printUsage();

    // Main loop
    float cutoff = 2000.0f;
    float resonance = 0.3f;
    float filter_env = 0.5f;
    float vel_sens = 0.5f;

    while (g_running.load())
    {
        // Check for keyboard input (platform-specific)
#ifdef _WIN32
        if (_kbhit())
        {
            int key = _getch();

            switch (key)
            {
            case 27: // ESC
                g_running.store(false);
                break;

            case '1':
                g_synth->setWaveType(WaveType::SINE);
                std::cout << "Wave: SINE\n";
                break;
            case '2':
                g_synth->setWaveType(WaveType::SAW);
                std::cout << "Wave: SAW\n";
                break;
            case '3':
                g_synth->setWaveType(WaveType::SQUARE);
                std::cout << "Wave: SQUARE\n";
                break;
            case '4':
                g_synth->setWaveType(WaveType::TRIANGLE);
                std::cout << "Wave: TRIANGLE\n";
                break;

            case 'q':
            case 'Q':
                cutoff = std::min(cutoff * 1.2f, 20000.0f);
                g_synth->setCutoff(cutoff);
                std::cout << "Cutoff: " << cutoff << " Hz\n";
                break;
            case 'a':
            case 'A':
                cutoff = std::max(cutoff / 1.2f, 20.0f);
                g_synth->setCutoff(cutoff);
                std::cout << "Cutoff: " << cutoff << " Hz\n";
                break;

            case 'w':
            case 'W':
                resonance = std::min(resonance + 0.1f, 1.0f);
                g_synth->setResonance(resonance);
                std::cout << "Resonance: " << (resonance * 100.0f) << "%\n";
                break;
            case 's':
            case 'S':
                resonance = std::max(resonance - 0.1f, 0.0f);
                g_synth->setResonance(resonance);
                std::cout << "Resonance: " << (resonance * 100.0f) << "%\n";
                break;

            case 'e':
            case 'E':
                filter_env = std::min(filter_env + 0.1f, 1.0f);
                g_synth->setFilterEnvAmount(filter_env);
                std::cout << "Filter Env: " << (filter_env * 100.0f) << "%\n";
                break;
            case 'd':
            case 'D':
                filter_env = std::max(filter_env - 0.1f, -1.0f);
                g_synth->setFilterEnvAmount(filter_env);
                std::cout << "Filter Env: " << (filter_env * 100.0f) << "%\n";
                break;

            case 'r':
            case 'R':
                vel_sens = std::min(vel_sens + 0.1f, 1.0f);
                g_synth->setFilterVelocitySensitivity(vel_sens);
                std::cout << "Velocity→Filter: " << (vel_sens * 100.0f) << "%\n";
                break;
            case 'f':
            case 'F':
                vel_sens = std::max(vel_sens - 0.1f, 0.0f);
                g_synth->setFilterVelocitySensitivity(vel_sens);
                std::cout << "Velocity→Filter: " << (vel_sens * 100.0f) << "%\n";
                break;

            case ' ':
                // Test chord with different velocities
                g_synth->noteOn(60, 1.0f);  // C4 - full velocity = bright
                g_synth->noteOn(64, 0.5f);  // E4 - half velocity = darker
                g_synth->noteOn(67, 0.25f); // G4 - quarter velocity = darkest
                std::cout << "Test chord: C-E-G with velocities 100%, 50%, 25%\n";
                break;

            case 'x':
            case 'X':
                g_synth->allNotesOff();
                std::cout << "All notes off\n";
                break;
            }
        }
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    std::cout << "\nShutting down...\n";

    if (midi_reader)
    {
        midi_reader->stopListening();
    }

    audio_sink->stop();
    g_synth.reset();

    std::cout << "Done.\n";
    return 0;
}
