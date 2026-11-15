/**
 * @file synth_cli_v2.cpp
 * @brief CLI Synthesizer V2 - Using SynthEngine with per-voice filtering
 *
 * This is the updated version of synth_cli using the new SynthEngine
 * which provides:
 * - Per-voice filtering with velocity modulation
 * - 4 oscillator chains per voice
 * - Filter envelope per voice
 *
 * The original synth_cli is preserved for compatibility.
 */

#include "../common/audio_constants.hpp"
#include "../../lib/audio/audio_sink.hpp"
#include "../../lib/audio/audio_backend.hpp"
#include "../../lib/synth/synth.hpp"
#include "../../lib/oscillators/lfo.hpp"
#include "../../lib/utils/platform_midi_reader.hpp"
#include "../../lib/utils/wav_writer.hpp"
#include "../../lib/effects/reverb.hpp"
#include "../../lib/effects/gain.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <chrono>

using namespace synth;
using namespace examples;

// ============================================================================
// Global State
// ============================================================================

static std::unique_ptr<SynthEngine> g_synth = nullptr;
static std::unique_ptr<LFO> g_lfo = nullptr;
static std::unique_ptr<SchroederReverb> g_reverb = nullptr;
static std::unique_ptr<Gain> g_gain = nullptr;
static std::atomic<bool> g_running{true};

// Recording state
static std::atomic<bool> g_recording{false};
static std::vector<float> g_recording_buffer;
static std::mutex g_recording_mutex;
static std::string g_recording_filename = "session_v2.wav";

// Filter parameters (for display)
static float g_cutoff = 2000.0f;
static float g_resonance = 0.3f;
static float g_filter_env = 0.5f;
static float g_vel_sens = 0.5f;

// ============================================================================
// Audio Callback
// ============================================================================

void audioCallback(float *buffer, int bufferSize, int channels)
{
    if (g_synth && g_running.load())
    {
        if (channels == 2)
        {
            g_synth->generateStereoBuffer(buffer, bufferSize);

            // Apply reverb (deinterleave, process, reinterleave)
            if (g_reverb && g_reverb->getMix() > 0.01f)
            {
                std::vector<float> mono(bufferSize);
                for (int i = 0; i < bufferSize; ++i)
                {
                    mono[i] = (buffer[i * 2] + buffer[i * 2 + 1]) * 0.5f;
                }
                g_reverb->processBuffer(mono.data(), bufferSize);
                for (int i = 0; i < bufferSize; ++i)
                {
                    buffer[i * 2] = mono[i];
                    buffer[i * 2 + 1] = mono[i];
                }
            }

            // Apply gain
            if (g_gain)
            {
                g_gain->process(buffer, buffer, bufferSize * 2);
            }

            // Record if enabled
            if (g_recording.load())
            {
                std::lock_guard<std::mutex> lock(g_recording_mutex);
                g_recording_buffer.insert(g_recording_buffer.end(), buffer, buffer + bufferSize * 2);
            }
        }
        else
        {
            g_synth->generateBuffer(buffer, bufferSize);

            if (g_reverb)
            {
                g_reverb->processBuffer(buffer, bufferSize);
            }

            if (g_recording.load())
            {
                std::lock_guard<std::mutex> lock(g_recording_mutex);
                g_recording_buffer.insert(g_recording_buffer.end(), buffer, buffer + bufferSize);
            }
        }
    }
    else
    {
        int totalSamples = (channels == 2) ? bufferSize * 2 : bufferSize;
        std::fill(buffer, buffer + totalSamples, 0.0f);
    }
}

// ============================================================================
// Recording Functions
// ============================================================================

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
    std::cout << "Recording started... (will save to " << g_recording_filename << ")\n";
}

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

    WAVConfig config = WAVWriter::createConfig(SAMPLE_RATE, 2, 16);
    int result = WAVWriter::writeFile(g_recording_filename, g_recording_buffer,
                                      static_cast<int>(g_recording_buffer.size()), config);

    if (result == 0)
    {
        float duration = WAVWriter::calculateDuration(static_cast<int>(g_recording_buffer.size()), SAMPLE_RATE);
        std::cout << "Recording saved: " << g_recording_filename
                  << " (" << duration << "s)\n";
    }
    else
    {
        std::cout << "Failed to save recording!\n";
    }

    g_recording_buffer.clear();
}

// ============================================================================
// MIDI Callbacks
// ============================================================================

void midiNoteCallback(const MidiNote &note)
{
    if (!g_synth)
        return;

    if (note.is_note_on && note.velocity > 0)
    {
        float velocity = note.velocity / 127.0f;
        g_synth->noteOn(note.note, velocity);
        std::cout << "Note ON: " << static_cast<int>(note.note)
                  << " vel=" << static_cast<int>(note.velocity) << "\n";
    }
    else
    {
        g_synth->noteOff(note.note);
    }
}

void midiControlCallback(uint8_t channel, uint8_t controller, uint8_t value)
{
    (void)channel;
    if (!g_synth)
        return;

    float norm = value / 127.0f;

    switch (controller)
    {
    // Amp Envelope (CC 71-74)
    case 71:
        g_synth->setAttackTime(0.001f + norm * 2.0f);
        std::cout << "Amp Attack: " << (0.001f + norm * 2.0f) << "s\n";
        break;
    case 72:
        g_synth->setDecayTime(0.01f + norm * 3.0f);
        std::cout << "Amp Decay: " << (0.01f + norm * 3.0f) << "s\n";
        break;
    case 73:
        g_synth->setSustainLevel(norm);
        std::cout << "Amp Sustain: " << (norm * 100.0f) << "%\n";
        break;
    case 74:
        g_synth->setReleaseTime(0.01f + norm * 5.0f);
        std::cout << "Amp Release: " << (0.01f + norm * 5.0f) << "s\n";
        break;

    // Filter (CC 75-78)
    case 75:
        g_cutoff = 20.0f + norm * 19980.0f;
        g_synth->setCutoff(g_cutoff);
        std::cout << "Cutoff: " << g_cutoff << " Hz\n";
        break;
    case 76:
        g_resonance = norm;
        g_synth->setResonance(g_resonance);
        std::cout << "Resonance: " << (g_resonance * 100.0f) << "%\n";
        break;
    case 77:
        g_filter_env = norm * 2.0f - 1.0f; // -1 to +1
        g_synth->setFilterEnvAmount(g_filter_env);
        std::cout << "Filter Env: " << (g_filter_env * 100.0f) << "%\n";
        break;
    case 78:
        g_vel_sens = norm;
        g_synth->setFilterVelocitySensitivity(g_vel_sens);
        std::cout << "Vel->Filter: " << (g_vel_sens * 100.0f) << "%\n";
        break;

    // Filter Envelope (CC 79-82)
    case 79:
        g_synth->setFilterAttack(0.001f + norm * 2.0f);
        std::cout << "Filter Attack: " << (0.001f + norm * 2.0f) << "s\n";
        break;
    case 80:
        g_synth->setFilterDecay(0.01f + norm * 3.0f);
        std::cout << "Filter Decay: " << (0.01f + norm * 3.0f) << "s\n";
        break;
    case 81:
        g_synth->setFilterSustain(norm);
        std::cout << "Filter Sustain: " << (norm * 100.0f) << "%\n";
        break;
    case 82:
        g_synth->setFilterRelease(0.01f + norm * 5.0f);
        std::cout << "Filter Release: " << (0.01f + norm * 5.0f) << "s\n";
        break;

    // Effects (CC 83-85)
    case 83:
        if (g_reverb)
            g_reverb->setMix(norm);
        std::cout << "Reverb Mix: " << (norm * 100.0f) << "%\n";
        break;
    case 84:
        if (g_reverb)
            g_reverb->setRoomSize(norm);
        std::cout << "Reverb Room: " << (norm * 100.0f) << "%\n";
        break;
    case 85:
        if (g_reverb)
            g_reverb->setDamping(norm);
        std::cout << "Reverb Damp: " << (norm * 100.0f) << "%\n";
        break;

    // Master Gain (CC 7)
    case 7:
        if (g_gain)
            g_gain->set_gain(norm * 2.0f);
        std::cout << "Master Gain: " << (norm * 200.0f) << "%\n";
        break;

    // Mod Wheel (CC 1) - feeds ModMatrix
    case 1:
        g_synth->setModWheel(norm);
        if (g_lfo)
            g_lfo->setAmplitude(norm);
        std::cout << "Mod Wheel: " << (norm * 100.0f) << "%\n";
        break;
    }
}

void midiChannelAftertouchCallback(uint8_t channel, uint8_t pressure)
{
    (void)channel;
    if (!g_synth)
        return;

    g_synth->setChannelAftertouch(pressure / 127.0f);
}

void midiPitchBendCallback(uint8_t channel, int value)
{
    (void)channel;
    if (!g_synth)
        return;

    // Convert 14-bit pitch bend to semitones (-2 to +2)
    float semitones = ((value - 8192) / 8192.0f) * 2.0f;
    g_synth->setPitchBend(semitones);
}

// ============================================================================
// Print Functions
// ============================================================================

void printSignalPath()
{
    std::cout << "\n";
    std::cout << "==========================================================\n";
    std::cout << "                    SIGNAL PATH (per voice)               \n";
    std::cout << "==========================================================\n";
    std::cout << "\n";
    std::cout << "  MIDI Note (velocity) -----------------------------------+\n";
    std::cout << "                                                          |\n";
    std::cout << "  +------------------------------------------------------+|\n";
    std::cout << "  |  VOICE (1 of 32)                                     ||\n";
    std::cout << "  |                                                      ||\n";
    std::cout << "  |  +------------------+    +------------------------+  ||\n";
    std::cout << "  |  | MOD MATRIX       |    | OscillatorChain x4     |  ||\n";
    std::cout << "  |  |                  |    |                        |  ||\n";
    std::cout << "  |  | Sources:         |--->| Osc -> AmpEnv -> Filter|  ||\n";
    std::cout << "  |  |  - Velocity    <-+----+-----------+      |     |  ||\n";
    std::cout << "  |  |  - Aftertouch    |    |           |      |     |  ||\n";
    std::cout << "  |  |  - FilterEnv     |--->|-----------+----->|     |  ||\n";
    std::cout << "  |  |  - AmpEnv        |    |                  v     |  ||\n";
    std::cout << "  |  |  - LFO (global)  |    |              [Cutoff]  |  ||\n";
    std::cout << "  |  |  - ModWheel [CC1]|    |              [Reso]    |  ||\n";
    std::cout << "  |  |                  |    |                        |  ||\n";
    std::cout << "  |  | Destinations:    |    +------------------------+  ||\n";
    std::cout << "  |  |  - Filter Cutoff |                                ||\n";
    std::cout << "  |  |  - Filter Reso   |                                ||\n";
    std::cout << "  |  |  - Osc Pitch     |                                ||\n";
    std::cout << "  |  |  - Osc Level     |                                ||\n";
    std::cout << "  |  |  - Pan           |                                ||\n";
    std::cout << "  |  +------------------+                                ||\n";
    std::cout << "  +------------------------------------------------------+|\n";
    std::cout << "                            |                             |\n";
    std::cout << "  +------------------------------------------------------+|\n";
    std::cout << "  |  GLOBAL MOD SOURCES                                  ||\n";
    std::cout << "  |    LFO [CC 21-22]                                    ||\n";
    std::cout << "  |    ModWheel [CC 1]  -------------------------------->+\n";
    std::cout << "  |    Aftertouch                                        |\n";
    std::cout << "  +------------------------------------------------------+\n";
    std::cout << "                            |\n";
    std::cout << "                            v\n";
    std::cout << "  +------------------------------------------------------+\n";
    std::cout << "  |  GLOBAL EFFECTS                                      |\n";
    std::cout << "  |  Voice Mix --> Reverb --> Gain --> Output            |\n";
    std::cout << "  |               [CC 83-85] [CC 7]                      |\n";
    std::cout << "  +------------------------------------------------------+\n";
    std::cout << "\n";
    std::cout << "  ModMatrix: 32 slots, any source -> any destination\n";
    std::cout << "  Default routes: Velocity->Cutoff, FilterEnv->Cutoff\n";
    std::cout << "\n";
}

void printStatus()
{
    std::cout << "\n--- Current Settings ---\n";
    std::cout << "Wave: " << static_cast<int>(g_synth->getWaveType()) << "\n";
    std::cout << "Cutoff: " << g_cutoff << " Hz\n";
    std::cout << "Resonance: " << (g_resonance * 100.0f) << "%\n";
    std::cout << "Filter Env: " << (g_filter_env * 100.0f) << "%\n";
    std::cout << "Vel->Filter: " << (g_vel_sens * 100.0f) << "%\n";
    std::cout << "Active Voices: " << g_synth->getActiveVoiceCount() << "\n";
    std::cout << "------------------------\n";
}

void printControls()
{
    std::cout << "\n";
    std::cout << "==========================================================\n";
    std::cout << "      SYNTH CLI V2 - SynthEngine with Per-Voice Filter    \n";
    std::cout << "==========================================================\n";
    std::cout << " KEYBOARD:\n";
    std::cout << "   1-4   = Wave (Sine, Saw, Square, Triangle)\n";
    std::cout << "   Q/A   = Cutoff up/down\n";
    std::cout << "   W/S   = Resonance up/down\n";
    std::cout << "   E/D   = Filter Env up/down\n";
    std::cout << "   R/F   = Velocity Sens up/down\n";
    std::cout << "   SPACE = Test chord (C-E-G with different velocities)\n";
    std::cout << "   X     = All notes off\n";
    std::cout << "   P     = Print status\n";
    std::cout << "   O     = Start recording\n";
    std::cout << "   I     = Stop recording & save\n";
    std::cout << "   ESC   = Quit\n";
    std::cout << "----------------------------------------------------------\n";
    std::cout << " MIDI CC:\n";
    std::cout << "   71-74 = Amp ADSR\n";
    std::cout << "   75-78 = Filter (Cutoff, Reso, Env, VelSens)\n";
    std::cout << "   79-82 = Filter ADSR\n";
    std::cout << "   83-85 = Reverb (Mix, Room, Damp)\n";
    std::cout << "   1/7   = LFO / Master Gain\n";
    std::cout << "==========================================================\n";
    std::cout << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    PlatformUtils::initialize();

    std::cout << "Initializing Synth CLI V2 (SynthEngine)...\n";

    // Create synth engine
    g_synth = std::make_unique<SynthEngine>(static_cast<float>(SAMPLE_RATE), WaveType::SAW);

    // Configure initial parameters
    g_synth->setCutoff(g_cutoff);
    g_synth->setResonance(g_resonance);
    g_synth->setFilterEnvAmount(g_filter_env);
    g_synth->setFilterVelocitySensitivity(g_vel_sens);
    g_synth->setADSR(0.01f, 0.2f, 0.7f, 0.5f);
    g_synth->setFilterAttack(0.01f);
    g_synth->setFilterDecay(0.3f);
    g_synth->setFilterSustain(0.3f);
    g_synth->setFilterRelease(0.5f);

    // Effects
    g_gain = std::make_unique<Gain>();
    g_gain->set_gain(1.0f);

    g_reverb = std::make_unique<SchroederReverb>(static_cast<float>(SAMPLE_RATE));
    g_reverb->applyPreset(SchroederReverb::createMediumRoomPreset());
    g_reverb->setMix(0.3f);

    g_lfo = std::make_unique<LFO>(2.0f, LFOWaveType::SINE, static_cast<float>(SAMPLE_RATE));
    g_lfo->setAmplitude(0.0f);

    // Initialize audio
    AudioSink sink;

#ifdef _WIN32
    std::cout << "Trying ASIO (VASIO-32) backend...\n";
    selectAsioDriverByName("VASIO-32");
    sink.setBackend(AudioBackendType::ASIO);
    if (sink.init() != 0)
    {
        std::cout << "ASIO failed, trying DirectSound...\n";
        sink.setBackend(AudioBackendType::DirectSound);
        if (sink.init() != 0)
        {
            std::cout << "DirectSound failed, trying WaveOut...\n";
            sink.setBackend(AudioBackendType::WaveOut);
            if (sink.init() != 0)
            {
                std::cerr << "Failed to initialize audio!\n";
                return -1;
            }
        }
    }
#elif defined(__linux__)
#ifdef HAVE_PULSEAUDIO
    sink.setBackend(AudioBackendType::PulseAudio);
    if (sink.init() != 0)
    {
        sink.setBackend(AudioBackendType::ALSA);
        if (sink.init() != 0)
        {
            std::cerr << "Failed to initialize audio!\n";
            return -1;
        }
    }
#else
    sink.setBackend(AudioBackendType::ALSA);
    if (sink.init() != 0)
    {
        std::cerr << "Failed to initialize audio!\n";
        return -1;
    }
#endif
#endif

    std::cout << "Audio initialized.\n";

    // Start audio
    if (sink.startWithCallback(audioCallback) != 0)
    {
        std::cerr << "Failed to start audio!\n";
        return -1;
    }

    // Initialize MIDI
    int num_midi = MidiInterface::getDeviceCount();
    MidiInterface midi;

    if (num_midi > 0)
    {
        std::cout << "\nMIDI devices:\n";
        for (int i = 0; i < num_midi; ++i)
        {
            std::cout << "  [" << i << "] " << MidiInterface::getDeviceName(i) << "\n";
        }

        if (midi.init() == 0)
        {
            midi.setNoteCallback(midiNoteCallback);
            midi.setControlCallback(midiControlCallback);
            midi.setChannelAftertouchCallback(midiChannelAftertouchCallback);
            midi.setPitchBendCallback(midiPitchBendCallback);

            // Open first MIDI device (device 0)
            if (midi.open(0) == 0)
            {
                std::cout << "MIDI device opened: " << MidiInterface::getDeviceName(0) << "\n";
            }
            else
            {
                std::cout << "Failed to open MIDI device, using keyboard fallback.\n";
                midi.enableKeyboardFallback();
            }
        }
    }
    else
    {
        std::cout << "No MIDI devices found. Using keyboard fallback.\n";
        midi.enableKeyboardFallback();
        midi.setNoteCallback(midiNoteCallback);
        midi.setControlCallback(midiControlCallback);
    }

    printSignalPath();
    printControls();

    // Main loop
    while (g_running.load())
    {
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
                g_cutoff = std::min(g_cutoff * 1.2f, 20000.0f);
                g_synth->setCutoff(g_cutoff);
                std::cout << "Cutoff: " << g_cutoff << " Hz\n";
                break;
            case 'a':
            case 'A':
                g_cutoff = std::max(g_cutoff / 1.2f, 20.0f);
                g_synth->setCutoff(g_cutoff);
                std::cout << "Cutoff: " << g_cutoff << " Hz\n";
                break;

            case 'w':
            case 'W':
                g_resonance = std::min(g_resonance + 0.1f, 1.0f);
                g_synth->setResonance(g_resonance);
                std::cout << "Resonance: " << (g_resonance * 100.0f) << "%\n";
                break;
            case 's':
            case 'S':
                g_resonance = std::max(g_resonance - 0.1f, 0.0f);
                g_synth->setResonance(g_resonance);
                std::cout << "Resonance: " << (g_resonance * 100.0f) << "%\n";
                break;

            case 'e':
            case 'E':
                g_filter_env = std::min(g_filter_env + 0.1f, 1.0f);
                g_synth->setFilterEnvAmount(g_filter_env);
                std::cout << "Filter Env: " << (g_filter_env * 100.0f) << "%\n";
                break;
            case 'd':
            case 'D':
                g_filter_env = std::max(g_filter_env - 0.1f, -1.0f);
                g_synth->setFilterEnvAmount(g_filter_env);
                std::cout << "Filter Env: " << (g_filter_env * 100.0f) << "%\n";
                break;

            case 'r':
            case 'R':
                g_vel_sens = std::min(g_vel_sens + 0.1f, 1.0f);
                g_synth->setFilterVelocitySensitivity(g_vel_sens);
                std::cout << "Vel->Filter: " << (g_vel_sens * 100.0f) << "%\n";
                break;
            case 'f':
            case 'F':
                g_vel_sens = std::max(g_vel_sens - 0.1f, 0.0f);
                g_synth->setFilterVelocitySensitivity(g_vel_sens);
                std::cout << "Vel->Filter: " << (g_vel_sens * 100.0f) << "%\n";
                break;

            case ' ':
                g_synth->noteOn(60, 1.0f);  // C - bright
                g_synth->noteOn(64, 0.5f);  // E - darker
                g_synth->noteOn(67, 0.25f); // G - darkest
                std::cout << "Test chord: C(100%) E(50%) G(25%)\n";
                break;

            case 'x':
            case 'X':
                g_synth->allNotesOff();
                std::cout << "All notes off\n";
                break;

            case 'p':
            case 'P':
                printStatus();
                break;

            case 'o':
            case 'O':
                startRecording();
                break;

            case 'i':
            case 'I':
                stopRecording();
                break;
            }
        }
#endif

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Cleanup
    std::cout << "\nShutting down...\n";

    midi.close();
    sink.stop();
    g_synth.reset();

    std::cout << "Done.\n";
    return 0;
}
