# Raspberry Pi Audio Fix Documentation

## Problem
You're getting this error on Raspberry Pi:
```
PulseAudio connection failed: Connection refused
Falling back to ALSA...
Successfully opened ALSA device: hw:0,0

🎵 Playing A4 (440 Hz) in different wave forms:

Segmentation fault
```

## Root Causes Fixed

### 1. **TripleBuffer Null Pointer Dereference**
**Problem**: The `buffer_length` was using `unique_ptr<int>` causing null pointer dereferences.
**Fix**: Changed to `atomic<int>` for thread-safe access without pointers.

### 2. **Missing Audio Callback**
**Problem**: Audio sink was running but had no callback set, causing undefined behavior.
**Fix**: Set proper audio callback with oscillator generation.

### 3. **PulseAudio Fallback**
**Problem**: No automatic fallback when PulseAudio fails.
**Fix**: Automatic fallback to ALSA with better device selection.

## Code Changes Made

### TripleBuffer Fix (Critical)
```cpp
// Before: Dangerous pointer usage
std::unique_ptr<int> buffer_length[NUM_BUFFERS];
return *buffer_length[bufferIndex];  // SEGFAULT!

// After: Thread-safe atomic integers
std::atomic<int> buffer_length[NUM_BUFFERS];
return buffer_length[bufferIndex].load();  // Safe!
```

### Audio Callback Fix
```cpp
// Before: No callback, undefined behavior
// Audio threads running with no data generation

// After: Proper callback setup
sink.setAudioCallback([&oscillator](float* buffer, int buffer_size, int* produced) {
    if (oscillator) {
        oscillator->generateBuffer(buffer, buffer_size);
        *produced = buffer_size;
    }
});
```

### Enhanced ALSA Device Selection
```cpp
const char* device_names[] = {
    "hw:0,0",           // First hardware device (bcm2835)
    "hw:0",             // Hardware device 0
    "default",          // System default
    "plughw:0,0",       // Plugin hardware device
    "sysdefault",       // System default fallback
    nullptr
};
```

## Solutions

### Quick Fix (Recommended)
1. Install dependencies:
   ```bash
   sudo apt-get update
   sudo apt-get install -y libasound2-dev libpulse-dev pkg-config cmake build-essential
   ```

2. Add your user to the audio group:
   ```bash
   sudo usermod -aG audio $USER
   ```

3. Reboot or log out/in:
   ```bash
   sudo reboot
   ```

4. Rebuild the project:
   ```bash
   CC=gcc CXX=g++ cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

### Expected Output (Fixed)
```
Available audio output devices:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  [0] bcm2835 Headphones [hw:0]
  [1] Launchkey MK4 49 [hw:1] 
  [2] vc4-hdmi [hw:2]
  [3] Default ALSA Device [default]
  [4] PulseAudio [pulse]
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
PulseAudio connection failed: Connection refused
Falling back to ALSA...
Successfully opened ALSA device: hw:0,0

🎵 Playing A4 (440 Hz) in different wave forms:

Playing Sine wave for 2 seconds...
✓ Sine wave completed
Playing Sawtooth wave for 2 seconds...
✓ Sawtooth wave completed
Playing Square wave for 2 seconds...
✓ Square wave completed

🎵 A4 Pitch Test completed successfully!
```

### Debugging Steps if Still Issues

1. **Check permissions**:
   ```bash
   groups $USER  # Should include 'audio'
   ls -la /dev/snd/  # Should be accessible
   ```

2. **Test basic ALSA**:
   ```bash
   speaker-test -c2 -t wav -D hw:0
   ```

3. **Run with debugging**:
   ```bash
   gdb ./bin/test_a4
   (gdb) run
   # If it crashes: (gdb) bt
   ```

4. **Check audio cards**:
   ```bash
   cat /proc/asound/cards
   aplay -l
   ```

### For Headless Raspberry Pi
The fixes work for both desktop and headless setups. For framebuffer-only (no X11), use:
- `rpi_fb_gl_window.hpp` instead of `linux_gl_window.hpp`
- No X11 dependencies required

## Technical Details

### TripleBuffer Thread Safety
- Uses `atomic<int>` for buffer lengths
- Lock-free read/write operations
- Proper initialization prevents null pointer access

### ALSA Backend Robustness
- Tries multiple device names in order
- Better error messages with troubleshooting tips
- Handles bcm2835 audio chip specifics

### Audio Callback Pattern
- Continuous audio generation through callback
- Thread-safe oscillator access
- Proper buffer management

The segmentation fault should now be completely resolved! 🎵
