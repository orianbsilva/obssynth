# Waveform Drawer - Custom Wave Shape Creator

A Python application for drawing custom waveforms with the mouse and saving them as WAV files for use with the synth project.

## Features

- **Interactive Drawing Canvas**: Draw waveforms by clicking and dragging with your mouse
- **Multiple Sample Rates**: Support for 8000, 11025, 22050, 44100, 48000, and 96000 Hz
- **Flexible Formats**: Save as 16-bit PCM or 32-bit float WAV files
- **Configurable Resolution**: Choose from 256, 512, 1024, or 2048 samples per wave cycle
- **Preset Waveforms**: Load common waveforms (sine, square, sawtooth, triangle, pulse, noise)
- **Real-time Preview**: View waveform statistics and properties
- **Auto-save to PCM folder**: Files are automatically saved to the project's PCM directory

## Requirements

- Python 3.6 or higher
- numpy (for signal processing)
- scipy (optional, for 32-bit float support)
- tkinter (included with most Python installations)

## Installation

1. **Install Python dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

2. **Run the application**:
   - **Windows**: Double-click `run_waveform_drawer.bat`
   - **Linux/macOS**: Run `./run_waveform_drawer.sh`
   - **Manual**: `python waveform_drawer.py`

## Usage

### Drawing Waveforms

1. **Start Drawing**: Left-click and drag on the black canvas to draw your waveform
2. **Clear Canvas**: Right-click anywhere on the canvas to clear it
3. **Adjust Settings**: Use the controls panel to set sample rate, format, and resolution
4. **Preview**: Click "Preview Wave" to see waveform statistics
5. **Save**: Click "Save WAV File" to export your creation

### Canvas Layout

- **Horizontal Axis**: Represents one complete wave cycle (0° to 360°)
- **Vertical Axis**: Represents amplitude (-1V to +1V)
- **Center Line**: Zero amplitude (0V)
- **Grid Lines**: Help with precise drawing

### Audio Settings

- **Sample Rate**: Determines the audio quality and file size
  - 48000 Hz (recommended for professional audio)
  - 44100 Hz (CD quality)
  - 22050 Hz (good quality, smaller files)
  - Lower rates for testing or special effects

- **Format**: Choose output bit depth
  - 32-bit Float: Professional quality, larger files
  - 16-bit PCM: Standard quality, smaller files

- **Resolution**: Samples per wave cycle
  - Higher resolution = smoother curves
  - Lower resolution = more angular/digital sound

### Keyboard Shortcuts

- **Right-click**: Clear canvas
- **Escape**: Close application

## Preset Waveforms

The application includes several preset waveforms you can load as starting points:

- **Sine Wave**: Smooth, pure tone
- **Square Wave**: Sharp, digital sound
- **Sawtooth Wave**: Bright, buzzing tone
- **Triangle Wave**: Mellow, flute-like sound
- **Pulse Wave**: Narrow square wave (25% duty cycle)
- **Noise**: Random waveform for texture

## Output Files

Saved WAV files are automatically placed in the `../pcm/` folder relative to the synth project. The files contain one second of audio with the drawn waveform repeated at the specified frequency.

### File Naming Convention

Files are automatically named with the following pattern:
```
custom_wave_[resolution]samples_[samplerate]Hz_[bitdepth].wav
```

Example: `custom_wave_1024samples_48000Hz_32float.wav`

## Technical Details

### Waveform Generation

1. **Drawing Phase**: Mouse movements are captured and converted to normalized coordinates
2. **Interpolation**: Points are interpolated to create a smooth waveform at the desired resolution
3. **Repetition**: The single cycle is repeated to create exactly one second of audio
4. **Export**: Data is saved as a standard WAV file

### File Format Support

- **16-bit PCM**: Uses Python's built-in `wave` module
- **32-bit Float**: Uses `scipy.io.wavfile` if available, falls back to 16-bit PCM

### Memory Usage

The application is optimized for low memory usage:
- Waveforms are stored as numpy arrays
- Only one second of audio is generated
- Efficient file I/O with streaming

## Integration with Synth Project

The generated WAV files can be used with the main synthesizer project:

1. **Custom Wavetables**: Use drawn waveforms as wavetable sources
2. **Sample-based Synthesis**: Load custom waveforms into samplers
3. **Analysis**: Study custom waveform properties
4. **Sound Design**: Create unique timbres and textures

## Troubleshooting

### Common Issues

**"numpy not found"**
- Install with: `pip install numpy`

**"tkinter not available"**
- On Linux: `sudo apt-get install python3-tk`
- On macOS: Use Python from python.org (not Homebrew)

**"scipy not found" (when using 32-bit float)**
- Install with: `pip install scipy`
- Or use 16-bit PCM format instead

**"Permission denied" saving files**
- Ensure the PCM folder exists and is writable
- Try running as administrator (Windows) or with sudo (Linux)

### Performance Tips

- Use lower resolutions (256-512 samples) for faster drawing
- Use 16-bit PCM for smaller file sizes
- Close other applications if drawing feels sluggish

## Examples

### Creating a Custom Bass Wave

1. Set resolution to 1024 samples
2. Set sample rate to 48000 Hz
3. Draw a sine wave with some sharp peaks added
4. Save as 32-bit float for maximum quality

### Making Retro Game Sounds

1. Set resolution to 256 samples for pixelated sound
2. Use 22050 Hz sample rate for retro feel
3. Draw sharp, angular waveforms
4. Save as 16-bit PCM

### Designing Smooth Pads

1. Use 2048 sample resolution for maximum smoothness
2. Set 48000 Hz sample rate
3. Draw flowing, organic curves
4. Save as 32-bit float

## Version History

- **v1.0**: Initial release with core drawing and export functionality
- Support for multiple sample rates and bit depths
- Preset waveforms and statistics preview
- Cross-platform compatibility

## License

This utility is part of the synth project and follows the same licensing terms.

## Contributing

Feel free to submit improvements, bug fixes, or new features. Some ideas for future versions:

- Audio preview playback
- Fourier analysis display
- Harmonic content visualization
- Waveform morphing tools
- Batch export functionality
- Custom file format support
