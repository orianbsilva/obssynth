# ASIO Backend Implementation

This document describes the ASIO (Audio Stream Input/Output) backend implementation for the Synth Audio Synthesizer.

## Overview

ASIO is a professional audio driver standard developed by Steinberg that provides:
- Ultra-low latency audio (typically 1-10ms)
- Direct hardware access bypassing Windows audio subsystem
- Support for professional audio interfaces
- Stable timing for real-time audio applications
- Multi-channel audio support

## Implementation

### Core Components

**AsioBackend Class** (`src/lib/audio/asio_backend.cpp`)
- Implements the `IAudioBackend` interface
- Manages ASIO driver initialization and lifecycle
- Handles audio buffer management and callbacks
- Provides low-latency audio processing

### Key Features

1. **Driver Management**
   - Automatic ASIO driver detection and initialization
   - Support for multiple ASIO drivers on the system
   - Proper cleanup and resource management

2. **Buffer Management**
   - Double-buffered audio processing
   - Configurable buffer sizes (typically 64-2048 samples)
   - Automatic format conversion (float to ASIO native format)

3. **Callback System**
   - Real-time audio generation through callbacks
   - Thread-safe buffer switching
   - Minimal latency audio processing

4. **Channel Support**
   - Stereo output (mono signals duplicated to both channels)
   - Extensible for multi-channel audio interfaces
   - Channel mapping and routing

## Usage

### Basic Usage

```cpp
#include "audio_sink.hpp"

// Create audio sink with ASIO backend
synth::AudioSink audioSink;
audioSink.setBackend(synth::AudioBackendType::ASIO);

// Initialize ASIO
if (audioSink.init() != 0) {
    // Handle initialization failure
    std::cerr << "Failed to initialize ASIO" << std::endl;
    return -1;
}

// Set audio callback
audioSink.setAudioCallback([](float* buffer, int bufferSize, int* samples_generated) {
    // Generate audio samples
    for (int i = 0; i < bufferSize; ++i) {
        buffer[i] = /* your audio generation code */;
    }
    *samples_generated = bufferSize;
});

// Start audio playback
audioSink.start();
```

### Professional Audio Setup

```cpp
// For professional applications, consider:
audioSink.setBackend(synth::AudioBackendType::ASIO);

// ASIO provides:
// - Latencies as low as 1-2ms with professional interfaces
// - Direct hardware access for maximum performance
// - Support for high sample rates (48kHz, 96kHz, 192kHz)
// - Multi-channel operation for complex audio routing
```

## Requirements

### Software Requirements
- Windows 7 or later (ASIO is Windows-only)
- ASIO drivers installed for your audio interface
- Professional audio interface (recommended)

### Hardware Requirements
- Audio interface with ASIO driver support
- Examples: RME, Focusrite, PreSonus, Steinberg, MOTU devices
- Built-in audio cards may have generic ASIO drivers (ASIO4ALL)

### Development Requirements
For full ASIO implementation, you need:
- Steinberg ASIO SDK (available from Steinberg)
- ASIO headers and libraries
- Compatible audio interface for testing

## Configuration

### Buffer Size Selection
```cpp
// ASIO buffer sizes (samples):
// 64   - Ultra-low latency (~1.5ms at 44.1kHz) - requires powerful CPU
// 128  - Very low latency (~3ms at 44.1kHz) - good for real-time performance
// 256  - Low latency (~6ms at 44.1kHz) - balanced performance
// 512  - Standard latency (~12ms at 44.1kHz) - reliable for most systems
// 1024+ - Higher latency - use for high CPU load applications
```

### Sample Rate Configuration
```cpp
// Common sample rates:
// 44100 Hz - CD quality, widely supported
// 48000 Hz - Professional standard
// 96000 Hz - High resolution audio
// 192000 Hz - Ultra-high resolution (requires high-end interfaces)
```

## Testing

### Test Program
Run the ASIO test program to verify functionality:

```bash
# Build and run ASIO test
./build/bin/Release/test_asio.exe
```

Expected output:
```
ASIO Audio Backend Test
======================

Available audio devices:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
WaveOut Devices:
  [0] Speakers (Realtek High Definition Audio) (WaveOut)
  [1] Windows Audio Session API (WASAPI)
  [2] Audio Stream Input/Output (ASIO)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Initializing ASIO backend...
ASIO Driver: Professional Audio Interface (Version 2)
ASIO Channels: 2 input, 2 output
ASIO Buffer size: 256 samples (range: 64-2048)
Output latency: 5.8 ms
ASIO backend initialized successfully
```

## Troubleshooting

### Common Issues

**"Failed to initialize ASIO driver"**
- Ensure ASIO drivers are properly installed
- Check that audio interface is connected and powered on
- Verify no other application is using the ASIO driver
- Try different buffer sizes

**"No ASIO devices found"**
- Install ASIO drivers for your audio interface
- Consider using ASIO4ALL as a generic ASIO driver
- Check Windows Device Manager for audio device status

**High CPU usage or audio dropouts**
- Increase buffer size (e.g., from 128 to 256 samples)
- Close unnecessary applications
- Disable Windows audio enhancements
- Check for driver updates

**Latency higher than expected**
- Verify ASIO driver is actually being used (not Windows audio)
- Reduce buffer size if CPU can handle it
- Check audio interface specifications
- Ensure ASIO driver is not using internal resampling

### Performance Optimization

1. **CPU Optimization**
   - Use Release build configuration
   - Disable debug output in production
   - Optimize audio generation algorithms
   - Consider CPU affinity for audio threads

2. **Driver Settings**
   - Use smallest stable buffer size
   - Disable unnecessary audio processing in driver
   - Set audio interface to external clock if available
   - Avoid USB hubs for audio interfaces

3. **System Configuration**
   - Disable Windows audio enhancements
   - Set high performance power plan
   - Disable unnecessary Windows services
   - Use dedicated audio interface (not built-in audio)

## Integration with Real ASIO SDK

This implementation provides a framework that can be integrated with the actual Steinberg ASIO SDK:

```cpp
// To integrate with real ASIO SDK:
// 1. Obtain ASIO SDK from Steinberg
// 2. Replace mock ASIO types with real SDK headers
// 3. Link against ASIO SDK libraries
// 4. Implement proper ASIO driver enumeration
// 5. Add error handling for specific ASIO error codes
```

## Limitations

Current implementation limitations:
- Mock ASIO driver (replace with real SDK for production)
- Limited to stereo output
- No ASIO driver enumeration
- No advanced ASIO features (time info, sample position, etc.)

For production use, integrate with the full Steinberg ASIO SDK to access:
- Real ASIO driver support
- Complete ASIO feature set
- Professional audio interface compatibility
- Advanced timing and synchronization features

## See Also

- [WASAPI Implementation](WASAPI_IMPLEMENTATION.md)
- [Audio Backend Architecture](AUDIO_BACKEND_ARCHITECTURE.md)
- [Synth Core Documentation](SYNTH.md)
- [Performance Optimization Guide](PERFORMANCE_OPTIMIZATION.md)
