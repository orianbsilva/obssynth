# Polyphonic Oscillator Documentation

## Overview

The `PolyphonicOscillator` class provides a polyphonic synthesizer capable of playing up to 8 simultaneous voices. Each voice is an independent oscillator that can be controlled individually, allowing for chord playing and complex harmonic content.

## Features

- **8 Voice Polyphony**: Supports up to 8 simultaneous notes
- **Voice Stealing**: Automatically manages voice allocation when all voices are in use
- **Multiple Wave Types**: Sine, Square, and Sawtooth waves
- **MIDI Integration**: Easy integration with MIDI note events
- **Thread-Safe**: Safe to use in multi-threaded audio applications
- **Real-time Performance**: Optimized for real-time audio generation

## Basic Usage

### 1. Include the Header

```cpp
#include "src/oscillators/polyphonic_oscillator.hpp"
using namespace synth;
```

### 2. Create a Polyphonic Oscillator

```cpp
// Create with sine waves at 44.1kHz sample rate
auto poly_osc = std::make_unique<PolyphonicOscillator>(WaveType::SINE, 44100.0f);
```

### 3. Audio Callback Integration

```cpp
// Audio generation callback
void audioCallback(float *buffer, int bufferSize, int *out_buffer_size)
{
    if (poly_osc)
    {
        // Generate polyphonic audio samples
        poly_osc->generateBuffer(buffer, bufferSize);
        *out_buffer_size = bufferSize;
    }
    else
    {
        // Fill with silence if no oscillator
        std::fill(buffer, buffer + bufferSize, 0.0f);
        *out_buffer_size = bufferSize;
    }
}
```

### 4. Playing Notes

```cpp
// Play a note (MIDI note number, velocity 0.0-1.0)
poly_osc->noteOn(60, 0.8f);  // C4 at 80% velocity

// Stop a note
poly_osc->noteOff(60);       // Stop C4

// Play a chord
poly_osc->noteOn(60, 0.8f);  // C4
poly_osc->noteOn(64, 0.8f);  // E4
poly_osc->noteOn(67, 0.8f);  // G4

// Stop all notes
poly_osc->allNotesOff();
```

## MIDI Integration Example

```cpp
// MIDI note callback function
void midiNoteCallback(const MidiNote &note)
{
    if (!poly_osc) return;

    if (note.is_note_on && note.velocity > 0)
    {
        // Note ON - start playing on available voice
        float velocity = static_cast<float>(note.velocity) / 127.0f;
        poly_osc->noteOn(note.note, velocity);
    }
    else
    {
        // Note OFF - stop playing this note
        poly_osc->noteOff(note.note);
    }
}
```

## API Reference

### Constructor

```cpp
PolyphonicOscillator(WaveType wave_type, float sample_rate)
```
- `wave_type`: Initial wave type (SINE, SQUARE, SAW)
- `sample_rate`: Audio sample rate (typically 44100.0f)

### Voice Control

```cpp
void noteOn(uint8_t note_number, float velocity = 1.0f)
```
- Start playing a MIDI note
- `note_number`: MIDI note (0-127, where 60 = C4)
- `velocity`: Note velocity (0.0-1.0)

```cpp
void noteOff(uint8_t note_number)
```
- Stop playing a specific MIDI note

```cpp
void allNotesOff()
```
- Stop all currently playing notes

### Parameter Control

```cpp
void setWaveType(WaveType wave_type)
```
- Change wave type for all voices (SINE, SQUARE, SAW)

```cpp
void setMasterAmplitude(float amplitude)
```
- Set master amplitude scaling for all active voices

### Audio Generation

```cpp
float getSample()
```
- Generate a single mixed sample from all active voices

```cpp
void generateBuffer(float* buffer, int size)
```
- Generate a buffer of mixed samples from all active voices

### Voice Status

```cpp
int getActiveVoiceCount() const
```
- Get the number of currently active voices

```cpp
bool isNoteActive(uint8_t note_number) const
```
- Check if a specific note is currently playing

### Utility Functions

```cpp
static float noteToFrequency(uint8_t note_number)
```
- Convert MIDI note number to frequency in Hz
- MIDI note 69 (A4) = 440 Hz

## Voice Management

The polyphonic oscillator uses intelligent voice management:

1. **Free Voice Allocation**: When a note is played, it first looks for an unused voice
2. **Voice Stealing**: If all 8 voices are active, it steals the oldest voice
3. **Note Tracking**: Each voice tracks which MIDI note it's playing
4. **Duplicate Note Handling**: If the same note is played twice, it stops the first instance

## Performance Considerations

- Each voice runs an independent oscillator
- Audio generation is optimized for real-time performance
- Voice mixing is performed in floating-point with proper scaling
- Amplitude is automatically scaled by voice count to prevent clipping

## Example: Complete Polyphonic Synthesizer

```cpp
#include "src/audio/audio_sink.hpp"
#include "src/oscillators/polyphonic_oscillator.hpp"
#include "src/utils/midi_reader.hpp"

using namespace synth;

// Global polyphonic oscillator
std::unique_ptr<PolyphonicOscillator> g_poly_osc;

// Audio callback
void audioCallback(float *buffer, int bufferSize, int *out_buffer_size)
{
    if (g_poly_osc)
    {
        g_poly_osc->generateBuffer(buffer, bufferSize);
        *out_buffer_size = bufferSize;
    }
    else
    {
        std::fill(buffer, buffer + bufferSize, 0.0f);
        *out_buffer_size = bufferSize;
    }
}

// MIDI callback
void midiCallback(const MidiNote &note)
{
    if (!g_poly_osc) return;
    
    if (note.is_note_on && note.velocity > 0)
    {
        float velocity = static_cast<float>(note.velocity) / 127.0f;
        g_poly_osc->noteOn(note.note, velocity);
    }
    else
    {
        g_poly_osc->noteOff(note.note);
    }
}

int main()
{
    // Initialize audio
    AudioSink sink;
    sink.init();
    
    // Create polyphonic oscillator
    g_poly_osc = std::make_unique<PolyphonicOscillator>(WaveType::SINE, 44100.0f);
    
    // Start audio with callback
    sink.startWithCallback(audioCallback);
    
    // Initialize and start MIDI
    MidiReader midi;
    midi.init();
    midi.setNoteCallBack(midiCallback);
    midi.start(0);
    
    // Main loop
    std::cout << "Polyphonic synthesizer running. Press Enter to quit.\n";
    std::cin.get();
    
    return 0;
}
```

## Troubleshooting

### No Sound Output
- Ensure audio sink is properly initialized
- Check that sample rate matches audio system
- Verify notes are being triggered with `getActiveVoiceCount()`

### Clicking/Popping Sounds
- The oscillators automatically reset phase on note start to minimize clicks
- Ensure proper amplitude scaling (don't exceed 1.0)

### Voice Stealing Issues
- Monitor voice count with `getActiveVoiceCount()`
- Consider implementing more sophisticated voice stealing algorithms
- Use `allNotesOff()` to reset all voices if needed

## Note Frequency Reference

| MIDI Note | Note Name | Frequency (Hz) |
|-----------|-----------|----------------|
| 60        | C4        | 261.63         |
| 61        | C#4       | 277.18         |
| 62        | D4        | 293.66         |
| 63        | D#4       | 311.13         |
| 64        | E4        | 329.63         |
| 65        | F4        | 349.23         |
| 66        | F#4       | 369.99         |
| 67        | G4        | 392.00         |
| 68        | G#4       | 415.30         |
| 69        | A4        | 440.00         |
| 70        | A#4       | 466.16         |
| 71        | B4        | 493.88         |
| 72        | C5        | 523.25         |
