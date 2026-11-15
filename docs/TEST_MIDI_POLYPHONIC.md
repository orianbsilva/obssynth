# Polyphonic MIDI Synthesizer Test

## Overview

`test_midi_polyphonic` is a real-time polyphonic MIDI synthesizer that supports up to 8 simultaneous voices with ADSR envelope control. This is an enhanced version of the original `test_midi` that uses the `PolyphonicADSROscillator` instead of `EnvelopedOscillator`.

## Key Features

### Polyphonic Operation
- **8 Voice Polyphony**: Can play up to 8 notes simultaneously
- **Voice Stealing**: Automatically manages voice allocation when all voices are in use
- **Per-Note ADSR**: Each voice has its own ADSR envelope
- **Real-time Voice Status**: Display active, releasing, and free voices

### MIDI Control
- **Note On/Off**: Play multiple notes simultaneously with velocity sensitivity
- **CC Control**: Real-time ADSR parameter adjustment via MIDI controllers
  - CC 21: Attack time (0.01s - 1.0s)
  - CC 22: Decay time (0.01s - 5.0s)
  - CC 23: Sustain level (1% - 70%)
  - CC 24: Release time (0.01s - 5.0s)

### Audio Features
- **Wave Types**: Sine, Square, and Sawtooth waves for all voices
- **WAV Recording**: Record polyphonic performances to file
- **Master Amplitude**: Global volume control for all voices

## Differences from Original test_midi

### Changed Features
1. **Polyphonic Operation**: Can play multiple notes simultaneously instead of monophonic
2. **Recording Filename**: Uses "midi_polyphonic_session.wav" instead of "midi_session.wav"
3. **Voice Management**: Displays voice status and provides "all notes off" functionality
4. **LFO Removed**: LFO modulation not implemented for polyphonic operation

### Removed Features
- **LFO Control**: Frequency modulation via LFO not available
- **Aftertouch Modulation**: Channel and polyphonic aftertouch frequency modulation disabled
- **Phase Reset**: No manual phase reset (handled automatically per voice)

## Controls

### Keyboard Controls
- **1, 2, 3**: Switch wave types (Sine, Square, Sawtooth)
- **R**: Start recording to WAV file
- **s**: Stop recording and save WAV file
- **l**: Show voice status (active, releasing, free voices)
- **a**: All notes off (trigger release on all active voices)
- **q**: Quit

### MIDI Controls
- **MIDI Notes**: Play polyphonic notes with velocity sensitivity
- **CC 21-24**: Real-time ADSR parameter control

## Usage Example

```bash
cd synth   # your clone
./build/bin/test_midi_polyphonic
```

1. Connect a MIDI keyboard or use virtual MIDI
2. Select MIDI input device when prompted
3. Play chords and melodies - up to 8 notes simultaneously
4. Use CC knobs to adjust ADSR parameters in real-time
5. Press 'l' to monitor voice usage
6. Press 'R' to start recording, 's' to stop and save
7. Press 'a' to stop all notes, 'q' to quit

## Technical Notes

### Voice Allocation
- Free voices are allocated first
- When all 8 voices are active, oldest voice is stolen for new notes
- Each voice has independent ADSR envelope state
- Voice release continues even after note off until envelope completes

### Performance
- Optimized for real-time audio generation
- Thread-safe voice management
- Minimal latency MIDI input processing
- Efficient polyphonic audio mixing

### Recording
- Records the mixed polyphonic output
- 44.1kHz, 16-bit, mono WAV format
- Real-time recording with no audio dropouts
- Automatic file size and duration calculation

## File Output

- **Recording**: `midi_polyphonic_session.wav` in current directory
- **Console Output**: Real-time voice status and MIDI event information

This polyphonic synthesizer demonstrates advanced real-time audio synthesis with multiple voice management, making it suitable for complex musical performances and chord playing.
