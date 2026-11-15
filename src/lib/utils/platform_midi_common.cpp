/**
 * @file platform_midi_common.cpp
 * @brief Shared MIDI parsing, keyboard fallback, callbacks
 */
#include "platform_midi_reader.hpp"
#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <cctype>
#include <algorithm>
#ifdef _WIN32
#include <conio.h>
#elif defined(__linux__)
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <cstdio>
#endif
namespace synth {
    const float MidiInterface::noteFrequencyTable_[128] = {
        // MIDI notes 0-127, frequencies calculated using A4=440Hz
        8.175799f, 8.661957f, 9.177024f, 9.722718f, 10.300861f, 10.913383f, 11.562326f, 12.249857f,
        12.978272f, 13.750000f, 14.567618f, 15.433853f, 16.351598f, 17.323914f, 18.354048f, 19.445436f,
        20.601722f, 21.826765f, 23.124651f, 24.499715f, 25.956544f, 27.500000f, 29.135235f, 30.867706f,
        32.703196f, 34.647829f, 36.708096f, 38.890873f, 41.203445f, 43.653530f, 46.249303f, 48.999429f,
        51.913087f, 55.000000f, 58.270470f, 61.735413f, 65.406391f, 69.295658f, 73.416192f, 77.781746f,
        82.406889f, 87.307058f, 92.498606f, 97.998859f, 103.826174f, 110.000000f, 116.540940f, 123.470825f,
        130.812783f, 138.591315f, 146.832384f, 155.563492f, 164.813778f, 174.614116f, 184.997211f, 195.997718f,
        207.652349f, 220.000000f, 233.081881f, 246.941651f, 261.625565f, 277.182631f, 293.664768f, 311.126984f,
        329.627557f, 349.228231f, 369.994423f, 391.995436f, 415.304698f, 440.000000f, 466.163762f, 493.883301f,
        523.251131f, 554.365262f, 587.329536f, 622.253967f, 659.255114f, 698.456463f, 739.988845f, 783.990872f,
        830.609395f, 880.000000f, 932.327523f, 987.766603f, 1046.502261f, 1108.730524f, 1174.659072f, 1244.507935f,
        1318.510228f, 1396.912926f, 1479.977691f, 1567.981744f, 1661.218790f, 1760.000000f, 1864.655046f, 1975.533205f,
        2093.004522f, 2217.461048f, 2349.318143f, 2489.015870f, 2637.020455f, 2793.825851f, 2959.955382f, 3135.963488f,
        3322.437581f, 3520.000000f, 3729.310092f, 3951.066410f, 4186.009045f, 4434.922096f, 4698.636287f, 4978.031740f,
        5274.040911f, 5587.651703f, 5919.910763f, 6271.926976f, 6644.875161f, 7040.000000f, 7458.620184f, 7902.132820f,
        8372.018090f, 8869.844191f, 9397.272573f, 9956.063479f, 10548.081821f, 11175.303406f, 11839.821527f, 12543.853951f};

    int MidiInterface::reconnect(int deviceId)
    {
        std::cout << "Forcing MIDI device reconnection..." << std::endl;

        // Close existing connection
        close();

        // Small delay to ensure clean disconnection
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Reopen the device
        return open(deviceId);
    }

    void MidiInterface::parseMidiMessage(uint8_t status, uint8_t data1, uint8_t data2)
    {
        uint8_t messageType = status & 0xF0;
        uint8_t channel = status & 0x0F;

        switch (messageType)
        {
        case MIDI_NOTE_ON:
            if (data2 > 0) // Velocity > 0 means note on
            {
                currentNote_.channel = channel;
                currentNote_.note = data1;
                currentNote_.velocity = data2;
                currentNote_.is_note_on = true;
                if (noteCallback_)
                    noteCallback_(currentNote_);
            }
            else // Velocity = 0 means note off
            {
                currentNote_.channel = channel;
                currentNote_.note = data1;
                currentNote_.velocity = 0;
                currentNote_.is_note_on = false;
                if (noteCallback_)
                    noteCallback_(currentNote_);
            }
            break;

        case MIDI_NOTE_OFF:
            currentNote_.channel = channel;
            currentNote_.note = data1;
            currentNote_.velocity = data2;
            currentNote_.is_note_on = false;
            if (noteCallback_)
                noteCallback_(currentNote_);
            break;

        case MIDI_POLYPHONIC_AFTERTOUCH:
            if (polyphonicAftertouchCallback_)
                polyphonicAftertouchCallback_(channel, data1, data2); // data1 = note, data2 = pressure
            break;

        case MIDI_CONTROL:
            if (controlCallback_)
                controlCallback_(channel, data1, data2);
            break;

        case MIDI_PROGRAM:
            if (programCallback_)
                programCallback_(channel, data1);
            break;

        case MIDI_CHANNEL_AFTERTOUCH:
            if (channelAftertouchCallback_)
                channelAftertouchCallback_(channel, data1);
            break;

        case MIDI_PITCH_BEND:
            if (pitchBendCallback_)
            {
                int bendValue = (data2 << 7) | data1;
                pitchBendCallback_(channel, bendValue - 8192); // Center at 0
            }
            break;
        }
    }

    void MidiInterface::initializeKeyMapping()
    {
        // White keys: asdf (C3, D3, E3, F3)
        keyToNote_['a'] = 48; // C3
        keyToNote_['s'] = 50; // D3
        keyToNote_['d'] = 52; // E3
        keyToNote_['f'] = 54; // F3

        // Black keys: qwert (C#3, D#3, F#3, G#3, A#3)
        keyToNote_['q'] = 49; // C#3
        keyToNote_['w'] = 51; // D#3
        keyToNote_['e'] = 55; // F#3
        keyToNote_['r'] = 57; // G#3
        keyToNote_['t'] = 59; // A#3

        // Initialize all keys as not pressed
        for (const auto &pair : keyToNote_)
        {
            keyPressed_[pair.first] = false;
        }
    }

    void MidiInterface::enableKeyboardFallback()
    {
        if (useKeyboardFallback_)
            return;

        useKeyboardFallback_ = true;
        keyboard_should_stop_.store(false);
        keyboard_thread_ = std::thread(&MidiInterface::keyboardThreadFunction, this);

        std::cout << "Keyboard fallback enabled!" << std::endl;
        std::cout << "White keys: a s d f (C3 D3 E3 F3)" << std::endl;
        std::cout << "Black keys: q w e r t (C#3 D#3 F#3 G#3 A#3)" << std::endl;
        std::cout << "Control changes: 0-9 keys + mouse scroll (or PageUp/PageDown)" << std::endl;
        std::cout << "Press 'x' to exit keyboard mode" << std::endl;
    }

    void MidiInterface::disableKeyboardFallback()
    {
        if (!useKeyboardFallback_)
            return;

        useKeyboardFallback_ = false;
        keyboard_should_stop_.store(true);

        if (keyboard_thread_.joinable())
        {
            keyboard_thread_.join();
        }

        // Send note off messages for any pressed keys
        for (auto &pair : keyPressed_)
        {
            if (pair.second)
            {
                uint8_t note = keyToNote_[pair.first];
                currentNote_.channel = 0;
                currentNote_.note = note;
                currentNote_.velocity = 0;
                currentNote_.is_note_on = false;
                if (noteCallback_)
                {
                    noteCallback_(currentNote_);
                }
                pair.second = false;
            }
        }
    }

    bool MidiInterface::kbhit()
    {
#ifdef _WIN32
        return _kbhit() != 0;
#elif defined(__linux__)
        struct termios oldt, newt;
        int ch;
        int oldf;

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);

        if (ch != EOF)
        {
            ungetc(ch, stdin);
            return true;
        }
        return false;
#else
        return false;
#endif
    }

    int MidiInterface::getch()
    {
#ifdef _WIN32
        return _getch();
#elif defined(__linux__)
        struct termios oldt, newt;
        int ch;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
#else
        return 0;
#endif
    }

    void MidiInterface::keyboardThreadFunction()
    {
        while (!keyboard_should_stop_.load())
        {
            if (kbhit())
            {
                char key = static_cast<char>(getch());
                processKeyboardInput(key);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void MidiInterface::processKeyboardInput(char key)
    {
        // Convert to lowercase for consistency
        key = std::tolower(key);

        // Exit command
        if (key == 'x')
        {
            disableKeyboardFallback();
            return;
        }

        // Number keys for control changes (0-9)
        if (key >= '0' && key <= '9')
        {
            int controller = key - '0';
            currentScrollController_ = controller;
            if (controlCallback_)
            {
                controlCallback_(0, controller, 64); // Default middle value
            }
            return;
        }

        // Special keys for scroll simulation
        if (key == '+' || key == '=')
        { // PageUp simulation
            handleControlChange(currentScrollController_, 127);
            return;
        }
        if (key == '-' || key == '_')
        { // PageDown simulation
            handleControlChange(currentScrollController_, 0);
            return;
        }

        // MIDI note keys
        auto noteIt = keyToNote_.find(key);
        if (noteIt != keyToNote_.end())
        {
            uint8_t note = noteIt->second;
            bool &wasPressed = keyPressed_[key];

            if (!wasPressed)
            {
                // Note on
                wasPressed = true;
                currentNote_.channel = 0;
                currentNote_.note = note;
                currentNote_.velocity = 100; // Fixed velocity
                currentNote_.is_note_on = true;
                if (noteCallback_)
                {
                    noteCallback_(currentNote_);
                }
            }
            // Note: In real keyboard usage, we can't easily detect key release,
            // so we'll send note off after a short delay in a real implementation
            // For now, user needs to press the key again or press 'x' to stop all notes
        }
    }

    void MidiInterface::handleControlChange(int controller, int value)
    {
        if (controlCallback_)
        {
            controlCallback_(0, controller, value);
        }
    }

    float MidiInterface::midiNoteToFrequency(uint8_t note)
    {
        // Use lookup table for O(1) performance
        if (note < 128)
        {
            return noteFrequencyTable_[note];
        }

        // Fallback for invalid note values (should never happen with proper MIDI)
        return 440.0f * powf(2.0f, (note - 69) / 12.0f);
    }

    float MidiInterface::mapControlValue(uint8_t value, float min_val, float max_val)
    {
        // Clamp value to valid MIDI range
        float normalized = std::min(127.0f, std::max(0.0f, static_cast<float>(value))) / 127.0f;
        return min_val + normalized * (max_val - min_val);
    }

    bool MidiInterface::isKeyboardFallbackEnabled() const
    {
        return useKeyboardFallback_;
    }

    bool MidiInterface::isDeviceOpen() const
    {
        return isOpen_;
    }

    void MidiInterface::setNoteCallback(MidiNoteCallback callback)
    {
        noteCallback_ = callback;
    }

    void MidiInterface::setPolyphonicAftertouchCallback(MidiPolyphonicAftertouchCallback callback)
    {
        polyphonicAftertouchCallback_ = callback;
    }

    void MidiInterface::setControlCallback(MidiControlCallback callback)
    {
        controlCallback_ = callback;
    }

    void MidiInterface::setProgramCallback(MidiProgramCallback callback)
    {
        programCallback_ = callback;
    }

    void MidiInterface::setChannelAftertouchCallback(MidiChannelAftertouchCallback callback)
    {
        channelAftertouchCallback_ = callback;
    }

    void MidiInterface::setPitchBendCallback(MidiPitchBendCallback callback)
    {
        pitchBendCallback_ = callback;
    }

    void MidiInterface::setSysExCallback(MidiSysExCallback callback)
    {
        sysExCallback_ = callback;
    }

    bool MidiInterface::isOutputOpen() const
    {
        return isOutputOpen_;
    }

} // namespace synth
