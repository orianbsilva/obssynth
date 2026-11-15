# Common Utilities - Shared Application Components

Platform-specific utilities shared across all applications in the synth project.

## Overview

The `common/` directory contains shared utilities that provide cross-platform functionality for both `synth_cli` and `synth_view` applications.

## Components

### Platform Utils (`platform_utils.cpp/hpp`)

Cross-platform utilities for system interaction and device management.

#### Key Functions
- **Device Enumeration**: MIDI and audio device discovery
- **System Integration**: Platform-specific system calls
- **Error Handling**: Unified error reporting across platforms
- **Resource Management**: Platform-specific resource cleanup

#### Platform Support
- **Linux**: ALSA integration; panel_kit.hpp: the SDL3 panels' drawing (Wayland)
- **Windows**: WinMM integration, Win32 utilities

#### Usage
```cpp
#include "platform_utils.hpp"

// Example: Enumerate MIDI devices
auto midi_devices = enumerate_midi_devices();
for (const auto& device : midi_devices) {
    std::cout << "Device: " << device.name << std::endl;
}
```

## Design Philosophy

### Cross-Platform Abstraction
- **Unified Interface**: Same API across all platforms
- **Platform-Specific Implementation**: Optimized for each target platform
- **Graceful Degradation**: Fallback behavior when features unavailable

### Shared Dependencies
- **Core Library**: Links to `synth_core` for audio types and constants
- **Standard Library**: Minimal external dependencies
- **Platform APIs**: Direct system API usage for optimal performance

## Build Integration

The common utilities are built as a static library (`app_common`) that both applications link against:

```cmake
target_link_libraries(synth_cli PRIVATE app_common)
target_link_libraries(synth_view PRIVATE app_common)
```

## Coding Standards

### Float-Only Policy
- All floating-point values use `float` type with `f` suffix
- No `double` precision to maintain audio processing consistency
- Example: `440.0f` not `440.0`

### Error Handling
- Platform-specific error codes converted to common enum
- Descriptive error messages with context
- Graceful fallback when possible

### Memory Management
- RAII principles for resource management
- Platform-specific cleanup in destructors
- No raw pointers for managed resources

## Future Extensions

### Planned Features
- **Configuration Management**: Cross-platform settings storage
- **Plugin Interface**: Dynamic library loading for effects
- **Network Utilities**: MIDI over network support
- **File System**: Cross-platform file operations

### Platform Extensions
- **macOS**: Core Audio and Core MIDI support
- **Mobile**: iOS/Android audio backends
- **Embedded**: Real-time OS support for hardware synthesizers
