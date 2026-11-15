# Synth CLI Package

This package contains the Synth CLI audio synthesizer and its associated PCM waveform data files.

## Contents

### bin/
- `synth_cli.exe` - Main MIDI synthesizer application
- Various test executables for demonstration and testing

### pcm/
- PCM waveform data files in WAV format
- Used by the synthesizer for custom waveforms
- Includes sine, square, sawtooth, triangle, pulse, and harmonic variations

## Quick Start

1. Navigate to the package directory
2. Run `.\bin\synth_cli.exe` to start the synthesizer
3. The synthesizer will automatically detect PCM files in the `pcm/` directory
4. Connect a MIDI controller or use the keyboard fallback mode

## Usage

The synthesizer supports:
- Real-time polyphonic MIDI input (up to 8 simultaneous notes)
- Multiple waveform types
- ADSR envelope control
- Reverb and chorus effects
- WAV file recording

## Requirements

- Windows 10/11
- Audio output device
- Optional: MIDI controller for full functionality

## Support

For more information, visit: https://github.com/orianbsilva/obssynth
