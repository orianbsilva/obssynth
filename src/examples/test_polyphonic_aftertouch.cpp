#include "../lib/utils/platform_midi_reader.hpp"
#include <iostream>
#include <map>

using namespace synth;

// Global state to track notes and their individual aftertouch
std::map<uint8_t, float> activeNotes; // note -> frequency
std::map<uint8_t, float> noteAftertouch; // note -> aftertouch pressure

void noteCallback(const MidiNote& note)
{
    if (note.is_note_on && note.velocity > 0)
    {
        float frequency = MidiInterface::midiNoteToFrequency(note.note);
        activeNotes[note.note] = frequency;
        noteAftertouch[note.note] = 0.0f; // Reset aftertouch for this note

        //std::cout << "♪ Note ON:  " << (int)note.note
                  //<< " (" << frequency << " Hz) vel=" << (int)note.velocity << std::endl;
    }
    else
    {
        activeNotes.erase(note.note);
        noteAftertouch.erase(note.note);

        //std::cout << "♪ Note OFF: " << (int)note.note << std::endl;
    }
}

void channelAftertouchCallback(uint8_t channel, uint8_t pressure)
{
    std::cout << "♪ Channel Aftertouch - Channel: " << (int)channel
              << ", Pressure: " << (int)pressure
              << " (affects ALL notes on channel)" << std::endl;
}

void polyphonicAftertouchCallback(uint8_t channel, uint8_t note, uint8_t pressure)
{
    if (activeNotes.find(note) != activeNotes.end())
    {
        noteAftertouch[note] = pressure;

        std::cout << "♪ Polyphonic Aftertouch - Channel: " << (int)channel
                  << ", Note: " << (int)note
                  << " (" << activeNotes[note] << " Hz), Pressure: " << (int)pressure << std::endl;

        // Show current state of all active notes
        if (activeNotes.size() > 1)
        {
            std::cout << "  Active notes: ";
            for (const auto& pair : activeNotes)
            {
                std::cout << (int)pair.first << ":" << (int)noteAftertouch[pair.first] << " ";
            }
            std::cout << std::endl;
        }
    }
    else
    {
        std::cout << "♪ Polyphonic Aftertouch for inactive note " << (int)note << std::endl;
    }
}

int main()
{
    std::cout << "Polyphonic Aftertouch Test" << std::endl;
    std::cout << "==========================" << std::endl;
    std::cout << "This test demonstrates the difference between:" << std::endl;
    std::cout << "- Channel Aftertouch (0xD0): affects all notes on the channel" << std::endl;
    std::cout << "- Polyphonic Aftertouch (0xA0): affects individual notes" << std::endl;
    std::cout << std::endl;

    MidiInterface reader;

    // Set up callbacks
    reader.setNoteCallback(noteCallback);
    reader.setChannelAftertouchCallback(channelAftertouchCallback);
    reader.setPolyphonicAftertouchCallback(polyphonicAftertouchCallback);

    // Initialize MIDI
    if (reader.init() != 0)
    {
        std::cerr << "Failed to initialize MIDI" << std::endl;
        return 1;
    }

    // Try to open MIDI device, fallback to keyboard if no device found
    if (reader.open() != 0)
    {
        std::cout << "No MIDI device found, enabling keyboard fallback..." << std::endl;
        reader.enableKeyboardFallback();
    }

    if (reader.isKeyboardFallbackEnabled())
    {
        std::cout << "Keyboard fallback mode enabled:" << std::endl;
        std::cout << "- Use 'asdf' keys for white keys (C, D, E, F)" << std::endl;
        std::cout << "- Use 'qwert' keys for black keys (C#, D#, F#, G#, A#)" << std::endl;
        std::cout << "- Channel aftertouch simulated via MIDI controllers (if available)" << std::endl;
        std::cout << "- Polyphonic aftertouch requires real MIDI controller support" << std::endl;
        std::cout << "- Press 'x' to exit" << std::endl;
    } else {
        std::cout << "MIDI device opened successfully!" << std::endl;
        std::cout << "Instructions:" << std::endl;
        std::cout << "1. Play multiple notes simultaneously" << std::endl;
        std::cout << "2. Apply pressure to individual keys (polyphonic aftertouch)" << std::endl;
        std::cout << "3. Apply channel pressure (channel aftertouch)" << std::endl;
        std::cout << "4. Compare the difference in behavior" << std::endl;
        std::cout << "- Press Ctrl+C to exit" << std::endl;
    }

    std::cout << std::endl;

    // Run for 30 seconds or until user exits
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(30);

    while (std::chrono::steady_clock::now() - start_time < timeout)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    reader.close();
    std::cout << "\nTest completed." << std::endl;

    return 0;
}
