# Waveform Drawer Utility Integration

## Overview

The Waveform Drawer utility (`util/waveform_drawer.py`) is a Python-based tool for creating custom waveforms interactively. It integrates with the synth project by saving waveforms to the `pcm/` folder for use in synthesis and audio processing.

## Quick Start

### Windows
```cmd
cd util
run_waveform_drawer.bat
```

### Linux/macOS
```bash
cd util
./run_waveform_drawer.sh
```

### Manual Launch
```bash
cd util
python waveform_drawer.py
```

## Integration Points

### 1. PCM Folder Integration
- All waveforms are saved to `../pcm/` folder
- Compatible with existing project structure
- Supports both 16-bit PCM and 32-bit float formats

### 2. Sample Rate Compatibility
- Supports all common sample rates (8kHz to 96kHz)
- Default 48kHz matches project audio settings
- Files are exactly 1 second long for easy testing

### 3. File Format Standards
- Standard WAV format compatible with all audio software
- Mono channel output (suitable for oscillator waveforms)
- Proper header information for audio applications

## Usage in Synth Project

### 1. Custom Oscillator Waveforms
```cpp
// Load custom waveform in C++ synthesizer
WAVReader reader;
std::vector<float> custom_wave = reader.readWAV("../pcm/my_custom_wave.wav");
oscillator.setCustomWaveform(custom_wave);
```

### 2. Wavetable Synthesis
- Create multiple waveforms for wavetable scanning
- Generate smooth morphing between different shapes
- Use different resolutions for different applications

### 3. Analysis and Research
- Study harmonic content of custom shapes
- Compare with standard waveforms
- Generate test signals for effect processing

## Technical Specifications

### Supported Formats
- **16-bit PCM**: 96KB per file (1 second at 48kHz)
- **32-bit Float**: 192KB per file (1 second at 48kHz)
- **Sample Rates**: 8000, 11025, 22050, 44100, 48000, 96000 Hz
- **Resolutions**: 256, 512, 1024, 2048 samples per cycle

### Output Characteristics
- **Duration**: Exactly 1 second
- **Channels**: Mono (single channel)
- **Amplitude**: Normalized to ±1.0 range
- **Phase**: Starts at 0° (beginning of cycle)

## Development Integration

### Build System
The utility is standalone and doesn't require CMake integration:
- Pure Python implementation
- No compilation required
- Portable across platforms

### Testing
Use the included test script:
```bash
cd util
python test_waveforms.py
```

This generates sample waveforms for validation.

### Dependencies
- Python 3.6+
- numpy (signal processing)
- scipy (optional, for 32-bit float)
- tkinter (GUI, usually included)

## File Organization

```
util/
├── waveform_drawer.py      # Main application
├── test_waveforms.py       # Test script
├── requirements.txt        # Python dependencies
├── run_waveform_drawer.bat # Windows launcher
├── run_waveform_drawer.sh  # Linux/macOS launcher
└── README.md              # Detailed documentation

pcm/                       # Output directory
├── custom_wave_*.wav      # User-created waveforms
└── test_*.wav            # Generated test waveforms
```

## Future Enhancements

### Planned Features
- Audio preview playback
- Fourier analysis display
- Batch export functionality
- Integration with C++ oscillator classes

### C++ Integration Ideas
```cpp
// Future: Direct integration with oscillator classes
class CustomWaveformOscillator : public Oscillator {
public:
    void loadFromPCMFile(const std::string& filename);
    void setWaveformFromDrawer(const std::vector<float>& samples);
};
```

## Performance Notes

### GUI Performance
- Optimized for smooth drawing on 1000x700 canvas
- Real-time interpolation and display
- Minimal memory usage

### File I/O
- Efficient WAV writing using numpy and scipy
- Streaming output for large files
- Cross-platform file handling

## Troubleshooting

### Common Issues
1. **Python not found**: Install Python 3.6+ from python.org
2. **Dependencies missing**: Run `pip install -r requirements.txt`
3. **Permission errors**: Ensure write access to PCM folder
4. **Display issues**: Install tkinter development packages on Linux

### Performance Tips
- Use lower resolutions (256-512) for faster drawing
- Save as 16-bit PCM for smaller files
- Close other applications if GUI is sluggish

This utility extends the synth project's capabilities by providing an intuitive way to create custom waveforms for synthesis and audio research.
