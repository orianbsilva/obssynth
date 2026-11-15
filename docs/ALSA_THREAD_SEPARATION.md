# ALSA Backend Thread Separation

## Overview

The ALSA backend has been refactored to separate the audio callback processing from the hardware playback, resulting in improved performance and reduced audio latency.

## Architecture Changes

### Before (Single Thread)
```
Single Thread:
┌─────────────────────┐
│ Audio Callback      │ ←── User audio generation
├─────────────────────┤
│ Buffer Processing   │ ←── Float to Short conversion
├─────────────────────┤
│ ALSA Device Write   │ ←── Hardware I/O
└─────────────────────┘
```

### After (Dual Thread)
```
Write Thread:                    Playback Thread:
┌─────────────────────┐         ┌─────────────────────┐
│ Audio Callback      │         │                     │
├─────────────────────┤    →    │ ALSA Device Write   │
│ Buffer Processing   │         │                     │
└─────────────────────┘         └─────────────────────┘
       ↓                                 ↑
   Triple Buffer (Lock-free communication)
```

## Benefits

### 1. **Reduced Audio Latency**
- **Write Thread** runs at high frequency (500μs intervals) to minimize audio callback latency
- **Playback Thread** runs at lower frequency (2ms intervals) optimized for hardware I/O
- Separation prevents hardware I/O blocking from affecting audio generation

### 2. **Better Real-time Performance**
- Audio callback processing is isolated from hardware I/O operations
- Reduces the risk of audio dropouts and underruns
- Improved responsiveness for real-time audio applications

### 3. **Enhanced Error Handling**
- Hardware errors (ALSA underruns) don't directly impact audio generation
- Better isolation allows for more robust error recovery
- Separate error handling strategies for different thread responsibilities

### 4. **Improved Scalability**
- Write thread can be prioritized for audio processing
- Playback thread can handle device-specific optimizations
- Better CPU utilization across multiple cores

## Thread Responsibilities

### Write Thread (`writeThreadFunc`)
- **Primary Role**: Audio callback processing and buffer management
- **Frequency**: 500μs intervals (high frequency)
- **Tasks**:
  - Execute user audio callback
  - Fill triple buffer with audio data
  - Convert float samples to 16-bit integers
  - Commit write operations to buffer

### Playback Thread (`playbackThreadFunc`)
- **Primary Role**: Hardware device communication
- **Frequency**: 2ms intervals (optimized for hardware)
- **Tasks**:
  - Read from triple buffer
  - Write audio data to ALSA device
  - Handle hardware errors and underruns
  - Manage device state recovery

## Technical Implementation

### Thread Synchronization
- **Lock-free**: Uses atomic operations in TripleBuffer
- **No blocking**: Threads operate independently without mutexes
- **Memory safe**: Triple buffering prevents data races

### Buffer Management
- **Triple Buffer**: Provides lock-free producer-consumer communication
- **Automatic conversion**: Float-to-short conversion handled in write thread
- **Dynamic sizing**: Buffer length tracking for variable-size audio chunks

### Error Recovery
- **ALSA underrun handling**: Isolated to playback thread
- **Device state management**: Automatic PCM device preparation
- **Graceful degradation**: System continues operating despite hardware issues

## Performance Considerations

### Write Thread Optimization
- Higher priority for audio callback execution
- Minimal sleep time to reduce audio latency
- Focus on computational tasks rather than I/O

### Playback Thread Optimization
- Longer sleep intervals to reduce CPU usage
- Hardware-specific optimizations
- Error handling without affecting audio generation

## Usage

The changes are transparent to existing code. The same `AudioCallback` interface is used:

```cpp
AlsaBackend backend;
backend.init();
backend.setCallback([](float* buffer, int size, int* produced) {
    // Your audio generation code here
    *produced = size;
});
backend.start(); // Now starts both threads
```

## Monitoring and Debugging

The separated architecture provides better visibility into system performance:
- Write thread issues affect audio generation quality
- Playback thread issues affect hardware communication
- Independent error reporting for each subsystem

This separation makes it easier to identify and resolve audio performance issues.
