# MIDI Channel Aftertouch Implementation

## Overview

This document describes the implementation of MIDI Channel Aftertouch message support in the synthesizer project. Channel aftertouch (also known as channel pressure) is a MIDI message that provides continuous pressure data from MIDI controllers after a key is pressed.

## Technical Details

### MIDI Message Format
- **Status Byte**: `0xD0` + channel number (0-15)
- **Data Byte 1**: Pressure value (0-127)
- **Data Byte 2**: Not used (2-byte message)

### Implementation Changes

#### 1. Header File (`src/utils/platform_midi_reader.hpp`)

**Added MIDI Message Constant:**
```cpp
static const uint8_t MIDI_CHANNEL_AFTERTOUCH = 0xD0;
```

**Added Callback Type Definition:**
```cpp
using MidiChannelAftertouchCallback = std::function<void(uint8_t channel, uint8_t pressure)>;
```

**Added Callback Member Variable:**
```cpp
MidiChannelAftertouchCallback channelAftertouchCallback_;
```

**Added Setter Method:**
```cpp
void setChannelAftertouchCallback(MidiChannelAftertouchCallback callback) { channelAftertouchCallback_ = callback; }
```

#### 2. Implementation File (`src/utils/platform_midi_reader.cpp`)

**Added Parsing Logic in `parseMidiMessage()`:**
```cpp
case MIDI_CHANNEL_AFTERTOUCH:
    if (channelAftertouchCallback_)
        channelAftertouchCallback_(channel, data1);
    break;
```

**Updated Linux MIDI Parsing:**
Updated the message completeness check to handle 2-byte channel aftertouch messages (like program change):
```cpp
if (bufferPos >= 3 || (bufferPos >= 2 && ((buffer[0] & 0xF0) == MIDI_PROGRAM || (buffer[0] & 0xF0) == MIDI_CHANNEL_AFTERTOUCH)))
```

## Usage Example

```cpp
#include "platform_midi_reader.hpp"

void channelAftertouchCallback(uint8_t channel, uint8_t pressure)
{
    std::cout << "Channel Aftertouch - Channel: " << (int)channel
              << ", Pressure: " << (int)pressure << std::endl;
}

int main()
{
    MidiReader reader;

    // Set up the callback
    reader.setChannelAftertouchCallback(channelAftertouchCallback);

    // Initialize and open MIDI device
    reader.init();
    reader.open();

    // Channel aftertouch messages will now trigger the callback

    return 0;
}
```

## Test Programs

### 1. `test_channel_aftertouch.cpp`
Interactive test program that:
- Sets up channel aftertouch callback
- Opens MIDI device or enables keyboard fallback
- Runs for 30 seconds to receive aftertouch messages
- Demonstrates real-world usage

### 2. `test_channel_aftertouch_parsing.cpp`
Verification test that:
- Confirms all implementation components are present
- Validates the API is working correctly
- Provides implementation status report

## Building and Testing

```bash
# Build all projects
make -C build

# Run the interactive test
./build/bin/test_channel_aftertouch

# Run the verification test
./build/bin/test_channel_aftertouch_parsing
```

## MIDI Controller Support

Channel aftertouch is supported by many professional MIDI controllers, including:
- Keyboard controllers with pressure-sensitive keys
- Pad controllers with aftertouch support
- Synthesizer keyboards with channel pressure

## Notes

- Channel aftertouch provides continuous pressure data (0-127)
- Unlike polyphonic aftertouch, channel aftertouch applies to the entire channel
- The implementation follows MIDI 1.0 specification
- Cross-platform support (Windows WinMM API and Linux ALSA)
- Thread-safe callback mechanism

## Files Modified

1. `src/utils/platform_midi_reader.hpp` - Added callback type, member variable, and setter method
2. `src/utils/platform_midi_reader.cpp` - Added parsing logic and Linux message handling
3. `examples/CMakeLists.txt` - Added new test programs
4. `examples/test_channel_aftertouch.cpp` - Interactive test program
5. `examples/test_channel_aftertouch_parsing.cpp` - Verification test program

## Compliance

The implementation follows the project's coding guidelines:
- Uses `float` for all floating-point values
- Private member variables suffixed with underscore (`_`)
- Uses lowercase `synth` namespace
- Modern C++17 coding practices
- No extern "C" wrappers
