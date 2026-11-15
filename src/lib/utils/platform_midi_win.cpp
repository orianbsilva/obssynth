/**
 * @file platform_midi_win.cpp
 * @brief Windows WinMM implementation of MidiInterface
 */
#ifdef _WIN32
#include "platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <vector>
namespace synth {
    MidiInterface::MidiInterface()
        : hMidiIn_(nullptr), hMidiOut_(nullptr), hEvent_(nullptr), isOpen_(false), isOutputOpen_(false), useKeyboardFallback_(false),
          keyboard_should_stop_(false), currentScrollController_(1)
    {
        initializeKeyMapping();
        lastScrollTime_ = std::chrono::steady_clock::now();
    }

    MidiInterface::~MidiInterface()
    {
        close();
        closeOutput();
        disableKeyboardFallback();
    }

    int MidiInterface::init()
    {
        hEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        return hEvent_ ? 0 : -1;
    }

    int MidiInterface::open(int deviceId)
    {
        if (isOpen_)
            return 0;

        // First try to open a real MIDI device
        bool midiDeviceOpened = false;

        UINT numDevices = midiInGetNumDevs();
        if (numDevices == 0 || deviceId >= static_cast<int>(numDevices))
        {
            std::cout << "No MIDI devices available (found " << numDevices << " devices)" << std::endl;
        }
        else
        {
            MMRESULT result = midiInOpen(&hMidiIn_, deviceId,
                                         reinterpret_cast<DWORD_PTR>(midiInputCallback),
                                         reinterpret_cast<DWORD_PTR>(this),
                                         CALLBACK_FUNCTION);

            if (result == MMSYSERR_NOERROR)
            {
                // Reset the device before starting
                midiInReset(hMidiIn_);

                result = midiInStart(hMidiIn_);
                if (result == MMSYSERR_NOERROR)
                {
                    midiDeviceOpened = true;
                    std::cout << "MIDI device opened successfully: " << getDeviceName(deviceId) << std::endl;

                    // Probe the device to ensure it's working
                    if (!probeMidiDevice())
                    {
                        std::cout << "Warning: MIDI device probe failed, but device appears open" << std::endl;
                    }
                }
                else
                {
                    std::cerr << "Failed to start MIDI input, error: " << result << std::endl;
                    midiInClose(hMidiIn_);
                    hMidiIn_ = nullptr;
                }
            }
            else
            {
                std::cerr << "Failed to open MIDI device " << deviceId << ", error: " << result << std::endl;
            }
        }


        // If no MIDI device could be opened, enable keyboard fallback
        if (!midiDeviceOpened)
        {
            std::cout << "Enabling keyboard fallback mode..." << std::endl;
            enableKeyboardFallback();
        }

        isOpen_ = true;
        return 0;
    }

    bool MidiInterface::testMidiConnection()
    {
        if (!isOpen_)
        {
            return false;
        }

        // If using keyboard fallback, consider it always connected
        if (useKeyboardFallback_)
        {
            return true;
        }

        // For Windows, check if the handle is still valid
        if (!hMidiIn_)
        {
            return false;
        }

        // Try to get device capabilities to test connection
        MIDIINCAPS caps;
        MMRESULT result = midiInGetDevCaps(reinterpret_cast<UINT_PTR>(hMidiIn_), &caps, sizeof(caps));
        return (result == MMSYSERR_NOERROR);

    }

    void MidiInterface::close()
    {
        if (!isOpen_)
            return;

        // Disable keyboard fallback if enabled
        disableKeyboardFallback();

        if (hMidiIn_)
        {
            midiInStop(hMidiIn_);
            midiInReset(hMidiIn_); // Reset the device to clear any pending messages
            midiInClose(hMidiIn_);
            hMidiIn_ = nullptr;
        }
        if (hEvent_)
        {
            CloseHandle(hEvent_);
            hEvent_ = nullptr;
        }


        isOpen_ = false;
    }

    void MidiInterface::drainMidiDevice()
    {
        // Windows uses midiInReset() which clears pending messages
    }

    bool MidiInterface::probeMidiDevice()
    {
        if (!hMidiIn_)
            return false;

        // For Windows, if the device opened successfully and started,
        // we consider it working. WinMM handles device validation internally.
        return true;

    }

    void MidiInterface::forceResetMidiDevice(int) {}
    void CALLBACK MidiInterface::midiInputCallback(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                                   DWORD_PTR dwParam1, DWORD_PTR dwParam2)
    {
        MidiInterface *reader = reinterpret_cast<MidiInterface *>(dwInstance);

        if (wMsg == MIM_DATA)
        {
            uint8_t status = static_cast<uint8_t>(dwParam1 & 0xFF);
            uint8_t data1 = static_cast<uint8_t>((dwParam1 >> 8) & 0xFF);
            uint8_t data2 = static_cast<uint8_t>((dwParam1 >> 16) & 0xFF);

            reader->parseMidiMessage(status, data1, data2);
        }
        else if (wMsg == MIM_LONGDATA)
        {
            // Handle SysEx messages
            MIDIHDR *midiHdr = reinterpret_cast<MIDIHDR *>(dwParam1);
            if (midiHdr && midiHdr->lpData && midiHdr->dwBytesRecorded > 0)
            {
                std::vector<uint8_t> sysexData(
                    reinterpret_cast<uint8_t *>(midiHdr->lpData),
                    reinterpret_cast<uint8_t *>(midiHdr->lpData) + midiHdr->dwBytesRecorded);

                if (reader->sysExCallback_)
                {
                    reader->sysExCallback_(sysexData);
                }
            }
        }
    }

    std::vector<std::string> MidiInterface::getDeviceList()
    {
        std::vector<std::string> devices;

        UINT numDevices = midiInGetNumDevs();
        for (UINT i = 0; i < numDevices; i++)
        {
            MIDIINCAPS caps;
            if (midiInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            {
                devices.push_back(std::string(caps.szPname));
            }
        }


        return devices;
    }

    int MidiInterface::openVirtual(const char *)
    {
        return -1; // WinMM has no virtual ports (loopMIDI makes one)
    }

    std::vector<MidiInterface::Source> MidiInterface::sources() const { return {}; } // MidiIn opens WinMM's devices
    bool MidiInterface::connectSource(int, int, bool) { return false; }

    int MidiInterface::getDeviceCount()
    {
        return static_cast<int>(midiInGetNumDevs());
    }

    std::string MidiInterface::getDeviceName(int deviceId)
    {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(deviceId, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
        {
            return std::string(caps.szPname);
        }
        return "Unknown Device";

    }

    int MidiInterface::openOutput(int deviceId)
    {
        if (isOutputOpen_)
        {
            return 0; // Already open
        }

        UINT numDevices = midiOutGetNumDevs();
        if (numDevices == 0 || deviceId >= static_cast<int>(numDevices))
        {
            std::cerr << "No MIDI output devices available (found " << numDevices << " devices)" << std::endl;
            return -1;
        }

        MMRESULT result = midiOutOpen(&hMidiOut_, deviceId, 0, 0, CALLBACK_NULL);
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to open MIDI output device " << deviceId << ": " << result << std::endl;
            return -1;
        }

        isOutputOpen_ = true;
        std::cout << "MIDI output device opened successfully: device " << deviceId << std::endl;
        return 0;

    }

    void MidiInterface::closeOutput()
    {
        if (!isOutputOpen_)
        {
            return;
        }

        if (hMidiOut_)
        {
            midiOutClose(hMidiOut_);
            hMidiOut_ = nullptr;
        }

        isOutputOpen_ = false;
        std::cout << "MIDI output device closed" << std::endl;
    }

    int MidiInterface::sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        note = std::min(note, static_cast<uint8_t>(127));
        velocity = std::min(velocity, static_cast<uint8_t>(127));

        // Create MIDI message: Note On (0x90 + channel), note, velocity
        uint32_t message = (0x90 | channel) | (note << 8) | (velocity << 16);

        MMRESULT result = midiOutShortMsg(hMidiOut_, message);
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to send MIDI Note On: " << result << std::endl;
            return -1;
        }
        return 0;

    }

    int MidiInterface::sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        note = std::min(note, static_cast<uint8_t>(127));
        velocity = std::min(velocity, static_cast<uint8_t>(127));

        // Create MIDI message: Note Off (0x80 + channel), note, velocity
        uint32_t message = (0x80 | channel) | (note << 8) | (velocity << 16);

        MMRESULT result = midiOutShortMsg(hMidiOut_, message);
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to send MIDI Note Off: " << result << std::endl;
            return -1;
        }
        return 0;

    }

    int MidiInterface::sendControlChange(uint8_t channel, uint8_t controller, uint8_t value)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        controller = std::min(controller, static_cast<uint8_t>(127));
        value = std::min(value, static_cast<uint8_t>(127));

        // Create MIDI message: Control Change (0xB0 + channel), controller, value
        uint32_t message = (0xB0 | channel) | (controller << 8) | (value << 16);

        MMRESULT result = midiOutShortMsg(hMidiOut_, message);
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to send MIDI Control Change: " << result << std::endl;
            return -1;
        }
        return 0;

    }

    int MidiInterface::sendProgramChange(uint8_t channel, uint8_t program)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        // Clamp values to valid MIDI ranges
        channel = std::min(channel, static_cast<uint8_t>(15));
        program = std::min(program, static_cast<uint8_t>(127));

        // Create MIDI message: Program Change (0xC0 + channel), program
        uint32_t message = (0xC0 | channel) | (program << 8);

        MMRESULT result = midiOutShortMsg(hMidiOut_, message);
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to send MIDI Program Change: " << result << std::endl;
            return -1;
        }
        return 0;

    }

    int MidiInterface::sendSysEx(const std::vector<uint8_t> &data)
    {
        if (!isOutputOpen_)
        {
            std::cerr << "MIDI output device not open" << std::endl;
            return -1;
        }

        if (data.empty())
        {
            std::cerr << "Empty SysEx data" << std::endl;
            return -1;
        }

        // Validate SysEx format (should start with 0xF0 and end with 0xF7)
        if (data[0] != 0xF0)
        {
            std::cerr << "Invalid SysEx message: must start with 0xF0" << std::endl;
            return -1;
        }

        if (data.back() != 0xF7)
        {
            std::cerr << "Invalid SysEx message: must end with 0xF7" << std::endl;
            return -1;
        }

        // For Windows, we need to use midiOutLongMsg for SysEx
        MIDIHDR midiHdr;
        memset(&midiHdr, 0, sizeof(MIDIHDR));

        midiHdr.lpData = reinterpret_cast<LPSTR>(const_cast<uint8_t *>(data.data()));
        midiHdr.dwBufferLength = static_cast<DWORD>(data.size());
        midiHdr.dwBytesRecorded = static_cast<DWORD>(data.size());

        // Prepare the header
        MMRESULT result = midiOutPrepareHeader(hMidiOut_, &midiHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to prepare SysEx header: " << result << std::endl;
            return -1;
        }

        // Send the SysEx message
        result = midiOutLongMsg(hMidiOut_, &midiHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to send SysEx message: " << result << std::endl;
            midiOutUnprepareHeader(hMidiOut_, &midiHdr, sizeof(MIDIHDR));
            return -1;
        }

        // Wait for the message to be sent (poll the WHDR_DONE flag)
        while (!(midiHdr.dwFlags & WHDR_DONE))
        {
            Sleep(1); // Brief sleep to avoid busy waiting
        }

        // Unprepare the header
        result = midiOutUnprepareHeader(hMidiOut_, &midiHdr, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
        {
            std::cerr << "Failed to unprepare SysEx header: " << result << std::endl;
            // Not a fatal error, continue
        }

        return 0;

    }

} // namespace synth
#endif
