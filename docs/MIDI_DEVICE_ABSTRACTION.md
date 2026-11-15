# MIDI Device Abstraction System

## Overview

The MIDI Device Abstraction System provides a unified interface for interacting with various MIDI grid controllers, starting with the Novation Launchpad X. This system allows for device-specific features while maintaining a consistent API.

## Architecture

### Base Classes

#### `MidiDevice` (Abstract Base Class)
- **Purpose**: Defines the common interface for all MIDI devices
- **Key Features**:
  - Device capability detection
  - Grid position abstraction
  - Event callback system
  - Color management (RGB to device-specific mapping)
  - Connection management

#### `DeviceFactory`
- **Purpose**: Creates device instances based on device name/type detection
- **Features**:
  - Automatic device type detection
  - Supported device enumeration
  - Device-specific instantiation

### Device Implementations

#### `LaunchpadX`
- **Purpose**: Complete implementation for Novation Launchpad X
- **Capabilities**:
  - 8x8 RGB LED grid
  - Velocity-sensitive pads
  - Polyphonic aftertouch
  - 8 side buttons (right edge)
  - 8 top buttons
  - Programmer mode support

### Key Structures

#### `RGBColor`
```cpp
struct RGBColor {
    uint8_t red, green, blue;
    // Predefined colors: OFF, RED, GREEN, BLUE, YELLOW, CYAN, MAGENTA, WHITE
};
```

#### `PadPosition`
```cpp
struct PadPosition {
    uint8_t x, y;  // 0-based grid coordinates
};
```

#### `DeviceCapability` (Flags)
- `RGB_LEDS`: Device supports colored LEDs
- `VELOCITY_SENSITIVE`: Pads are velocity-sensitive
- `AFTERTOUCH`: Device supports aftertouch
- `SIDE_BUTTONS`: Device has side buttons
- `TOP_BUTTONS`: Device has top buttons
- `FADERS`: Device has faders (future)
- `ROTARY_ENCODERS`: Device has encoders (future)
- `DISPLAYS`: Device has displays (future)

## Usage Examples

### Basic Device Setup

```cpp
#include "src/lib/devices/launchpadx.hpp"

// Create device using factory
auto device = DeviceFactory::createDevice("Launchpad X");
if (!device) {
    std::cerr << "Device not supported!" << std::endl;
    return -1;
}

// Check capabilities
if (device->hasCapability(DeviceCapability::RGB_LEDS)) {
    std::cout << "Device supports RGB LEDs" << std::endl;
}

// Connect to MIDI device (device ID from MIDI system)
if (!device->connect(1)) {
    std::cerr << "Failed to connect to device!" << std::endl;
    return -1;
}
```

### Setting Colors

```cpp
// Set individual pad colors
device->setPadColor(PadPosition(0, 0), RGBColor::RED);
device->setPadColor(PadPosition(1, 0), RGBColor::GREEN);

// Set all pads to the same color
device->setAllPads(RGBColor::BLUE);

// Clear all pads
device->clearAllPads();

// Set button colors (if supported)
device->setSideButtonColor(0, RGBColor::YELLOW);
device->setTopButtonColor(0, RGBColor::CYAN);
```

### Event Handling

```cpp
// Set up event callbacks
device->setPadCallback([](const PadPosition& pos, uint8_t velocity, bool is_press) {
    std::cout << "Pad at (" << pos.x << ", " << pos.y << ") "
              << (is_press ? "pressed" : "released")
              << " with velocity " << static_cast<int>(velocity) << std::endl;
});

device->setSideButtonCallback([](uint8_t button_id, bool is_press) {
    std::cout << "Side button " << static_cast<int>(button_id)
              << " " << (is_press ? "pressed" : "released") << std::endl;
});

device->setAftertouchCallback([](const PadPosition& pos, uint8_t pressure) {
    std::cout << "Aftertouch at (" << pos.x << ", " << pos.y << ") "
              << "pressure: " << static_cast<int>(pressure) << std::endl;
});
```

### Advanced Launchpad X Features

```cpp
// Cast to specific device type for advanced features
LaunchpadX* launchpad = dynamic_cast<LaunchpadX*>(device.get());
if (launchpad) {
    // Enable programmer mode for full control
    launchpad->enterProgrammerMode();

    // Use built-in patterns
    launchpad->rainbowPattern();
    launchpad->checkerboardPattern(RGBColor::RED, RGBColor::GREEN);

    // Flash and pulse effects
    launchpad->flashPad(PadPosition(3, 3), RGBColor::WHITE, 200);
    launchpad->pulsePad(PadPosition(4, 4), RGBColor::RED);

    // Test all pads
    launchpad->testAllPads();

    // Session mode for standalone operation
    launchpad->enterSessionMode();
    launchpad->setLayout(LaunchpadXConstants::LAYOUT_DRUM_RACK);
    // Device now operates independently
}
```

## Launchpad X Specifics

### Operating Modes

The Launchpad X supports three main operating modes:

1. **Programmer Mode**: Full software control over LEDs and behavior (default)
2. **DAW Mode**: Integration with DAW software
3. **Session Mode**: Standalone operation for live performance

#### Session Mode Features
- Independent clip/sample triggering
- Multiple layout options (Session, Drum Rack, Chromatic, User)
- Scene and track control via side/top buttons
- Real-time layout switching

```cpp
// Session mode usage
launchpad->enterSessionMode();
launchpad->setLayout(LaunchpadXConstants::LAYOUT_CHROMATIC);
if (launchpad->isSessionMode()) {
    // Device operating independently
}
launchpad->exitSessionMode();  // Return to programmer mode
```

### MIDI Note Mapping

#### Grid Pads
- **Range**: Notes 11-88 (8x8 grid)
- **Layout**: Note 11 = bottom-left (0,7), Note 18 = bottom-right (7,7)
- **Pattern**: Each row starts at (previous_row_start + 10)

#### Side Buttons (Right Edge)
- **Notes**: 89, 79, 69, 59, 49, 39, 29, 19 (top to bottom)
- **Mapping**: Button index = (89 - note) / 10

#### Top Buttons
- **Range**: Notes 91-98 (left to right)
- **Mapping**: Button index = note - 91

### SysEx Commands

The Launchpad X uses SysEx messages for advanced control:

```cpp
// Header: F0 00 20 29 02 0C
// Commands:
// - 0E 01: Enter programmer mode
// - 10: Enter DAW mode
// - 03: Set LED color
// - 0A: Reset device
// Footer: F7
```

### Color Palette

The Launchpad X uses a 127-color palette. The implementation provides RGB-to-palette mapping for common colors:

- **0**: Off
- **1-3**: Red (low to full)
- **16-21**: Green (low to full)
- **37-45**: Blue variants
- **13**: Yellow
- **37**: Cyan
- **53**: Magenta

## Testing

### Test Programs

1. **`test_launchpadx_device`**: Comprehensive device abstraction test
   - Device capability detection
   - Color pattern demonstrations
   - Event callback testing
   - Feature showcase

2. **`test_launchpad`**: Legacy test for direct MIDI communication
   - Basic MIDI I/O testing
   - Manual device connection

### Running Tests

```bash
# Build the project
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# Run Launchpad X device test
./bin/test_launchpadx_device

# Run legacy Launchpad test
./bin/test_launchpad
```

## Future Extensions

### Planned Device Support

1. **Launchpad Pro**: Enhanced version with pressure-sensitive pads and display
2. **Launchpad Mini**: Simplified version with basic LED support
3. **Generic Grid Controllers**: Support for other 8x8 MIDI controllers

### Planned Features

1. **Animation System**: Built-in animations and transitions
2. **Pattern Templates**: Pre-built visual patterns for common use cases
3. **Multi-Device Support**: Managing multiple devices simultaneously
4. **Device Auto-Discovery**: Automatic detection and setup of connected devices
5. **Custom Color Palettes**: User-defined color mappings

### Integration Points

The device abstraction system is designed to integrate with:

1. **Synthesizer Engine**: MIDI input drives audio synthesis
2. **Patch System**: Visual feedback for patch states
3. **Sequencer**: Grid-based step sequencing
4. **Performance Tools**: Live performance interfaces

## Implementation Notes

### Thread Safety

- Device operations are not inherently thread-safe
- MIDI callbacks may be called from different threads
- Users should implement appropriate synchronization if needed

### Memory Management

- Uses smart pointers for automatic memory management
- RAII principles for device connection lifecycle
- No manual cleanup required for normal usage

### Error Handling

- Connection failures return false/null gracefully
- Invalid operations (e.g., out-of-bounds coordinates) are handled safely
- Debug output available via compilation flags

### Platform Compatibility

- Built on existing platform MIDI infrastructure
- Windows: Uses WinMM MIDI system
- Linux: Uses ALSA MIDI system
- Color mapping optimized for Launchpad hardware characteristics
