/**
 * @file platform_midi_reader.hpp
 * @brief Cross-platform MIDI input reader with keyboard fallback
 *
 * Provides MIDI input functionality for both Windows and Linux platforms:
 * - Windows: Uses WinMM MIDI input API
 * - Linux: Uses ALSA raw MIDI and sequencer APIs
 *
 * Features keyboard fallback mode for systems without MIDI devices,
 * comprehensive callback system for different MIDI events, and
 * device enumeration and management.
 */

#pragma once

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#elif defined(__linux__)
#include <alsa/asoundlib.h>
#include <poll.h>
#include <termios.h>
#include <fcntl.h>
#endif

#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <chrono>

namespace synth
{

    // MIDI message types
    constexpr uint8_t MIDI_NOTE_OFF = 0x80;
    constexpr uint8_t MIDI_NOTE_ON = 0x90;
    constexpr uint8_t MIDI_POLYPHONIC_AFTERTOUCH = 0xA0;
    constexpr uint8_t MIDI_CONTROL = 0xB0;
    constexpr uint8_t MIDI_PROGRAM = 0xC0;
    constexpr uint8_t MIDI_CHANNEL_AFTERTOUCH = 0xD0;
    constexpr uint8_t MIDI_PITCH_BEND = 0xE0;

    // MIDI note structure
    struct MidiNote
    {
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
        bool is_note_on;

        MidiNote() : channel(0), note(0), velocity(0), is_note_on(false) {}
    };

    // MIDI callback function types
    using MidiNoteCallback = std::function<void(const MidiNote &)>;
    using MidiPolyphonicAftertouchCallback = std::function<void(uint8_t channel, uint8_t note, uint8_t pressure)>;
    using MidiControlCallback = std::function<void(uint8_t channel, uint8_t controller, uint8_t value)>;
    using MidiProgramCallback = std::function<void(uint8_t channel, uint8_t program)>;
    using MidiChannelAftertouchCallback = std::function<void(uint8_t channel, uint8_t pressure)>;
    using MidiPitchBendCallback = std::function<void(uint8_t channel, int value)>;
    using MidiSysExCallback = std::function<void(const std::vector<uint8_t> &data)>;

    /**
     * @brief Cross-platform MIDI interface with input/output capabilities
     *
     * Handles bidirectional MIDI communication with hardware devices on Windows (WinMM) and Linux (ALSA).
     * Supports both raw MIDI and ALSA sequencer interfaces on Linux.
     * Provides keyboard fallback mode for systems without MIDI hardware.
     * Implements comprehensive callback system for all MIDI event types.
     * Supports MIDI output for controlling devices like LED controllers and synthesizers.
     */
    class MidiInterface
    {
    private:
#ifdef _WIN32
        HMIDIIN hMidiIn_;
        HMIDIOUT hMidiOut_;
        HANDLE hEvent_;
#elif defined(__linux__)
        snd_rawmidi_t *midi_in_;
        snd_rawmidi_t *midi_out_;
        snd_seq_t *seq_handle_; // For ALSA sequencer alternative
        int seq_port_;          // Sequencer port
        int seq_dest_client_;   // Destination client ID for output
        int seq_dest_port_;     // Destination port ID for output
        bool use_sequencer_;    // Flag to use sequencer instead of raw MIDI
        std::thread midi_thread_;
        std::atomic<bool> should_stop_;
#endif
        bool isOpen_;
        bool isOutputOpen_;
        bool useKeyboardFallback_;
        std::thread keyboard_thread_;
        std::atomic<bool> keyboard_should_stop_;
        std::unordered_map<char, uint8_t> keyToNote_;
        std::unordered_map<char, bool> keyPressed_;
        std::chrono::steady_clock::time_point lastScrollTime_;
        int currentScrollController_;
        MidiNote currentNote_;
        MidiNoteCallback noteCallback_;
        MidiPolyphonicAftertouchCallback polyphonicAftertouchCallback_;
        MidiControlCallback controlCallback_;
        MidiProgramCallback programCallback_;
        MidiChannelAftertouchCallback channelAftertouchCallback_;
        MidiPitchBendCallback pitchBendCallback_;
        MidiSysExCallback sysExCallback_;
        int eventSource_ = -1; // the ALSA sequencer client of the event being handled

#ifdef _WIN32
        // Static callback function for MIDI input (Windows)
        static void CALLBACK midiInputCallback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                               DWORD_PTR dwParam1, DWORD_PTR dwParam2);
#elif defined(__linux__)
        // MIDI thread function for Linux
        void midiThreadFunction();
#endif

        // Keyboard fallback functions
        void initializeKeyMapping();
        void keyboardThreadFunction();
        bool kbhit();
        int getch();
        void processKeyboardInput(char key);
        void handleControlChange(int controller, int value);

        // MIDI device management functions
        void drainMidiDevice();
        bool probeMidiDevice();
        void forceResetMidiDevice(int deviceId);
        bool trySequencerOpen(int deviceId); // Try ALSA sequencer interface

        // Parse MIDI message
        void parseMidiMessage(uint8_t status, uint8_t data1, uint8_t data2);

    public:
        // inside a callback: the sequencer client the event came from (aconnect -l numbers it), -1 when unknown
        int eventSource() const { return eventSource_; }

        // a port another program plays from (a controller, a sequencer), and whether it feeds our own (openVirtual)
        struct Source
        {
            int client = -1, port = -1;
            std::string name; // the client's, and the port's when it has several
            bool connected = false;
        };
        // Linux: the sequencer's readable ports, but ours and the system's; Windows: none (MidiIn opens devices)
        std::vector<Source> sources() const;
        // join a source to our port, or part them (as aconnect)
        bool connectSource(int client, int port, bool on);
        /**
         * @brief Construct a new MidiInterface object
         */
        MidiInterface();

        /**
         * @brief Destroy the MidiInterface object and clean up resources
         */
        ~MidiInterface();

        /**
         * @brief Copy constructor (deleted for safety)
         */
        MidiInterface(const MidiInterface &) = delete;

        /**
         * @brief Copy assignment operator (deleted for safety)
         */
        MidiInterface &operator=(const MidiInterface &) = delete;

        /**
         * @brief Initialize the MIDI input system
         * @return 0 on success, negative error code on failure
         */
        int init();

        /**
         * @brief Open a MIDI input device
         * @param deviceId Device ID to open (default: 0)
         * @return 0 on success, negative error code on failure
         */
        int open(int deviceId = 0);
        // An input of its own that other programs connect to (Linux: the ALSA sequencer port
        // <product>:input, subscribing to nothing): a software sequencer, a modular, aconnect.
        int openVirtual(const char *clientName = "ObsSynth4D"); // a port of our own, named for the product: <clientName>:input

        /**
         * @brief Force reconnection to MIDI device
         * @param deviceId Device ID to reconnect to (default: 0)
         * @return 0 on success, negative error code on failure
         */
        int reconnect(int deviceId = 0);

        /**
         * @brief Enable keyboard fallback mode for systems without MIDI devices
         */
        void enableKeyboardFallback();

        /**
         * @brief Disable keyboard fallback mode
         */
        void disableKeyboardFallback();

        /**
         * @brief Check if keyboard fallback mode is enabled
         * @return true if keyboard fallback is enabled, false otherwise
         */
        bool isKeyboardFallbackEnabled() const;

        /**
         * @brief Close the MIDI device and stop input
         */
        void close();

        /**
         * @brief Check if a MIDI device is currently open
         * @return true if device is open, false otherwise
         */
        bool isDeviceOpen() const;

        /**
         * @brief Test if MIDI device is still responding
         * @return true if device is responding, false if stale connection
         */
        bool testMidiConnection();

        /**
         * @brief Set the note on/off callback function
         * @param callback Function to call when note events are received
         */
        void setNoteCallback(MidiNoteCallback callback);

        /**
         * @brief Set the polyphonic aftertouch callback function
         * @param callback Function to call when polyphonic aftertouch events are received
         */
        void setPolyphonicAftertouchCallback(MidiPolyphonicAftertouchCallback callback);

        /**
         * @brief Set the control change callback function
         * @param callback Function to call when control change events are received
         */
        void setControlCallback(MidiControlCallback callback);

        /**
         * @brief Set the program change callback function
         * @param callback Function to call when program change events are received
         */
        void setProgramCallback(MidiProgramCallback callback);

        /**
         * @brief Set the channel aftertouch callback function
         * @param callback Function to call when channel aftertouch events are received
         */
        void setChannelAftertouchCallback(MidiChannelAftertouchCallback callback);

        /**
         * @brief Set the pitch bend callback function
         * @param callback Function to call when pitch bend events are received
         */
        void setPitchBendCallback(MidiPitchBendCallback callback);

        /**
         * @brief Set the System Exclusive (SysEx) callback function
         * @param callback Function to call when SysEx messages are received
         */
        void setSysExCallback(MidiSysExCallback callback);

        /**
         * @brief Open MIDI output device for sending messages
         * @param deviceId Device ID to open for output (default: 0)
         * @return 0 on success, negative error code on failure
         */
        int openOutput(int deviceId = 0);

        /**
         * @brief Close MIDI output device
         */
        void closeOutput();

        /**
         * @brief Check if MIDI output device is open
         * @return true if output device is open, false otherwise
         */
        bool isOutputOpen() const;

        /**
         * @brief Send a MIDI Note On message
         * @param channel MIDI channel (0-15)
         * @param note MIDI note number (0-127)
         * @param velocity Note velocity (0-127)
         * @return 0 on success, negative error code on failure
         */
        int sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

        /**
         * @brief Send a MIDI Note Off message
         * @param channel MIDI channel (0-15)
         * @param note MIDI note number (0-127)
         * @param velocity Note velocity (0-127)
         * @return 0 on success, negative error code on failure
         */
        int sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);

        /**
         * @brief Send a MIDI Control Change message
         * @param channel MIDI channel (0-15)
         * @param controller Controller number (0-127)
         * @param value Controller value (0-127)
         * @return 0 on success, negative error code on failure
         */
        int sendControlChange(uint8_t channel, uint8_t controller, uint8_t value);

        /**
         * @brief Send a MIDI Program Change message
         * @param channel MIDI channel (0-15)
         * @param program Program number (0-127)
         * @return 0 on success, negative error code on failure
         */
        int sendProgramChange(uint8_t channel, uint8_t program);

        /**
         * @brief Send a MIDI System Exclusive (SysEx) message
         * @param data Complete SysEx message including F0 start and F7 end bytes
         * @return 0 on success, negative error code on failure
         */
        int sendSysEx(const std::vector<uint8_t> &data);

        /**
         * @brief Get list of available MIDI input device names
         * @return Vector of device name strings
         */
        static std::vector<std::string> getDeviceList();

        /**
         * @brief Get the number of available MIDI input devices
         * @return Number of available devices
         */
        static int getDeviceCount();

        /**
         * @brief Get the name of a specific MIDI input device
         * @param deviceId Device ID to query
         * @return Device name string
         */
        static std::string getDeviceName(int deviceId);

        /**
         * @brief Convert MIDI note number to frequency in Hz
         * @param note MIDI note number (0-127)
         * @return Frequency in Hz
         */
        static float midiNoteToFrequency(uint8_t note);

        /**
         * @brief Map MIDI control value to a specified range
         * @param value MIDI control value (0-127)
         * @param min_val Minimum output value
         * @param max_val Maximum output value
         * @return Mapped value in the specified range
         */
        static float mapControlValue(uint8_t value, float min_val, float max_val);

    private:
        // Static lookup table for MIDI note to frequency conversion
        static const float noteFrequencyTable_[128];
    };

} // namespace synth
