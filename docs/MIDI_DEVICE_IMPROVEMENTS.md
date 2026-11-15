# MIDI Device Handling Improvements

## Summary of Changes

This document describes the improvements made to `platform_midi_reader.cpp` and `platform_midi_reader.hpp` to fix issues with MIDI devices not responding after application restart.

## Problem

After closing and relaunching the application, MIDI devices would not send any data until a device and machine reboot. This was caused by:

1. Incomplete cleanup of MIDI device resources
2. Stale data remaining in device buffers
3. Lack of proper device reset and probing
4. Poor error handling in the MIDI thread

## Changes Made

### 1. Enhanced Device Cleanup (`close()` method)

**Windows:**
- Added `midiInReset()` call to clear any pending messages before closing
- Ensures complete device state reset

**Linux:**
- Added `drainMidiDevice()` call to flush any stale MIDI data
- Proper thread synchronization before device closure

### 2. Improved Device Opening (`open()` method)

**Windows:**
- Added `midiInReset()` call immediately after opening device
- Added device probing to verify connection is working
- Better error handling with detailed error messages

**Linux:**
- Tries multiple subdevice numbers (0-3) if initial open fails
- Drains device after opening to clear stale data
- Enhanced error reporting with ALSA error strings
- Added device probing to verify connection

### 3. New Device Management Methods

#### `drainMidiDevice()`
- **Linux**: Reads and discards any pending MIDI data from device buffers
- **Windows**: No-op (handled by `midiInReset()`)
- Prevents stale data from previous sessions affecting new connections

#### `probeMidiDevice()`
- **Windows**: Verifies device handle is valid
- **Linux**: Tests device info and poll descriptor setup
- Confirms device is ready for input/output operations

#### `reconnect(deviceId)`
- Forces complete disconnection and reconnection
- Includes delay to ensure clean disconnection
- Useful for recovering from stale connections

#### `testMidiConnection()`
- Tests if current MIDI connection is still active
- **Windows**: Checks device capabilities
- **Linux**: Verifies poll descriptors are working
- Can be called periodically to detect connection issues

### 4. Robust MIDI Thread (Linux)

Enhanced the `midiThreadFunction()` with:

- **Error Counting**: Tracks consecutive errors and stops if too many occur
- **Better Error Handling**: Distinguishes between normal timeouts and actual errors
- **Graceful Degradation**: Continues operation through temporary issues
- **Improved Logging**: Detailed error messages for debugging

### 5. Additional Improvements

- Added missing includes (`<cerrno>`, `<cstring>`) for proper error handling
- Enhanced error messages with system error descriptions
- Better thread lifecycle management
- Improved documentation and code comments

## API Changes

### New Public Methods

```cpp
// Force reconnection to MIDI device
int reconnect(int deviceId = 0);

// Test if MIDI device is still responding
bool testMidiConnection();
```

### New Private Methods

```cpp
// Drain any pending MIDI data from device
void drainMidiDevice();

// Probe MIDI device to ensure it's responding
bool probeMidiDevice();
```

## Usage Examples

### Basic Usage with Improved Reliability

```cpp
MidiReader midi;
midi.init();

// Open with enhanced error handling
if (midi.open(0) == 0) {
    // Test connection is working
    if (midi.testMidiConnection()) {
        std::cout << "MIDI device ready" << std::endl;
    }
}
```

### Handling Stale Connections

```cpp
// If MIDI stops responding, force reconnection
if (!midi.testMidiConnection()) {
    std::cout << "Connection stale, reconnecting..." << std::endl;
    midi.reconnect(0);
}
```

### Robust Application Restart

```cpp
// In application shutdown
midi.close(); // Now includes proper cleanup

// In application startup (even immediate restart)
midi.open(0); // Now includes device reset and probing
```

## Benefits

1. **Eliminates Device Hang**: Proper cleanup prevents device from remaining in stale state
2. **Faster Recovery**: No need for device/machine reboot after application restart
3. **Better Diagnostics**: Enhanced error reporting helps identify connection issues
4. **Robust Operation**: Improved error handling prevents thread crashes
5. **Connection Monitoring**: Can detect and recover from connection failures

## Testing

A test program (`test_midi_reconnect.cpp`) was created to verify the improvements:

- Tests device opening and connection verification
- Demonstrates reconnection functionality
- Shows proper fallback to keyboard mode when no MIDI devices available
- Validates connection testing methods

## Platform Compatibility

- **Windows**: Uses WinMM API with proper device reset
- **Linux**: Uses ALSA with enhanced error handling and device draining
- **Fallback**: Keyboard mode works on both platforms when MIDI unavailable

These changes ensure reliable MIDI device operation across application restarts without requiring system reboots.
