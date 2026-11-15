# SYNTH_CLI - Professional MIDI Synthesizer

A complete real-time MIDI synthesizer with professional audio effects and cross-platform support.

## Features

### Core Synthesis
- **Multi-waveform Oscillator**: Sine, sawtooth, square waves
- **ADSR Envelope**: Attack, Decay, Sustain, Release controls
- **LFO Modulation**: Low-frequency oscillation with multiple waveforms
- **Polyphonic Support**: Multiple simultaneous notes

### Real-time MIDI Control
- **Device Enumeration**: Professional device listing with hardware details
- **Note Processing**: Real-time MIDI note-on/off with velocity control
- **Aftertouch Support**: Channel and polyphonic aftertouch
- **MIDI CC Mapping**: Controller support for all major parameters

### Audio Effects
- **Schroeder Reverb**: Professional reverb algorithm
  - Multiple presets: Small Room, Medium Room, Large Room, Hall
  - Real-time parameter control via MIDI CC
- **Real-time Recording**: WAV file recording during performance

### Platform Support
- **Linux**: ALSA (or PulseAudio) audio, keyboard input from the terminal
- **Windows**: WinMM audio backend, Win32 keyboard input

## Controls

### MIDI Controls
- **Notes**: Control oscillator pitch and amplitude
- **Channel Aftertouch**: Frequency modulation (±10%) for all notes
- **Polyphonic Aftertouch**: Frequency modulation (±10%) per note
- **CC 21-24**: ADSR envelope control (Attack, Decay, Sustain, Release)
- **CC 91**: Reverb level (0-60%)
- **CC 93**: Room size (0-100%)
- **CC 94**: Reverb damping (0-100%)

### Keyboard Controls
- **1, 2, 3**: Switch waveform (Sine, Square, Sawtooth)
- **R**: Start recording to WAV file
- **S**: Stop recording and save
- **V**: Toggle reverb on/off
- **V** (shift): Cycle reverb presets
- **L**: Toggle LFO on/off
- **F/f**: LFO frequency up/down
- **W**: Cycle LFO waveform
- **A/a**: LFO amplitude up/down
- **R**: Reset LFO phase
- **Q**: Quit application

## Quick Start

### Linux
```bash
# From project root
./synth_cli.sh

# Direct execution
./build/bin/synth_cli
```

### Windows
```cmd
# From project root
synth_cli.bat

# Direct execution
build\bin\synth_cli.exe
```

## Build Requirements

### Linux
- ALSA development libraries (`libasound2-dev`)
- Threading support (pthread)

### Windows
- WinMM library (included with Windows SDK)
- Windows threading API

## Technical Implementation

The synthesizer is built on:
- **Core Library**: `synth_core` for audio processing
- **Platform Utilities**: Cross-platform MIDI and audio device handling
- **Float-only Math**: Optimized for audio processing with strict float usage
- **Real-time Architecture**: Low-latency audio processing suitable for live performance
