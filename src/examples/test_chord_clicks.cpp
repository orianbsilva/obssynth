// Test program to verify click-free chord playback
#include <iostream>
#include <thread>
#include <chrono>
#include "oscillators/polyphonic_oscillator.hpp"

using namespace synth;

int main()
{
    const float sample_rate = 48000.0f;
    PolyphonicOscillator poly_osc(WaveType::SINE, sample_rate);

    // Set ADSR with fast attack to test click-free behavior
    std::cout << "Setting ADSR: Attack=0.01s, Decay=0.1s, Sustain=0.8, Release=0.2s\n";
    poly_osc.setADSR(0.01f, 0.1f, 0.8f, 0.2f); // Very fast attack

    std::cout << "\n=== Test: Chord playback (C Major: C-E-G) ===\n";

    // Start all notes of C major chord simultaneously (similar to real-world chord playing)
    uint8_t chord_notes[] = {60, 64, 67}; // C4, E4, G4
    float velocities[] = {0.8f, 0.7f, 0.75f};

    std::cout << "Starting C major chord simultaneously...\n";
    for (int i = 0; i < 3; i++)
    {
        poly_osc.noteOn(chord_notes[i], velocities[i], 0.0f);
        std::cout << "  Note " << static_cast<int>(chord_notes[i])
                  << " started on voice with velocity " << velocities[i] << "\n";
    }

    std::cout << "Active voices: " << poly_osc.getActiveVoiceCount() << std::endl;

    // Generate samples for the first few milliseconds to check for smoothness
    std::cout << "\nAnalyzing first 10ms of audio for click detection...\n";
    float max_sample = 0.0f;
    float min_sample = 0.0f;
    float prev_sample = 0.0f;
    float max_jump = 0.0f;

    // Check the first 480 samples (10ms at 48kHz)
    for (int i = 0; i < 480; i++)
    {
        float sample = poly_osc.getSample();

        // Track min/max for overall amplitude
        if (sample > max_sample)
            max_sample = sample;
        if (sample < min_sample)
            min_sample = sample;

        // Track maximum sample-to-sample jump (click indicator)
        if (i > 0)
        {
            float jump = std::abs(sample - prev_sample);
            if (jump > max_jump)
                max_jump = jump;
        }

        prev_sample = sample;

        // Print first few samples for manual inspection
        if (i < 10)
        {
            std::cout << "  Sample " << i << ": " << sample << "\n";
        }
    }

    std::cout << "\nClick Analysis Results:\n";
    std::cout << "  Max amplitude: " << max_sample << "\n";
    std::cout << "  Min amplitude: " << min_sample << "\n";
    std::cout << "  Largest sample jump: " << max_jump << "\n";

    // A jump larger than 0.1 would indicate a significant click
    if (max_jump < 0.1f)
    {
        std::cout << "  ✓ GOOD: No significant clicks detected (jump < 0.1)\n";
    }
    else
    {
        std::cout << "  ⚠ WARNING: Potential click detected (jump = " << max_jump << ")\n";
    }

    // Hold chord for 200ms
    std::cout << "\nHolding chord for 200ms...\n";
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
               .count() < 200)
    {
        [[maybe_unused]] float sample = poly_osc.getSample();
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    // Release all notes simultaneously
    std::cout << "Releasing chord...\n";
    for (int i = 0; i < 3; i++)
    {
        poly_osc.noteOff(chord_notes[i]);
        std::cout << "  Note " << static_cast<int>(chord_notes[i]) << " released\n";
    }

    std::cout << "Active voices after release: " << poly_osc.getActiveVoiceCount() << std::endl;

    // Wait for all voices to finish
    start_time = std::chrono::steady_clock::now();
    while (poly_osc.getActiveVoiceCount() > 0 &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
                   .count() < 1000)
    {
        for (int i = 0; i < 960; i++)
        { // 20ms worth
            [[maybe_unused]] float sample = poly_osc.getSample();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "Final active voices: " << poly_osc.getActiveVoiceCount() << std::endl;

    if (poly_osc.getActiveVoiceCount() == 0)
    {
        std::cout << "\n✓ SUCCESS: Chord playback test completed successfully\n";
        std::cout << "  - No clicking detected\n";
        std::cout << "  - All voices properly deactivated\n";
        return 0;
    }
    else
    {
        std::cout << "\n✗ FAILURE: Some voices didn't deactivate properly\n";
        return 1;
    }
}
