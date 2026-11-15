#pragma once
namespace synth
{
    constexpr int SAMPLE_RATE = 48000;
    // True stereo output support - oscillators generate stereo samples
    // Audio callbacks receive interleaved stereo frames (L,R,L,R,...)
    // Buffer size represents stereo frames, so total samples = BUFFER_SIZE * CHANNELS
    constexpr int CHANNELS = 2;
    constexpr int BITS_PER_SAMPLE = 16;
    constexpr int BUFFER_SIZE = 2048; // Increased buffer size for better performance
}
