#include "../lib/utils/platform_midi_reader.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include "../lib/audio/audio_sink.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace synth;
using namespace examples;

class KeyboardMidiDemo
{
private:
    MidiInterface midiReader_;
    Oscillator oscillator_;
    AudioSink audioSink_;
    std::atomic<bool> running_;
    std::atomic<float> currentFrequency_;
    std::atomic<float> currentAmplitude_;

public:
    KeyboardMidiDemo()
        : oscillator_(440.0f, WaveType::SINE, 48000.0f), // Initialize with default values
          running_(false), currentFrequency_(0.0f), currentAmplitude_(0.0f)
    {

        // Initialize audio
        audioSink_.init();

        // Set up MIDI callbacks
        midiReader_.setNoteCallback([this](const MidiNote &note)
                                    { handleNoteCallback(note); });

        midiReader_.setControlCallback([this](uint8_t channel, uint8_t controller, uint8_t value)
                                       { handleControlCallback(channel, controller, value); });
    }

    void handleNoteCallback(const MidiNote &note)
    {
        float frequency = MidiInterface::midiNoteToFrequency(note.note);

        if (note.is_note_on && note.velocity > 0)
        {
            std::cout << "Note ON:  " << static_cast<int>(note.note)
                      << " (freq: " << frequency << " Hz, vel: "
                      << static_cast<int>(note.velocity) << ")" << std::endl;

            currentFrequency_.store(frequency);
            currentAmplitude_.store(note.velocity / 127.0f * 0.3f); // Scale to reasonable amplitude
            oscillator_.setFrequency(frequency);
        }
        else
        {
            std::cout << "Note OFF: " << static_cast<int>(note.note)
                      << " (freq: " << frequency << " Hz)" << std::endl;

            // Only stop if this was the current note
            if (std::abs(currentFrequency_.load() - frequency) < 1.0f)
            {
                currentAmplitude_.store(0.0f);
            }
        }
    }

    void handleControlCallback(uint8_t channel, uint8_t controller, uint8_t value)
    {
        std::cout << "Control Change: Channel=" << static_cast<int>(channel)
                  << " Controller=" << static_cast<int>(controller)
                  << " Value=" << static_cast<int>(value) << std::endl;

        // Example: Use controller 1 (mod wheel) to control oscillator type
        if (controller == 1)
        {
            if (value < 43)
            {
                oscillator_.setWaveType(WaveType::SINE);
                std::cout << "-> Switched to SINE wave" << std::endl;
            }
            else if (value < 85)
            {
                oscillator_.setWaveType(WaveType::SQUARE);
                std::cout << "-> Switched to SQUARE wave" << std::endl;
            }
            else
            {
                oscillator_.setWaveType(WaveType::SAW);
                std::cout << "-> Switched to SAW wave" << std::endl;
            }
        }
    }

    void audioCallback(float *buffer, int frames, int *channels)
    {
        int channelCount = *channels;
        float amplitude = currentAmplitude_.load();

        for (int i = 0; i < frames; i++)
        {
            float sample = 0.0f;
            if (amplitude > 0.0f)
            {
                sample = oscillator_.getSample() * amplitude;
            }

            // Fill all channels with the same sample
            for (int ch = 0; ch < channelCount; ch++)
            {
                buffer[i * channelCount + ch] = sample;
            }
        }
    }

    void run()
    {
        PlatformUtils::printHeader("Keyboard MIDI Fallback Demo",
                                   "Demonstrates keyboard fallback when no MIDI device is available");

        const char *controls[] = {
            "White keys: a s d f (C3 D3 E3 F3)",
            "Black keys: q w e r t (C#3 D#3 F#3 G#3 A#3)",
            "Controllers: 0-9 keys (select controller) + +/- keys (adjust value)",
            "Press 'x' to exit keyboard mode",
            "Press 'q' to quit demo",
            nullptr};
        PlatformUtils::printControls(controls);

        // Initialize MIDI reader
        if (midiReader_.init() != 0)
        {
            std::cerr << "Failed to initialize MIDI reader" << std::endl;
            return;
        }

        // Try to open MIDI device (will fallback to keyboard if no device)
        midiReader_.open(0);

        // Set up audio callback
        audioSink_.setAudioCallback([this](float *buffer, int frames, int *channels)
                                    { audioCallback(buffer, frames, *channels); });

        // Start audio
        if (audioSink_.start() != 0)
        {
            std::cerr << "Failed to start audio" << std::endl;
            return;
        }

        running_.store(true);
        std::cout << "\nDemo running! ";

        if (midiReader_.isKeyboardFallbackEnabled())
        {
            std::cout << "Using keyboard fallback mode." << std::endl;
        }
        else
        {
            std::cout << "Using real MIDI device." << std::endl;
        }

        // Main loop
        while (running_.load())
        {
            if (PlatformUtils::kbhit())
            {
                char c = static_cast<char>(PlatformUtils::getch());
                if (c == 'q' || c == 'Q')
                {
                    running_.store(false);
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Cleanup
        audioSink_.stop();
        midiReader_.close();

        std::cout << "Demo finished!" << std::endl;
    }
};

int main()
{
    PlatformUtils::initialize();

    try
    {
        KeyboardMidiDemo demo;
        demo.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        PlatformUtils::cleanup();
        return 1;
    }

    PlatformUtils::cleanup();
    return 0;
}
