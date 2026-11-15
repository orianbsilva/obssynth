/**
 * @file simple_midi_monitor.cpp
 * @brief Simple MIDI message monitor using MidiInterface
 *
 * This program connects to a MIDI device and prints all incoming
 * MIDI messages in a structured, readable format. Perfect for
 * discovering button mappings on the APC Key 25 Mk1.
 */

#include "../lib/utils/platform_midi_reader.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace synth;
using namespace examples;

static std::atomic<bool> running(true);

void signal_handler(int)
{
    running = false;
}

const char* getMidiEventTypeName(const MidiNote& note)
{
    return note.is_note_on ? "Note On" : "Note Off";
}

const char* getControlName(uint8_t note, bool is_note)
{
    if (is_note) // Note On/Off
    {
        // APC Key 25 common note mappings
        if (note >= 36 && note <= 43)
        {
            return "Pad";
        }
        else if (note >= 82 && note <= 86)
        {
            return "Scene Button";
        }
        else if (note >= 91 && note <= 98)
        {
            switch (note)
            {
                case 91: return "Play Button";
                case 92: return "Stop Button";
                case 93: return "Record Button";
                case 98: return "Shift Button";
                default: return "Transport Button";
            }
        }
        else if (note >= 48 && note <= 72)
        {
            return "Keyboard Key";
        }
    }
    else // Control Change
    {
        if (note >= 48 && note <= 55)
        {
            return "Fader";
        }
        else if (note >= 56 && note <= 63)
        {
            return "Encoder";
        }
    }

    return "Unknown Control";
}

void printMidiNote(const MidiNote& note)
{
    // Get current timestamp
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 100000;

    std::cout << std::left
              << std::setw(8) << timestamp
              << std::setw(12) << getMidiEventTypeName(note)
              << std::setw(4) << "Ch" << (note.channel + 1)
              << std::setw(8) << "Note" << static_cast<int>(note.note)
              << std::setw(10) << "Velocity" << static_cast<int>(note.velocity)
              << std::setw(20) << getControlName(note.note, true);

    // Add specific details
    if (note.note >= 36 && note.note <= 43)
    {
        std::cout << " #" << (note.note - 35);
    }
    else if (note.note >= 82 && note.note <= 86)
    {
        std::cout << " #" << (note.note - 81);
    }

    std::cout << std::endl;
}

void printControlChange(uint8_t channel, uint8_t controller, uint8_t value)
{
    // Get current timestamp
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count() % 100000;

    std::cout << std::left
              << std::setw(8) << timestamp
              << std::setw(12) << "Control Change"
              << std::setw(4) << "Ch" << (channel + 1)
              << std::setw(8) << "CC" << static_cast<int>(controller)
              << std::setw(10) << "Value" << static_cast<int>(value)
              << std::setw(20) << getControlName(controller, false);

    // Add specific details
    if (controller >= 48 && controller <= 55)
    {
        std::cout << " #" << (controller - 47);
    }
    else if (controller >= 56 && controller <= 63)
    {
        std::cout << " #" << (controller - 55);
    }

    std::cout << std::endl;
}

int main()
{
    signal(SIGINT, signal_handler);

    PlatformUtils::initialize();

    PlatformUtils::printHeader("Simple MIDI Message Monitor",
                             "Discover button mappings for your APC Key 25 Mk1");

    std::cout << "Connect your APC Key 25 and press controls to see MIDI messages.\n";
    std::cout << "Press Ctrl+C to exit.\n\n";

    // Initialize MIDI
    MidiInterface midiInterface;
    if (midiInterface.init() != 0)
    {
        std::cerr << "Failed to initialize MIDI interface" << std::endl;
        return 1;
    }

    // List available devices
    auto devices = midiInterface.getDeviceList();
    std::cout << "Available MIDI devices:\n";
    for (size_t i = 0; i < devices.size(); i++)
    {
        std::cout << "  " << i << ": " << devices[i] << std::endl;
    }
    std::cout << std::endl;

    // Find APC device
    int device_id = -1;
    for (size_t i = 0; i < devices.size(); i++)
    {
        const std::string& device_name = devices[i];
        if (device_name.find("APC") != std::string::npos ||
            device_name.find("Key 25") != std::string::npos)
        {
            device_id = static_cast<int>(i);
            std::cout << "Auto-selected APC device: " << device_name << " (ID: " << device_id << ")\n";
            break;
        }
    }

    if (device_id == -1)
    {
        std::cout << "APC device not found. Enter device ID manually: ";
        std::cin >> device_id;
        if (device_id < 0 || device_id >= static_cast<int>(devices.size()))
        {
            std::cerr << "Invalid device ID" << std::endl;
            return 1;
        }
    }

    // Connect to device
    if (midiInterface.open(device_id) != 0)
    {
        std::cerr << "Failed to connect to MIDI device" << std::endl;
        return 1;
    }

    std::cout << "Connected to: " << devices[device_id] << std::endl;
    std::cout << "\nMonitoring MIDI messages... Press controls on your APC Key 25!\n";
    std::cout << std::string(80, '=') << std::endl;

    // Print header
    std::cout << std::left
              << std::setw(8) << "Time"
              << std::setw(12) << "Type"
              << std::setw(7) << "Chan"
              << std::setw(8) << "Note/CC"
              << std::setw(10) << "Value"
              << std::setw(20) << "Control"
              << "Details" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    // Set up MIDI callbacks
    midiInterface.setNoteCallback([](const MidiNote& note) {
        printMidiNote(note);
    });

    midiInterface.setControlCallback([](uint8_t channel, uint8_t controller, uint8_t value) {
        printControlChange(channel, controller, value);
    });

    // Main monitoring loop
    while (running)
    {
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "\nMIDI monitoring stopped.\n";
    std::cout << "\n=== Discovery Summary ===\n";
    std::cout << "Use the note numbers you saw above to update the pad mappings in:\n";
    std::cout << "src/lib/devices/apc_key25_mk1.hpp\n";
    std::cout << "\nLook for the PAD_NOTES array and update with the correct values.\n";

    midiInterface.close();
    PlatformUtils::cleanup();
    return 0;
}
