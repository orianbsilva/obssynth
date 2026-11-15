#include "../lib/utils/platform_midi_reader.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace synth;
using namespace examples;

int main() {
    PlatformUtils::initialize();

    PlatformUtils::printHeader("Keyboard MIDI Fallback Demo",
                             "Demonstrates keyboard input when no MIDI device is available");

    const char* controls[] = {
        "White keys: a s d f (C3 D3 E3 F3)",
        "Black keys: q w e r t (C#3 D#3 F#3 G#3 A#3)",
        "Controllers: 0-9 keys (select controller) + +/- keys (adjust value)",
        "Press 'x' to exit keyboard mode",
        "Press 'q' to quit demo",
        nullptr
    };
    PlatformUtils::printControls(controls);

    MidiInterface midiReader;

    // Set up MIDI callbacks
    midiReader.setNoteCallback([](const MidiNote& note) {
        float frequency = MidiInterface::midiNoteToFrequency(note.note);

        if (note.is_note_on && note.velocity > 0) {
            std::cout << "🎵 Note ON:  " << static_cast<int>(note.note)
                     << " (freq: " << frequency << " Hz, vel: "
                     << static_cast<int>(note.velocity) << ")" << std::endl;
        } else {
            std::cout << "🎵 Note OFF: " << static_cast<int>(note.note)
                     << " (freq: " << frequency << " Hz)" << std::endl;
        }
    });

    midiReader.setControlCallback([](uint8_t channel, uint8_t controller, uint8_t value) {
        std::cout << "🎛️  Control Change: Channel=" << static_cast<int>(channel)
                 << " Controller=" << static_cast<int>(controller)
                 << " Value=" << static_cast<int>(value) << std::endl;
    });

    // Initialize MIDI reader
    if (midiReader.init() != 0) {
        std::cerr << "Failed to initialize MIDI reader" << std::endl;
        PlatformUtils::cleanup();
        return 1;
    }

    // Try to open MIDI device (will fallback to keyboard if no device)
    midiReader.open(0);

    std::cout << "\n🎹 Demo running! ";

    if (midiReader.isKeyboardFallbackEnabled()) {
        std::cout << "Using keyboard fallback mode." << std::endl;
        std::cout << "Try pressing the keys listed above!" << std::endl;
    } else {
        std::cout << "Using real MIDI device." << std::endl;
        std::cout << "Play your MIDI controller!" << std::endl;
    }

    // Main loop
    bool running = true;
    while (running) {
        if (PlatformUtils::kbhit()) {
            char c = static_cast<char>(PlatformUtils::getch());
            if (c == 'q' || c == 'Q') {
                running = false;
                break;
            }
            // If not using keyboard fallback, echo the key press
            if (!midiReader.isKeyboardFallbackEnabled()) {
                std::cout << "Key pressed: " << c << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Cleanup
    midiReader.close();

    std::cout << "\n✅ Demo finished!" << std::endl;
    PlatformUtils::cleanup();
    return 0;
}
