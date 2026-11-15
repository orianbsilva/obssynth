#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace synth;

void channelAftertouchCallback(uint8_t channel, uint8_t pressure)
{
    std::cout << "Channel Aftertouch - Channel: " << (int)channel
              << ", Pressure: " << (int)pressure << std::endl;
}

void noteCallback(const MidiNote& note)
{
    std::cout << "Note " << (note.velocity > 0 ? "ON" : "OFF")
              << " - Channel: " << (int)note.channel
              << ", Note: " << (int)note.note
              << ", Velocity: " << (int)note.velocity << std::endl;
}

int main()
{
    MidiInterface reader;

    // Set up callbacks
    reader.setNoteCallback(noteCallback);
    reader.setChannelAftertouchCallback(channelAftertouchCallback);

    // Initialize MIDI
    if (reader.init() != 0) {
        std::cerr << "Failed to initialize MIDI" << std::endl;
        return 1;
    }

    // Try to open MIDI device, fallback to keyboard if no device found
    if (reader.open() != 0) {
        std::cout << "No MIDI device found, enabling keyboard fallback..." << std::endl;
        reader.enableKeyboardFallback();
    }

    std::cout << "MIDI Channel Aftertouch Test" << std::endl;
    std::cout << "=============================" << std::endl;

    if (reader.isKeyboardFallbackEnabled()) {
        std::cout << "Keyboard fallback mode enabled:" << std::endl;
        std::cout << "- Use 'asdf' keys for white keys (C, D, E, F)" << std::endl;
        std::cout << "- Use 'qwert' keys for black keys (C#, D#, F#, G#, A#)" << std::endl;
        std::cout << "- Channel aftertouch can be generated via MIDI controllers" << std::endl;
        std::cout << "- Press 'x' to exit" << std::endl;
    } else {
        std::cout << "MIDI device opened successfully!" << std::endl;
        std::cout << "- Send channel aftertouch messages from your MIDI controller" << std::endl;
        std::cout << "- Press Ctrl+C to exit" << std::endl;
    }

    std::cout << std::endl;

    // Run for 30 seconds
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(30);

    while (std::chrono::steady_clock::now() - start_time < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    reader.close();
    std::cout << "\nTest completed." << std::endl;

    return 0;
}
