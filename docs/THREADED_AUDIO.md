# Threaded Audio Sink with Triple Buffering

This document explains the improvements made to the audio sink implementation.

## What Changed

### Before (Synchronous)
- **Single Buffer**: Used one audio buffer at a time
- **Blocking Playback**: Each `playBuffer()` call blocked until the buffer finished playing
- **Manual Loop**: Applications had to manually loop to provide continuous audio
- **Thread Blocking**: Audio generation happened in the main thread, causing potential interruptions

### After (Threaded with Triple Buffering)
- **Triple Buffer**: Uses three buffers to avoid locking between threads
- **Continuous Playback**: Audio thread runs independently, ensuring uninterrupted playback
- **Callback-Based**: Applications provide a callback function that's called automatically
- **Lock-Free**: No mutex or critical sections needed between audio generation and playback

## Key Improvements

### 1. Triple Buffer Architecture
```
Buffer 0: [Writing] ← Audio generation thread writes here
Buffer 1: [Pending] ← Ready to be played next
Buffer 2: [Playing] ← Currently being played by audio hardware
```

The buffers rotate automatically:
- **Write Buffer**: Where new audio data is generated
- **Pending Buffer**: Converted and ready for playback
- **Read Buffer**: Currently being played by the audio hardware

### 2. Lock-Free Design
- Uses atomic operations for buffer index management
- No mutex locks that could cause audio dropouts
- Each thread works on different buffers simultaneously

### 3. Continuous Audio Generation
- Audio callback runs in a separate thread
- Maintains consistent audio stream without gaps
- Prevents underruns that cause audio artifacts

## Usage Examples

### New Threaded Approach (Recommended)
```cpp
AudioSink sink;
sink.init();

// Set callback for continuous audio generation
sink.setAudioCallback([&](float* buffer, int size) {
    oscillator.generateBuffer(buffer, size);
});

sink.start(); // Audio now plays continuously
// ... do other work
sink.stop();  // Stops gracefully
```

### Legacy Approach (Still Supported)
```cpp
AudioSink sink;
sink.init();
sink.start();

while (playing) {
    oscillator.generateBuffer(buffer, size);
    sink.playBuffer(buffer, size); // Works but less efficient
}
sink.stop();
```

## Benefits

1. **Lower Latency**: Triple buffering reduces the chance of audio dropouts
2. **Better Performance**: Audio generation doesn't block the main thread
3. **Smooth Playback**: Continuous audio stream without gaps
4. **Scalability**: Main thread is free for UI, MIDI input, and other tasks
5. **Professional Quality**: Meets standards for real-time audio applications

## Technical Details

- **Buffer Size**: 4096 samples (default)
- **Sample Rate**: 44.1 kHz
- **Channels**: 2 (Stereo)
- **Bit Depth**: 16-bit signed integer output
- **Internal Format**: 32-bit float for processing

The implementation automatically converts from 32-bit float (used internally) to 16-bit signed integers (required by Windows audio API) with proper clamping to prevent clipping.
