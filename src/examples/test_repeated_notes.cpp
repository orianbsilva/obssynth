// Test program to verify repeated note hits and voice stealing
#include <iostream>
#include <thread>
#include <chrono>
#include "oscillators/polyphonic_oscillator.hpp"

using namespace synth;

int main()
{
    const float sample_rate = 48000.0f;
    PolyphonicOscillator poly_osc(WaveType::SINE, sample_rate);

    // Set ADSR with reasonable release time
    std::cout << "Setting ADSR: Attack=0.1s, Decay=0.2s, Sustain=0.7, Release=0.5s\n";
    poly_osc.setADSR(0.1f, 0.2f, 0.7f, 0.5f); // 0.5 second release

    std::cout << "\n=== Test 1: Single note on/off ===\n";
    std::cout << "Starting note 60...\n";
    poly_osc.noteOn(60, 0.8f, 0.0f);
    std::cout << "Active voices: " << poly_osc.getActiveVoiceCount() << std::endl;

    // Hold for 200ms
    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
               .count() < 200)
    {
        [[maybe_unused]] float sample = poly_osc.getSample();
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    std::cout << "Calling noteOff...\n";
    poly_osc.noteOff(60);
    std::cout << "Active voices after noteOff: " << poly_osc.getActiveVoiceCount() << std::endl;

    // Wait for release to complete
    start_time = std::chrono::steady_clock::now();
    while (poly_osc.getActiveVoiceCount() > 0 &&
           std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
                   .count() < 1000)
    {
        for (int i = 0; i < 960; i++)
        { // 20ms worth of samples
            [[maybe_unused]] float sample = poly_osc.getSample();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "Final active voices: " << poly_osc.getActiveVoiceCount() << std::endl;

    std::cout << "\n=== Test 2: Repeated note hits ===\n";
    for (int i = 0; i < 5; i++)
    {
        std::cout << "Hit " << (i + 1) << ": Starting note 60...\n";
        poly_osc.noteOn(60, 0.8f, 0.0f);
        std::cout << "  Active voices: " << poly_osc.getActiveVoiceCount() << std::endl;

        // Hold for 100ms
        start_time = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start_time)
                   .count() < 100)
        {
            [[maybe_unused]] float sample = poly_osc.getSample();
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    }

    std::cout << "After 5 repeated hits, active voices: " << poly_osc.getActiveVoiceCount() << std::endl;

    std::cout << "Calling noteOff...\n";
    poly_osc.noteOff(60);
    std::cout << "Active voices after noteOff: " << poly_osc.getActiveVoiceCount() << std::endl;

    // Wait for voices to finish
    start_time = std::chrono::steady_clock::now();
    int check_count = 0;
    while (poly_osc.getActiveVoiceCount() > 0 && check_count < 30)
    {
        for (int i = 0; i < 2400; i++)
        { // 50ms worth of samples
            [[maybe_unused]] float sample = poly_osc.getSample();
        }
        std::cout << "  " << (check_count * 50) << "ms: Active voices = "
                  << poly_osc.getActiveVoiceCount() << std::endl;
        check_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (poly_osc.getActiveVoiceCount() == 0)
    {
        std::cout << "\n✓ SUCCESS: All voices properly deactivated\n";
        return 0;
    }
    else
    {
        std::cout << "\n✗ FAILURE: " << poly_osc.getActiveVoiceCount()
                  << " voices still active\n";
        return 1;
    }
}
