#include "../common/audio_constants.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include "../lib/oscillators/pcm_waveform.hpp"
#include "../lib/oscillators/lfo.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "../lib/utils/platform_midi_reader.hpp"
#include "../lib/utils/wav_writer.hpp"
#include "../lib/effects/reverb.hpp"
#include "../lib/effects/chorus.hpp"
#include "../lib/effects/gain.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <atomic>
#include <vector>
#include <mutex>
#include <limits>
#include <unordered_map>
// #include "../lib/src/utils/logger.hpp"

using namespace synth;
using namespace examples;

// Global variables for MIDI control
static std::unique_ptr<PolyphonicOscillator> g_oscillator = nullptr;
static std::unique_ptr<LFO> g_lfo = nullptr;
std::unique_ptr<SchroederReverb> g_reverb;
std::unique_ptr<Chorus> g_chorus;
std::unique_ptr<Gain> g_gain;
static std::atomic<bool> g_note_playing{false};
static std::atomic<bool> g_running{true};
static uint8_t g_current_note = 0; // Track the currently playing note for polyphonic aftertouch

// Recording variables
static std::atomic<bool> g_recording{false};
static std::vector<float> g_recording_buffer;
static std::mutex g_recording_mutex;
static std::string g_recording_filename = "midi_session.wav";

// static std::unique_ptr<Logger> g_logger = nullptr;

// Function to cycle through chorus presets
void cycleChorusPreset()
{
    static int chorus_preset_index = 0;
    const char *chorus_preset_names[] = {"Subtle", "Classic", "Wide", "Deep"};

    switch (chorus_preset_index)
    {
    case 0: // Subtle
        g_chorus->setMix(0.3f);
        g_chorus->setDepth(0.3f);
        g_chorus->setRate(0.3f);
        g_chorus->setFeedback(0.1f);
        break;
    case 1: // Classic
        g_chorus->setMix(0.5f);
        g_chorus->setDepth(0.5f);
        g_chorus->setRate(0.5f);
        g_chorus->setFeedback(0.2f);
        break;
    case 2: // Wide
        g_chorus->setMix(0.6f);
        g_chorus->setDepth(0.7f);
        g_chorus->setRate(0.4f);
        g_chorus->setFeedback(0.3f);
        break;
    case 3: // Deep
        g_chorus->setMix(0.7f);
        g_chorus->setDepth(0.8f);
        g_chorus->setRate(0.6f);
        g_chorus->setFeedback(0.4f);
        break;
    }

    std::cout << "Chorus Preset: " << chorus_preset_names[chorus_preset_index]
              << " (Mix: " << (g_chorus->getMix() * 100.0f)
              << "%, Depth: " << (g_chorus->getDepth() * 100.0f)
              << "%, Rate: " << g_chorus->getRate()
              << " Hz, Feedback: " << (g_chorus->getFeedback() * 100.0f) << "%)\n";

    chorus_preset_index = (chorus_preset_index + 1) % 4;
}

void cycleReverbPresets()
{
    static int preset_index = 0;
    const char *preset_names[] = {"Small Room", "Medium Room", "Large Room", "Hall"};

    switch (preset_index)
    {
    case 0:
        g_reverb->applyPreset(SchroederReverb::createSmallRoomPreset());
        break;
    case 1:
        g_reverb->applyPreset(SchroederReverb::createMediumRoomPreset());
        break;
    case 2:
        g_reverb->applyPreset(SchroederReverb::createLargeRoomPreset());
        break;
    case 3:
        g_reverb->applyPreset(SchroederReverb::createHallPreset());
        break;
    }

    std::cout << "Reverb Preset: " << preset_names[preset_index]
              << " (Mix: " << (g_reverb->getMix() * 100.0f)
              << "%, Room: " << (g_reverb->getRoomSize() * 100.0f)
              << "%, Damping: " << (g_reverb->getDamping() * 100.0f) << "%)\n";

    preset_index = (preset_index + 1) % 4;
}

// Audio generation callback - this runs in the audio thread
void audioCallback(float *buffer, int bufferSize, int channels)
{
    if (g_oscillator && g_running.load())
    {
        if (channels == 2)
        {
            // Generate stereo audio directly
            g_oscillator->generateStereoBuffer(buffer, bufferSize);

            // // Apply reverb if available (reverb needs to be updated for stereo)
            // if (g_reverb)
            // {
            //     // Note: Current reverb processes mono, need to update for stereo
            //     // For now, process left and right channels separately
            //     std::vector<float> leftChannel(bufferSize);
            //     std::vector<float> rightChannel(bufferSize);

            //     // Deinterleave stereo to mono channels
            //     for (int i = 0; i < bufferSize; i++)
            //     {
            //         leftChannel[i] = buffer[i * 2];
            //         rightChannel[i] = buffer[i * 2 + 1];
            //     }

            //     // Process each channel
            //     g_reverb->processBuffer(leftChannel.data(), bufferSize);
            //     g_reverb->processBuffer(rightChannel.data(), bufferSize);

            //     // Reinterleave back to stereo
            //     for (int i = 0; i < bufferSize; i++)
            //     {
            //         buffer[i * 2] = leftChannel[i];
            //         buffer[i * 2 + 1] = rightChannel[i];
            //     }
            // }

            // // Apply chorus if available (chorus needs to be updated for stereo)
            // if (g_chorus)
            // {
            //     // Note: Current chorus processes mono, need to update for stereo
            //     // For now, process left and right channels separately
            //     std::vector<float> leftChannel(bufferSize);
            //     std::vector<float> rightChannel(bufferSize);

            //     // Deinterleave stereo to mono channels
            //     for (int i = 0; i < bufferSize; i++)
            //     {
            //         leftChannel[i] = buffer[i * 2];
            //         rightChannel[i] = buffer[i * 2 + 1];
            //     }

            //     // Process each channel
            //     g_chorus->processBuffer(leftChannel.data(), bufferSize);
            //     g_chorus->processBuffer(rightChannel.data(), bufferSize);

            //     // Reinterleave back to stereo
            //     for (int i = 0; i < bufferSize; i++)
            //     {
            //         buffer[i * 2] = leftChannel[i];
            //         buffer[i * 2 + 1] = rightChannel[i];
            //     }
            // }

            if (g_gain)
            {
                g_gain->process(buffer, buffer, bufferSize * 2); // Process stereo buffer
            }

            // Record stereo audio if recording is enabled
            if (g_recording.load())
            {
                std::lock_guard<std::mutex> lock(g_recording_mutex);
                g_recording_buffer.insert(g_recording_buffer.end(), buffer, buffer + bufferSize * 2);
            }
        }
        else
        {
            // Fallback to mono generation
            g_oscillator->generateBuffer(buffer, bufferSize);

            // Apply reverb if available
            if (g_reverb)
            {
                g_reverb->processBuffer(buffer, bufferSize);
            }

            // Apply chorus if available
            if (g_chorus)
            {
                g_chorus->processBuffer(buffer, bufferSize);
            }

            // Record audio if recording is enabled
            if (g_recording.load())
            {
                std::lock_guard<std::mutex> lock(g_recording_mutex);
                g_recording_buffer.insert(g_recording_buffer.end(), buffer, buffer + bufferSize);
            }
        }
    }
    else
    {
        // Fill with silence if no oscillator or not running
        int totalSamples = (channels == 2) ? bufferSize * 2 : bufferSize;
        std::fill(buffer, buffer + totalSamples, 0.0f);
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

    // Create WAV configuration (stereo, 16-bit, 48kHz)
    WAVConfig config = WAVWriter::createConfig(SAMPLE_RATE, 2, 16); // 2 channels (stereo), 16 bits

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

// PCM file selection function
void selectPCMFile()
{
    auto pcm_files = PCMWaveform::listPCMFiles("pcm");

    if (pcm_files.empty())
    {
        std::cout << "No PCM files found in pcm/ directory.\n";
        std::cout << "Please place WAV files in the pcm/ directory to use custom waveforms.\n";
        return;
    }

    std::cout << "\nAvailable PCM files:\n";
    std::cout << "====================\n";
    for (size_t i = 0; i < pcm_files.size(); ++i)
    {
        std::cout << "  " << (i + 1) << ". " << pcm_files[i] << "\n";
    }
    std::cout << "  " << (pcm_files.size() + 1) << ". Cancel\n";
    std::cout << "\nSelect a PCM file (1-" << (pcm_files.size() + 1) << "): ";

    // Simple number input (this is a basic implementation)
    // In a full implementation, you'd want proper input handling
    std::string input;
    std::getline(std::cin, input);

    try
    {
        int selection = std::stoi(input);
        if (selection >= 1 && selection <= static_cast<int>(pcm_files.size()))
        {
            std::string filename = "pcm/" + pcm_files[selection - 1];

            // Create PCM waveform and load from file
            auto pcm_waveform = std::make_shared<PCMWaveform>();

            bool loaded = false;

            // Check if this is a wav bank file
            if (pcm_files[selection - 1].find("bank_") == 0)
            {
                // Load as wav bank
                loaded = pcm_waveform->loadWavBank(filename);
                if (loaded)
                {
                    std::cout << "✓ Successfully loaded wav bank: " << pcm_files[selection - 1] << "\n";
                    std::cout << "  Wave count: " << pcm_waveform->getBankWaveCount() << "\n";
                    std::cout << "  Total samples: " << pcm_waveform->getSampleCount() << "\n";
                    std::cout << "  Samples per wave: " << (pcm_waveform->getSampleCount() / pcm_waveform->getBankWaveCount()) << "\n";
                    std::cout << "  Use CC 77 to control bank position (0-100%)\n";
                }
            }
            else
            {
                // Load as single waveform
                loaded = pcm_waveform->loadFromFile(filename);
                if (loaded)
                {
                    std::cout << "✓ Successfully loaded: " << pcm_files[selection - 1] << "\n";
                    std::cout << "  Sample count: " << pcm_waveform->getSampleCount() << "\n";
                }
            }

            if (loaded)
            {
                // Get the raw data and set to all voices
                const auto &waveform_data = pcm_waveform->getData();
                std::cout << "DEBUG: synth_cli waveform_data size: " << waveform_data.size() << std::endl;
                if (!waveform_data.empty())
                {
                    std::cout << "DEBUG: synth_cli first sample: " << waveform_data[0]
                              << ", last sample: " << waveform_data[waveform_data.size() - 1] << std::endl;
                    if (waveform_data.size() > 100)
                    {
                        std::cout << "DEBUG: synth_cli middle sample [100]: " << waveform_data[100] << std::endl;
                        if (waveform_data.size() > 1000)
                        {
                            std::cout << "DEBUG: synth_cli middle sample [1000]: " << waveform_data[1000] << std::endl;
                        }
                    }
                }

                // Set wave type to PCM_CUSTOM and apply to all voices
                g_oscillator->setWaveType(WaveType::PCM_CUSTOM);

                // Set the custom waveform using PCMWaveform object to preserve wav bank metadata
                g_oscillator->setCustomWaveform(pcm_waveform);

                std::string type_desc = pcm_waveform->isWavBank() ? "PCM Wav Bank" : "PCM Custom";
                std::cout << "Wave type changed to: " << type_desc << " (" << pcm_files[selection - 1] << ")\n";
            }
            else
            {
                std::cout << "✗ Failed to load: " << pcm_files[selection - 1] << "\n";
            }
        }
        else if (selection == static_cast<int>(pcm_files.size() + 1))
        {
            std::cout << "PCM selection cancelled.\n";
        }
        else
        {
            std::cout << "Invalid selection.\n";
        }
    }
    catch (const std::exception &)
    {
        std::cout << "Invalid input. Please enter a number.\n";
    }
}

// MIDI CC Parameter mapping structure
struct CCParameter
{
    enum class Type
    {
        OSCILLATOR_ATTACK,
        OSCILLATOR_DECAY,
        OSCILLATOR_SUSTAIN,
        OSCILLATOR_RELEASE,
        OSCILLATOR_DRY_WET_MIX,
        OSCILLATOR_BANK_POSITION,
        REVERB_MIX,
        REVERB_ROOM_SIZE,
        REVERB_DAMPING,
        CHORUS_MIX,
        CHORUS_DEPTH,
        CHORUS_RATE,
        CHORUS_FEEDBACK,
        LFO_FREQUENCY,
        LFO_AMPLITUDE,
        GAIN_LEVEL,
        FM_MODULATOR_RATIO,
        FM_MODULATION_INDEX,
        FM_ALGORITHM,
        FM_OPERATOR2_RATIO,
        FM_OPERATOR3_RATIO,
        FM_OPERATOR4_RATIO,
        FM_OPERATOR2_LEVEL,
        FM_OPERATOR3_LEVEL,
        FM_OPERATOR4_LEVEL,
        FM_FEEDBACK
    };

    Type type;
    float min_value;
    float max_value;
    const char *description;
};

// MIDI CC to Parameter mapping
static const std::unordered_map<uint8_t, CCParameter> cc_parameter_map = {
    // === LAUNCHKEY 49 FADERS (CC 41-48) ===
    {71, {CCParameter::Type::OSCILLATOR_ATTACK, 0.01f, 2.0f, "Attack Time"}},
    {72, {CCParameter::Type::OSCILLATOR_DECAY, 0.01f, 5.0f, "Decay Time"}},
    {73, {CCParameter::Type::OSCILLATOR_SUSTAIN, 0.0f, 1.0f, "Sustain Level"}},
    {74, {CCParameter::Type::OSCILLATOR_RELEASE, 0.01f, 5.0f, "Release Time"}},
    {75, {CCParameter::Type::REVERB_MIX, 0.0f, 0.8f, "Reverb Mix"}},
    {76, {CCParameter::Type::CHORUS_MIX, 0.0f, 1.0f, "Chorus Mix"}},
    {77, {CCParameter::Type::OSCILLATOR_DRY_WET_MIX, 0.0f, 1.0f, "Dry/Wet Mix"}},

    {79, {CCParameter::Type::GAIN_LEVEL, 0.0f, 2.0f, "Master Gain"}},

    // === LAUNCHKEY 49 KNOBS (CC 21-28) ===
    {21, {CCParameter::Type::LFO_FREQUENCY, 0.0f, 20.0f, "LFO Frequency"}},
    {22, {CCParameter::Type::LFO_AMPLITUDE, 0.0f, 1.0f, "LFO Amplitude"}},
    {23, {CCParameter::Type::REVERB_ROOM_SIZE, 0.0f, 1.0f, "Reverb Room Size"}},
    {24, {CCParameter::Type::REVERB_DAMPING, 0.0f, 1.0f, "Reverb Damping"}},
    {25, {CCParameter::Type::CHORUS_DEPTH, 0.0f, 1.0f, "Chorus Depth"}},
    {26, {CCParameter::Type::CHORUS_RATE, 0.1f, 5.0f, "Chorus Rate"}},
    {27, {CCParameter::Type::CHORUS_FEEDBACK, 0.0f, 0.7f, "Chorus Feedback"}},
    {28, {CCParameter::Type::OSCILLATOR_BANK_POSITION, 0.0f, 1.0f, "Bank Position"}},

    // === FM SYNTHESIS CONTROLS (CC 29-36) ===
    {29, {CCParameter::Type::FM_MODULATOR_RATIO, 0.1f, 16.0f, "FM Modulator Ratio"}},
    {30, {CCParameter::Type::FM_MODULATION_INDEX, 0.0f, 20.0f, "FM Modulation Index"}},
    {31, {CCParameter::Type::FM_ALGORITHM, 0.0f, 4.0f, "FM Algorithm"}},
    {32, {CCParameter::Type::FM_OPERATOR2_RATIO, 0.1f, 16.0f, "FM Op2 Ratio"}},
    {33, {CCParameter::Type::FM_OPERATOR3_RATIO, 0.1f, 16.0f, "FM Op3 Ratio"}},
    {34, {CCParameter::Type::FM_OPERATOR4_RATIO, 0.1f, 16.0f, "FM Op4 Ratio"}},
    {35, {CCParameter::Type::FM_OPERATOR2_LEVEL, 0.0f, 1.0f, "FM Op2 Level"}},
    {36, {CCParameter::Type::FM_FEEDBACK, 0.0f, 1.0f, "FM Feedback"}},

    // === STANDARD MIDI CCs ===
    {1, {CCParameter::Type::LFO_AMPLITUDE, 0.0f, 1.0f, "Modulation Wheel"}},
    {7, {CCParameter::Type::GAIN_LEVEL, 0.0f, 2.0f, "Volume"}},
    {11, {CCParameter::Type::REVERB_MIX, 0.0f, 0.8f, "Expression"}},
};

// Apply CC parameter function
void applyCCParameter(const CCParameter &param, uint8_t midi_value)
{
    float mapped_value = MidiInterface::mapControlValue(midi_value, param.min_value, param.max_value);

    switch (param.type)
    {
    case CCParameter::Type::OSCILLATOR_ATTACK:
        if (g_oscillator)
        {
            g_oscillator->setAttackTime(mapped_value);
            std::cout << param.description << ": " << mapped_value << "s\n";
        }
        break;

    case CCParameter::Type::OSCILLATOR_DECAY:
        if (g_oscillator)
        {
            g_oscillator->setDecayTime(mapped_value);
            std::cout << param.description << ": " << mapped_value << "s\n";
        }
        break;

    case CCParameter::Type::OSCILLATOR_SUSTAIN:
        if (g_oscillator)
        {
            g_oscillator->setSustainLevel(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::OSCILLATOR_RELEASE:
        if (g_oscillator)
        {
            g_oscillator->setReleaseTime(mapped_value);
            std::cout << param.description << ": " << mapped_value << "s\n";
        }
        break;

    case CCParameter::Type::OSCILLATOR_DRY_WET_MIX:
        if (g_oscillator)
        {
            g_oscillator->setDryWetMix(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "% wet\n";
        }
        break;

    case CCParameter::Type::OSCILLATOR_BANK_POSITION:
        if (g_oscillator && g_oscillator->isWavBank())
        {
            g_oscillator->setBankPosition(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        else if (g_chorus)
        {
            // Fallback to chorus feedback if no wav bank
            g_chorus->setFeedback(mapped_value);
            std::cout << "Chorus Feedback: " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::REVERB_MIX:
        if (g_reverb)
        {
            g_reverb->setMix(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::REVERB_ROOM_SIZE:
        if (g_reverb)
        {
            g_reverb->setRoomSize(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::REVERB_DAMPING:
        if (g_reverb)
        {
            g_reverb->setDamping(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::CHORUS_MIX:
        if (g_chorus)
        {
            g_chorus->setMix(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::CHORUS_DEPTH:
        if (g_chorus)
        {
            g_chorus->setDepth(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::CHORUS_RATE:
        if (g_chorus)
        {
            g_chorus->setRate(mapped_value);
            std::cout << param.description << ": " << mapped_value << " Hz\n";
        }
        break;

    case CCParameter::Type::CHORUS_FEEDBACK:
        if (g_chorus)
        {
            g_chorus->setFeedback(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::LFO_FREQUENCY:
        if (g_lfo)
        {
            g_lfo->setFrequency(mapped_value);
            std::cout << param.description << ": " << mapped_value << " Hz\n";
        }
        break;

    case CCParameter::Type::LFO_AMPLITUDE:
        if (g_lfo)
        {
            g_lfo->setAmplitude(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::GAIN_LEVEL:
        if (g_gain)
        {
            g_gain->set_gain(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::FM_MODULATOR_RATIO:
        if (g_oscillator)
        {
            g_oscillator->setFMModulatorRatio(mapped_value);
            std::cout << param.description << ": " << mapped_value << "\n";
        }
        break;

    case CCParameter::Type::FM_MODULATION_INDEX:
        if (g_oscillator)
        {
            g_oscillator->setFMModulationIndex(mapped_value);
            std::cout << param.description << ": " << mapped_value << "\n";
        }
        break;

    case CCParameter::Type::FM_ALGORITHM:
        if (g_oscillator)
        {
            // Map 0.0-4.0 to algorithm enum values
            int algorithm_index = static_cast<int>(mapped_value);
            if (algorithm_index > 4)
                algorithm_index = 4;
            FMAlgorithm algorithm = static_cast<FMAlgorithm>(algorithm_index);
            g_oscillator->setFMAlgorithm(algorithm);

            const char *algorithm_names[] = {"2-Op Simple", "2-Op Feedback", "4-Op Serial", "4-Op Parallel", "4-Op Mixed"};
            std::cout << param.description << ": " << algorithm_names[algorithm_index] << "\n";
        }
        break;

    case CCParameter::Type::FM_OPERATOR2_RATIO:
        if (g_oscillator)
        {
            g_oscillator->setFMOperatorRatio(2, mapped_value);
            std::cout << param.description << ": " << mapped_value << "\n";
        }
        break;

    case CCParameter::Type::FM_OPERATOR3_RATIO:
        if (g_oscillator)
        {
            g_oscillator->setFMOperatorRatio(3, mapped_value);
            std::cout << param.description << ": " << mapped_value << "\n";
        }
        break;

    case CCParameter::Type::FM_OPERATOR4_RATIO:
        if (g_oscillator)
        {
            g_oscillator->setFMOperatorRatio(4, mapped_value);
            std::cout << param.description << ": " << mapped_value << "\n";
        }
        break;

    case CCParameter::Type::FM_OPERATOR2_LEVEL:
        if (g_oscillator)
        {
            g_oscillator->setFMOperatorLevel(2, mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::FM_OPERATOR3_LEVEL:
        if (g_oscillator)
        {
            g_oscillator->setFMOperatorLevel(3, mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::FM_OPERATOR4_LEVEL:
        if (g_oscillator)
        {
            g_oscillator->setFMOperatorLevel(4, mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;

    case CCParameter::Type::FM_FEEDBACK:
        if (g_oscillator)
        {
            g_oscillator->setFMFeedback(mapped_value);
            std::cout << param.description << ": " << (mapped_value * 100.0f) << "%\n";
        }
        break;
    }
}

// MIDI note callback function
void midiNoteCallback(const MidiNote &note)
{

    std::cout << "MIDI Note - Channel: " << static_cast<int>(note.channel)
              << ", Note: " << static_cast<int>(note.note)
              << ", Velocity: " << static_cast<int>(note.velocity) << "\n";

    if (!g_oscillator)
        return;

    if (note.is_note_on && note.velocity > 0)
    {
        // Note ON - start playing
        g_current_note = note.note; // Track current note for polyphonic aftertouch

        g_note_playing.store(true);

        // Set velocity based on MIDI velocity (0-127 -> 0.0f-1.0f)
        float velocity = MidiInterface::mapControlValue(note.velocity, 0.6f, 1.0f);

        // Use polyphonic noteOn with note number and velocity
        g_oscillator->noteOn(note.note, velocity);

        std::cout << "♪ Playing MIDI note " << static_cast<int>(note.note) << " at "
                  << static_cast<int>(velocity * 100.0f + 0.5f) << "% velocity\n";
    }
    else
    {
        // Note OFF - stop playing this specific note
        g_oscillator->noteOff(note.note);

        // Check voice status
        int active_voices = g_oscillator->getActiveVoiceCount();
        int releasing_voices = g_oscillator->getReleasingVoiceCount();

        if (active_voices == releasing_voices) // All remaining voices are just releasing
        {
            g_note_playing.store(false);
        }

        // std::cout << "♪ Note OFF: " << static_cast<int>(note.note)
        //          << " (Active: " << active_voices << ", Releasing: " << releasing_voices << ")\n";
    }
}

void midiControlCallBack(uint8_t channel, uint8_t controller, uint8_t value)
{
    // Suppress unused parameter warning
    (void)channel;

    std::cout << "MIDI Control Change - Controller: " << static_cast<int>(controller)
              << ", Value: " << static_cast<int>(value) << "\n";

    // Look up the CC parameter mapping
    auto it = cc_parameter_map.find(controller);
    if (it != cc_parameter_map.end())
    {
        const CCParameter &param = it->second;
        applyCCParameter(param, value);
    }
    else
    {
        std::cout << "Unmapped CC: " << static_cast<int>(controller) << " = " << static_cast<int>(value) << "\n";
    }
}

// Print CC mapping information
void printCCMappings()
{
    std::cout << "\n=== LAUNCHKEY 49 CONTROL MAPPING ===\n";
    std::cout << "FADERS (CC 41-48):\n";
    std::cout << "  CC 41 - Attack Time (0.01-2.0s)\n";
    std::cout << "  CC 42 - Decay Time (0.01-5.0s)\n";
    std::cout << "  CC 43 - Sustain Level (0-100%)\n";
    std::cout << "  CC 44 - Release Time (0.01-5.0s)\n";
    std::cout << "  CC 45 - Reverb Mix (0-80%)\n";
    std::cout << "  CC 46 - Chorus Mix (0-100%)\n";
    std::cout << "  CC 47 - Dry/Wet Mix (0-100%)\n";
    std::cout << "  CC 48 - Master Gain (0-200%)\n\n";

    std::cout << "KNOBS (CC 21-28):\n";
    std::cout << "  CC 21 - LFO Frequency (0-20 Hz)\n";
    std::cout << "  CC 22 - LFO Amplitude (0-100%)\n";
    std::cout << "  CC 23 - Reverb Room Size (0-100%)\n";
    std::cout << "  CC 24 - Reverb Damping (0-100%)\n";
    std::cout << "  CC 25 - Chorus Depth (0-100%)\n";
    std::cout << "  CC 26 - Chorus Rate (0.1-5.0 Hz)\n";
    std::cout << "  CC 27 - Chorus Feedback (0-70%)\n";
    std::cout << "  CC 28 - Bank Position (0-100%)\n\n";

    std::cout << "FM SYNTHESIS (CC 29-36):\n";
    std::cout << "  CC 29 - FM Modulator Ratio (0.1-10.0)\n";
    std::cout << "  CC 30 - FM Modulation Index (0-20.0)\n";
    std::cout << "  CC 31 - FM Algorithm (0-4)\n";
    std::cout << "  CC 32 - FM Operator 2 Ratio (0.1-10.0)\n";
    std::cout << "  CC 33 - FM Operator 2 Level (0-100%)\n";
    std::cout << "  CC 34 - FM Operator 3 Ratio (0.1-10.0)\n";
    std::cout << "  CC 35 - FM Operator 3 Level (0-100%)\n";
    std::cout << "  CC 36 - FM Operator 4 Ratio (0.1-10.0)\n\n";

    std::cout << "STANDARD MIDI:\n";
    std::cout << "  CC 1 - Modulation Wheel (LFO Amplitude)\n";
    std::cout << "  CC 7 - Volume (Master Gain)\n";
    std::cout << "  CC 11 - Expression (Reverb Mix)\n\n";
}

// MIDI aftertouch callback function
void midiAftertouchCallback(uint8_t channel, uint8_t pressure)
{
    // Suppress unused parameter warning
    (void)channel;
    (void)pressure;

    if (!g_oscillator)
        return;

    // TODO: Implement aftertouch for polyphonic oscillator
    // PolyphonicADSROscillator doesn't currently support frequency modulation
    // This would require extending the API to support per-voice modulation
    // std::cout << "Aftertouch: " << static_cast<int>(pressure) << " (not implemented for polyphonic mode)\n";
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

    // TODO: Implement polyphonic aftertouch for polyphonic oscillator
    // PolyphonicADSROscillator doesn't currently support per-voice frequency modulation
    // This would require extending the API to support per-voice modulation
    // std::cout << "Polyphonic Aftertouch: Note " << static_cast<int>(note)
    //           << " Pressure " << static_cast<int>(pressure)
    //           << " (not implemented for polyphonic mode)\n";
}

int main()
{
    PlatformUtils::initialize();

    const char *controls[] = {
        "MIDI Notes - Polyphonic control (up to 8 simultaneous notes)",
        " === LAUNCHKEY 49 CONTROL MAPPING ===",
        "Faders (CC 41-48) - Attack, Decay, Sustain, Release, Reverb, Chorus, Dry/Wet, Gain",
        "Knobs (CC 21-28) - LFO Freq/Amp, Reverb Room/Damping, Chorus Depth/Rate/Feedback, Bank Pos",
        " === ENVELOPE ===",
        "CC 41 (Fader1) - Attack time (0.01-2.0s)",
        "CC 42 (Fader2) - Decay time (0.01-5.0s)",
        "CC 43 (Fader3) - Sustain level (0-100%)",
        "CC 44 (Fader4) - Release time (0.01-5.0s)",
        "CC 47 (Fader7) - Dry/Wet Mix (0-100% wet)",
        " ==== WAVE SELECTION ====",
        "1 - Switch to Sine wave (all voices)",
        "2 - Switch to Square wave (all voices)",
        "3 - Switch to Sawtooth wave (all voices)",
        "4 - Switch to Triangle wave (all voices)",
        "5 - Switch to Pulse wave (all voices)",
        "6 - Switch to Noise wave (all voices)",
        "7 - Switch to Ramp Up wave (all voices)",
        "8 - Switch to Ramp Down wave (all voices)",
        "9 - Switch to FM Synthesis (all voices)",
        " === FM SYNTHESIS ===",
        "F - Cycle FM algorithm (2-op simple, 2-op feedback, 4-op serial, parallel, mixed)",
        "CC 29 - FM Modulator Ratio (0.1-10.0)",
        "CC 30 - FM Modulation Index (0-20.0)",
        "CC 31 - FM Algorithm (0-4)",
        "CC 32 - FM Operator 2 Ratio (0.1-10.0)",
        "CC 33 - FM Operator 2 Level (0-100%)",
        "CC 34 - FM Operator 3 Ratio (0.1-10.0)",
        "CC 35 - FM Operator 3 Level (0-100%)",
        "CC 36 - FM Operator 4 Ratio (0.1-10.0)",
        " ==== RECORDING ====",
        "R - Start recording to WAV file",
        "s - Stop recording and save WAV file",
        " === REVERB ====",
        "v - Toggle reverb on/off",
        "V - Cycle reverb presets",
        "CC 45 (Fader5) - Reverb Mix (0-80%)",
        "CC 23 (Knob3) - Room Size (0-100%)",
        "CC 24 (Knob4) - Reverb Damping (0-100%)",
        " === CHORUS ====",
        "c - Toggle chorus on/off",
        "C - Cycle chorus presets",
        "CC 46 (Fader6) - Chorus Mix (0-100%)",
        "CC 25 (Knob5) - Chorus Depth (0-100%)",
        "CC 26 (Knob6) - Chorus Rate (0.1-5.0 Hz)",
        "CC 27 (Knob7) - Chorus Feedback (0-70%)",
        " === LFO ====",
        "CC 21 (Knob1) - LFO Frequency (0-20 Hz)",
        "CC 22 (Knob2) - LFO Amplitude (0-100%)",
        "l - Toggle LFO on/off (not implemented for polyphonic mode)",
        " === MISC ====",
        "CC 48 (Fader8) - Master Gain (0-200%)",
        "CC 28 (Knob8) - Wav Bank Position (0-100%)",
        "CC 1 (Mod Wheel) - LFO Amplitude (0-100%)",
        "CC 7 (Volume) - Master Gain (0-200%)",
        "CC 11 (Expression) - Reverb Mix (0-80%)",
        " === BANK POSITION TEST ===",
        "0 - Set bank position to 0%",
        "( - Set bank position to 80%",
        "[ - Set bank position to 75%",
        "] - Set bank position to 100%",
        "m - Show detailed CC mapping",
        "q - Quit",
        nullptr};

    PlatformUtils::printHeader("Polyphonic MIDI Synthesizer",
                               "Real-time polyphonic MIDI input with envelope control via CC messages");
    PlatformUtils::printControls(controls);

    // List available MIDI devices
    std::cout << "Scanning MIDI input devices...\n";
    int num_midi_devices = MidiInterface::getDeviceCount();


    if (num_midi_devices == 0)
    {
        std::cout << "No MIDI input devices found!\n";
        std::cout << "Please connect a MIDI device or use a virtual MIDI port.\n";
        std::cout << "The synthesizer will continue in audio-only mode (no MIDI input).\n";
        std::cout << "You can still test audio output and recording features.\n\n";
    }
    else
    {
        // Get device list and display with indices
        std::cout << "\nAvailable MIDI input devices:\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        for (int i = 0; i < num_midi_devices; i++)
        {
            std::string device_name = MidiInterface::getDeviceName(i);
            std::cout << "  [" << i << "] " << device_name;

            // Add some helpful context for known device types
            if (device_name.find("Launchpad") != std::string::npos ||
                device_name.find("Launch") != std::string::npos)
            {
                std::cout << " (MIDI Controller)";
            }
            else if (device_name.find("Headset") != std::string::npos ||
                     device_name.find("Audio") != std::string::npos)
            {
                std::cout << " (Audio Device)";
            }
            else if (device_name.find("HDA") != std::string::npos ||
                     device_name.find("sof-") != std::string::npos)
            {
                std::cout << " (System Audio)";
            }

            std::cout << "\n";
        }
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    }

    // List available audio devices
    std::cout << "Scanning audio output devices...\n";

    AudioSink::listDevices();

    // Initialize audio sink
    AudioSink sink;

    // Platform-specific audio backend initialization
#ifdef _WIN32
    // Audio backend priority: DirectSound (best performance) -> WASAPI (lowest latency) -> WaveOut (most compatible)
    // Try DirectSound first, then WASAPI, then fallback to WaveOut
    std::cout << "Trying DirectSound backend...\n";
    sink.setBackend(AudioBackendType::DirectSound);

    if (sink.init() != 0)
    {
        std::cout << "DirectSound initialization failed, trying WASAPI backend...\n";
        sink.setBackend(AudioBackendType::WASAPI);

        if (sink.init() != 0)
        {
            std::cout << "WASAPI initialization failed, trying WaveOut backend...\n";
            sink.setBackend(AudioBackendType::WaveOut);
            if (sink.init() != 0)
            {
                std::cout << "Failed to initialize audio sink with any backend\n";
                return -1;
            }
            else
            {
                std::cout << "Successfully initialized WaveOut backend\n";
            }
        }
        else
        {
            std::cout << "Successfully initialized WASAPI backend\n";
        }
    }
    else
    {
        std::cout << "Successfully initialized DirectSound backend\n";
    }
#elif defined(__linux__)
    // Linux audio backend priority: PulseAudio -> ALSA
#ifdef HAVE_PULSEAUDIO
    std::cout << "Trying PulseAudio backend...\n";
    sink.setBackend(AudioBackendType::PulseAudio);

    if (sink.init() != 0)
    {
        std::cout << "PulseAudio initialization failed, trying ALSA backend...\n";
        sink.setBackend(AudioBackendType::ALSA);
        if (sink.init() != 0)
        {
            std::cout << "Failed to initialize audio sink with any backend\n";
            return -1;
        }
        else
        {
            std::cout << "Successfully initialized ALSA backend\n";
        }
    }
    else
    {
        std::cout << "Successfully initialized PulseAudio backend\n";
    }
#else
    std::cout << "Trying ALSA backend...\n";
    sink.setBackend(AudioBackendType::ALSA);
    if (sink.init() != 0)
    {
        std::cout << "Failed to initialize ALSA audio backend\n";
        return -1;
    }
    else
    {
        std::cout << "Successfully initialized ALSA backend\n";
    }
#endif
#endif

    // Initialize oscillator - create a polyphonic ADSR oscillator
    g_oscillator = std::make_unique<PolyphonicOscillator>(WaveType::SINE, static_cast<float>(SAMPLE_RATE));
    g_oscillator->setMasterAmplitude(1.f); // Set master volume to 100%

    g_gain = std::make_unique<Gain>();
    g_gain->set_gain(1.0f); // Start with lower gain to prevent

    g_oscillator->setADSR(1.0f, 1.0f, 0.8f, 1.0f);

    g_lfo = std::make_unique<LFO>(2.0f, LFOWaveType::SINE, static_cast<float>(SAMPLE_RATE));
    g_lfo->setAmplitude(0.0f); // Start with LFO off

    // Initialize reverb with medium room preset
    g_reverb = std::make_unique<SchroederReverb>(static_cast<float>(SAMPLE_RATE));
    auto medium_room = SchroederReverb::createMediumRoomPreset();
    g_reverb->applyPreset(medium_room);
    g_reverb->setMix(0.5f); // Start with reverb off

    // Initialize chorus with default settings
    // g_chorus = std::make_unique<Chorus>(static_cast<float>(SAMPLE_RATE));
    // g_chorus->setMix(0.0f);      // Start with chorus off
    // g_chorus->setDepth(0.5f);    // Medium depth
    // g_chorus->setRate(0.5f);     // Slow rate
    // g_chorus->setFeedback(0.2f); // Light feedback

    // Start silent

    // Start audio output with callback
    if (sink.startWithCallback(audioCallback) != 0)
    {
        std::cout << "Failed to start audio output\n";
        return -1;
    }

    // Initialize MIDI reader only if devices are available
    bool midi_available = (num_midi_devices > 0);
    MidiInterface midi_reader;

    if (midi_available)
    {
        if (midi_reader.init() != 0)
        {
            std::cout << "Failed to initialize MIDI reader\n";
            return -1;
        }

        // Set MIDI callback
        midi_reader.setNoteCallback(midiNoteCallback);
        midi_reader.setControlCallback(midiControlCallBack);
        midi_reader.setChannelAftertouchCallback(midiAftertouchCallback);
        // midi_reader.setPolyphonicAftertouchCallback(midiPolyphonicAftertouchCallback);

        // Ask user which MIDI device to use
        int midi_device = 0;

        if (num_midi_devices == 1)
        {
            std::cout << "\nUsing the only available MIDI device: [0] "
                      << MidiInterface::getDeviceName(0) << "\n";
        }
        else
        {
            bool valid_selection = false;
            while (!valid_selection)
            {
                std::cout << "\nSelect MIDI input device [0-" << (num_midi_devices - 1) << "]: ";

                if (!(std::cin >> midi_device))
                {
                    // Clear error state and ignore bad input
                    std::cin.clear();
                    std::cin.ignore(10000, '\n');
                    std::cout << "Invalid input. Please enter a number.\n";
                    continue;
                }

                if (midi_device < 0 || midi_device >= num_midi_devices)
                {
                    std::cout << "Invalid device selection (" << midi_device << "). Please select a device between 0 and "
                              << (num_midi_devices - 1) << ".\n";
                }
                else
                {
                    std::cout << "Selected device [" << midi_device << "]: "
                              << MidiInterface::getDeviceName(midi_device) << "\n";
                    valid_selection = true;
                }
            }
        }

        // Start MIDI input
        if (midi_reader.open(midi_device) != 0)
        {
            std::cout << "Failed to start MIDI input\n";
            PlatformUtils::cleanup();
            return -1;
        }
    }

    // no device chosen: an input of its own all the same, for a software sequencer or a modular to play it
    if (!midi_available)
    {
        midi_reader.setNoteCallback(midiNoteCallback);
        midi_reader.setControlCallback(midiControlCallBack);
        midi_reader.setChannelAftertouchCallback(midiAftertouchCallback);
        if (midi_reader.openVirtual() == 0)
            midi_available = true;
    }

    std::cout << "🎹 Polyphonic MIDI Synthesizer Ready!\n";
    if (midi_available)
    {
        std::cout << "Play up to 8 simultaneous notes on your MIDI device.\n";
    }
    else
    {
        std::cout << "Running in audio-only mode (no MIDI input).\n";
        std::cout << "Use keyboard controls to test audio output and recording.\n";
    }
    std::cout << "Current wave type: Sine\n";
    std::cout << "Recording file: " << g_recording_filename << "\n";
    std::cout << "Active voices: " << g_oscillator->getActiveVoiceCount()
              << "/8 (Releasing: " << 0 << ")\n"; // TODO: Implement getReleasingVoiceCount()
    std::cout << "Effects: Reverb OFF, Chorus OFF\n";
    std::cout << "LFO: Not supported in polyphonic mode\n\n";

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
                std::cout << "Wave type changed to: Sine\n";
                break;
            case '2':
                g_oscillator->setWaveType(WaveType::SQUARE);
                std::cout << "Wave type changed to: Square\n";
                break;
            case '3':
                g_oscillator->setWaveType(WaveType::SAW);
                std::cout << "Wave type changed to: Sawtooth\n";
                break;
            case '4':
                g_oscillator->setWaveType(WaveType::TRIANGLE);
                std::cout << "Wave type changed to: Triangle\n";
                break;
            case '5':
                g_oscillator->setWaveType(WaveType::PULSE);
                std::cout << "Wave type changed to: Pulse\n";
                break;
            case '6':
                g_oscillator->setWaveType(WaveType::NOISE);
                std::cout << "Wave type changed to: Noise\n";
                break;
            case '7':
                g_oscillator->setWaveType(WaveType::RAMP_UP);
                std::cout << "Wave type changed to: Ramp Up\n";
                break;
            case '8':
                g_oscillator->setWaveType(WaveType::RAMP_DOWN);
                std::cout << "Wave type changed to: Ramp Down\n";
                break;
            case '9':
                g_oscillator->setWaveType(WaveType::FM_SYNTHESIS);
                std::cout << "Wave type changed to: FM Synthesis (Algorithm: ";
                switch (g_oscillator->getFMAlgorithm())
                {
                case FMAlgorithm::TWO_OP_SIMPLE:
                    std::cout << "2-Op Simple";
                    break;
                case FMAlgorithm::TWO_OP_FEEDBACK:
                    std::cout << "2-Op Feedback";
                    break;
                case FMAlgorithm::FOUR_OP_SERIAL:
                    std::cout << "4-Op Serial";
                    break;
                case FMAlgorithm::FOUR_OP_PARALLEL:
                    std::cout << "4-Op Parallel";
                    break;
                case FMAlgorithm::FOUR_OP_MIXED:
                    std::cout << "4-Op Mixed";
                    break;
                }
                std::cout << ")\n";
                break;
            case 'w':
            case 'W':
                selectPCMFile();
                break;
            case 'R':
                startRecording();
                break;
            case 's':
            case 'S':
                stopRecording();
                break;
            case 'v':
                // Toggle reverb on/off
                if (g_reverb->getMix() > 0.0f)
                {
                    g_reverb->setMix(0.0f);
                    std::cout << "Reverb: OFF\n";
                }
                else
                {
                    g_reverb->setMix(0.3f);
                    std::cout << "Reverb: ON (Mix: 30%, Room: " << (g_reverb->getRoomSize() * 100.0f)
                              << "%, Damping: " << (g_reverb->getDamping() * 100.0f) << "%)\n";
                }
                break;
            case 'V':
                // Cycle reverb presets
                cycleReverbPresets();
                break;
            case 'c':
                // Toggle chorus on/off
                if (g_chorus->getMix() > 0.0f)
                {
                    g_chorus->setMix(0.0f);
                    std::cout << "Chorus: OFF\n";
                }
                else
                {
                    g_chorus->setMix(0.5f);
                    std::cout << "Chorus: ON (Mix: " << (g_chorus->getMix() * 100.0f)
                              << "%, Depth: " << (g_chorus->getDepth() * 100.0f)
                              << "%, Rate: " << g_chorus->getRate() << " Hz)\n";
                }
                break;
            case 'C':
                cycleChorusPreset();
                break;
            case 'm':
            case 'M':
                printCCMappings();
                break;
            case 'l':
            case 'L':
                // LFO not supported in polyphonic mode
                std::cout << "LFO: Not supported in polyphonic mode\n";
                break;
            case '[':
                // Test bank position 75%
                if (g_oscillator && g_oscillator->isWavBank())
                {
                    g_oscillator->setBankPosition(0.75f);
                    std::cout << "Bank Position set to: 75%\n";
                }
                else
                {
                    std::cout << "No wav bank loaded\n";
                }
                break;
            case ']':
                // Test bank position 100%
                if (g_oscillator && g_oscillator->isWavBank())
                {
                    g_oscillator->setBankPosition(1.0f);
                    std::cout << "Bank Position set to: 100%\n";
                }
                else
                {
                    std::cout << "No wav bank loaded\n";
                }
                break;
            case '0':
                // Test bank position 0%
                if (g_oscillator && g_oscillator->isWavBank())
                {
                    g_oscillator->setBankPosition(0.0f);
                    std::cout << "Bank Position set to: 0%\n";
                }
                else
                {
                    std::cout << "No wav bank loaded\n";
                }
                break;
            case '(':
                // Test bank position 80%
                if (g_oscillator && g_oscillator->isWavBank())
                {
                    g_oscillator->setBankPosition(0.8f);
                    std::cout << "Bank Position set to: 80%\n";
                }
                else
                {
                    std::cout << "No wav bank loaded\n";
                }
                break;
            case 'F':
            case 'f':
                // Cycle FM algorithm
                if (g_oscillator)
                {
                    FMAlgorithm current = g_oscillator->getFMAlgorithm();
                    FMAlgorithm next;
                    const char *next_name;

                    switch (current)
                    {
                    case FMAlgorithm::TWO_OP_SIMPLE:
                        next = FMAlgorithm::TWO_OP_FEEDBACK;
                        next_name = "2-Op Feedback";
                        break;
                    case FMAlgorithm::TWO_OP_FEEDBACK:
                        next = FMAlgorithm::FOUR_OP_SERIAL;
                        next_name = "4-Op Serial";
                        break;
                    case FMAlgorithm::FOUR_OP_SERIAL:
                        next = FMAlgorithm::FOUR_OP_PARALLEL;
                        next_name = "4-Op Parallel";
                        break;
                    case FMAlgorithm::FOUR_OP_PARALLEL:
                        next = FMAlgorithm::FOUR_OP_MIXED;
                        next_name = "4-Op Mixed";
                        break;
                    case FMAlgorithm::FOUR_OP_MIXED:
                        next = FMAlgorithm::TWO_OP_SIMPLE;
                        next_name = "2-Op Simple";
                        break;
                    default:
                        next = FMAlgorithm::TWO_OP_SIMPLE;
                        next_name = "2-Op Simple";
                        break;
                    }

                    g_oscillator->setFMAlgorithm(next);
                    std::cout << "FM Algorithm: " << next_name << "\n";
                }
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
    g_reverb.reset();
    g_chorus.reset();

    PlatformUtils::cleanup();
    std::cout << "Polyphonic MIDI Synthesizer stopped.\n";

    return 0;
}
