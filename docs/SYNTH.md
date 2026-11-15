# Windows Audio Synthesizer - C Implementation

✅ **COMPLETED**: Always use C language and no other audio library only Windows Syscalls

✅ **COMPLETED**: Create a sink audio output to a real device for Windows
- Implemented using Windows WinMM API (`waveOut*` functions)
- Direct system call interface, no external libraries
- Real-time audio output with buffering

✅ **COMPLETED**: Create an oscillator that generates sine, saw and square wave
- Pure C implementation with math.h
- Three wave types: Sine, Sawtooth, Square
- Configurable frequency, amplitude, and sample rate

✅ **COMPLETED**: Create a test file to play an A4 pitch to an audio device
- Test application plays 440 Hz (A4) for 2 seconds each wave type
- Demonstrates all three wave forms
- Lists available audio devices

## Files Created:
- `src/audio_sink.h/.c` - Windows audio output management
- `src/oscillator.h/.c` - Wave generation engine  
- `src/midi_reader.h/.c` - MIDI input handling
- `src/test_a4.c` - A4 pitch test application
- `src/test_midi.c` - MIDI synthesizer application
- `build.bat` - Automated build script
- `run.bat` - A4 test execution script
- `run_midi.bat` - MIDI test execution script
- `clean.bat` - Build cleanup script
- `Makefile` - Make build configuration
- `README.md` - Documentation

## Project Structure:
```
synth/
├── src/           # Source code
├── build/         # Build artifacts
├── *.bat          # Windows build scripts
└── README.md      # Documentation
```

## Usage:
1. Run `build.bat` to compile both applications
2. Run `run.bat` or `build\test_a4.exe` for A4 pitch test
3. Run `run_midi.bat` or `build\test_midi.exe` for MIDI synthesizer




✅ **COMPLETED**: Create a MIDI reader
- Windows MIDI input using WinMM API (`midiIn*` functions)
- Real-time MIDI message parsing (Note ON/OFF, velocity, channel)
- MIDI note to frequency conversion
- Callback system for note events

✅ **COMPLETED**: Create a test file that reads MIDI input and changes the pitch of oscillator
- Real-time MIDI synthesizer application
- Dynamic frequency control from MIDI notes
- Velocity-based amplitude control
- Interactive wave type switching (1, 2, 3 keys)
- Multiple MIDI device support

