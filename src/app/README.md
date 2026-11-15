# Applications - Professional Audio Tools

**Professional audio synthesis and visualization applications** with cross-platform support.

## Project Organization

### 📁 Directory Structure
```
src/app/
├── synth_cli/          # MIDI Synthesizer Application
│   ├── synth_cli.cpp   # Main synthesizer implementation
│   └── README.md       # Detailed synth_cli documentation
├── synth_view/         # OpenGL Waveform Viewer
│   ├── main_screen.cpp # Main application entry
│   ├── gl_renderer.*   # OpenGL rendering engine
│   ├── win32_gl_window.*  # Windows Win32/OpenGL implementation
│   ├── platform_gl.hpp    # OpenGL abstraction layer
│   ├── platform_window.hpp # Window management abstraction
│   └── README.md       # Detailed synth_view documentation
├── common/             # Shared Utilities
│   ├── platform_utils.*# Cross-platform device and system utilities
│   └── README.md       # Common utilities documentation
├── CMakeLists.txt      # Build configuration for all applications
└── README.md           # This file - project overview
```

## 🎹 SYNTH_CLI - Professional MIDI Synthesizer

Complete real-time MIDI synthesizer with professional audio effects.

### Key Features
- **Real-time MIDI Processing**: Professional MIDI device integration
- **Multi-waveform Synthesis**: Sine, sawtooth, square waves
- **ADSR Envelope Control**: Attack, Decay, Sustain, Release
- **LFO Modulation**: Low-frequency oscillation with multiple waveforms
- **Schroeder Reverb**: Professional reverb algorithm with presets
- **WAV Recording**: Real-time audio recording capability
- **Cross-platform Audio**: ALSA (Linux) and WinMM (Windows) backends

## 🎨 SYNTH_VIEW - OpenGL Waveform Viewer

Real-time waveform visualization with interactive control and MIDI integration.

### Key Features
- **Real-time Visualization**: Live OpenGL waveform display at 60 FPS
- **Interactive Control**: Mouse and keyboard parameter control
- **MIDI Integration**: Real-time MIDI note processing with visual feedback
- **Graphics**: the panels obssynth4d and obsdrums are SDL3 on both (Linux: Wayland, the system's SDL3; Windows: SDL3 fetched and built static by CMake); synth_view (Win32/OpenGL, ImGui) is Windows only
- **Audio, MIDI** (common/platform_io.hpp, as v1): Windows ASIO (VB-Audio VASIO-32), else DirectSound, else WaveOut, and every MIDI input device, controllers first; Linux PulseAudio, else ALSA, and a port of the product's own (ObsSynth4D:input, ObsDrums:input)
- **Hardware Acceleration**: GPU-based rendering for smooth performance
- **Virtual Keyboard**: Fallback when MIDI devices unavailable

## Quick Start

### Linux/macOS
```bash
# Quick launch (builds automatically if needed)
./synth_cli.sh

# Manual launch
cd build && ./synth_cli
```

### Windows
```cmd
# Quick launch (builds automatically if needed)
synth_cli.bat

# Manual launch
cd build && synth_cli.exe
```

## Features

### Real-time MIDI Control
- **Device Enumeration**: Professional device listing with hardware details
- **Note Processing**: Real-time MIDI note-on/off with velocity control
- **Aftertouch**: Channel and polyphonic aftertouch support
- **Controller Support**: MIDI CC mapping for all major parameters

### Audio Effects
- **Schroeder Reverb**: Professional reverb algorithm with multiple presets
  - Small Room, Medium Room, Large Room, Hall presets
  - Real-time parameter control (mix, room size, damping)
- **ADSR Envelope**: Full envelope control with attack, decay, sustain, release
- **LFO Modulation**: Multiple waveform LFO with frequency and amplitude control

### Recording & Playback
- **Direct Recording**: Real-time recording to WAV files
- **Multiple Wave Types**: Sine, Square, Sawtooth oscillators
- **Professional Audio**: 44.1kHz, 16-bit, optimized performance

## Controls

### MIDI Controls
- **MIDI Notes** - Control oscillator pitch and amplitude
- **CC 21-24** - ADSR envelope parameters (Attack, Decay, Sustain, Release)
- **CC 91** - Reverb level (0-60%)
- **CC 93** - Room size (0-100%)
- **CC 94** - Reverb damping (0-100%)
- **Channel Aftertouch** - Global frequency modulation (±10%)
- **Polyphonic Aftertouch** - Per-note frequency modulation (±10%)

### Keyboard Shortcuts
- **1/2/3** - Switch wave types (Sine/Square/Sawtooth)
- **R** - Start recording to WAV file
- **s** - Stop recording and save WAV file
- **v** - Toggle reverb on/off
- **V** - Cycle reverb presets
- **l** - Toggle LFO on/off
- **f/F** - LFO frequency up/down
- **w** - Cycle LFO waveform
- **a/A** - LFO amplitude up/down
- **r** - Reset LFO phase
- **q** - Quit application

## Device Selection

On startup, synth_cli will:
1. Scan and display all available MIDI input devices
2. Scan and display all available audio output devices
3. Prompt for MIDI device selection with professional formatting
4. Initialize audio subsystem and start real-time processing

Example device listing:
```
Available MIDI input devices:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  [0] Logi USB Headset (Audio Device)
  [1] Launchpad X (MIDI Controller)
  [2] HDA NVidia (System Audio)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

## Technical Details

- **Language**: Modern C++17 with smart pointers and RAII
- **Audio Backend**: ALSA (Linux), WinMM (Windows)
- **Sample Rate**: 44.1kHz
- **Bit Depth**: 16-bit
- **Buffer Size**: Optimized for real-time performance
- **Memory Management**: Safe smart pointer usage throughout
- **Performance**: Pre-computed frequency lookup tables for optimal performance

## Build Information

- **Build System**: CMake 3.20+
- **Dependencies**: System libraries only (ALSA on Linux; WinMM on Windows)
- **Output**: Available in both `build/synth_cli` and `build/bin/synth_cli`
- **Size**: ~140KB optimized executable

## Project Status

SYNTH_CLI represents the culmination of all synthesizer features into a single, professional application:
- ✅ Frequency lookup table optimization
- ✅ Professional MIDI device enumeration
- ✅ Audio device listing and selection
- ✅ Integrated Schroeder reverb effects
- ✅ Real-time MIDI CC control mapping
- ✅ Cross-platform compatibility
- ✅ Professional user interface

This is the **main application** of the synthesizer project, providing a complete, production-ready MIDI synthesizer experience.
