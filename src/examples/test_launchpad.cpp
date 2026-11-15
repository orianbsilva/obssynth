// Test program specifically for Launchpad X (device 1)
#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace synth;

int main() {
    std::cout << "Launchpad X MIDI Test" << std::endl;
    std::cout << "====================" << std::endl;

    MidiInterface midi;

    // Initialize MIDI
    if (midi.init() != 0) {
        std::cerr << "Failed to initialize MIDI" << std::endl;
        return 1;
    }

    // List available devices
    auto devices = MidiInterface::getDeviceList();
    std::cout << "Available MIDI devices:" << std::endl;
    for (size_t i = 0; i < devices.size(); i++) {
        std::cout << "  " << i << ": " << devices[i] << std::endl;
    }

    // Set up callbacks
    midi.setNoteCallback([](const MidiNote& note) {
        std::cout << "MIDI Note: Channel=" << static_cast<int>(note.channel)
                  << " Note=" << static_cast<int>(note.note)
                  << " Velocity=" << static_cast<int>(note.velocity)
                  << " " << (note.is_note_on ? "ON" : "OFF") << std::endl;
    });

    midi.setControlCallback([](uint8_t channel, uint8_t controller, uint8_t value) {
        std::cout << "MIDI Control: Channel=" << static_cast<int>(channel)
                  << " Controller=" << static_cast<int>(controller)
                  << " Value=" << static_cast<int>(value) << std::endl;
    });

    // Try to open Launchpad X (device 1)
    int device_id = 1;
    std::cout << "\nAttempting to open MIDI device " << device_id << " (Launchpad X)..." << std::endl;
    int result = midi.open(device_id);

    if (result == 0) {
        std::cout << "MIDI device opened successfully" << std::endl;

        // Test connection
        if (midi.testMidiConnection()) {
            std::cout << "MIDI connection test: PASSED" << std::endl;
        } else {
            std::cout << "MIDI connection test: FAILED" << std::endl;
        }

        // Listen for MIDI input for 15 seconds
        std::cout << "\nListening for MIDI input for 15 seconds..." << std::endl;
        std::cout << "Please play some notes or press buttons on the Launchpad X" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(15));

    } else {
        std::cout << "Failed to open MIDI device " << device_id << std::endl;
    }

    midi.close();
    std::cout << "\nMIDI device closed. Test completed." << std::endl;

    return 0;
}
