// Test program to verify ADSR release phase behavior
#include <iostream>
#include <thread>
#include <chrono>
#include "oscillators/polyphonic_oscillator.hpp"

using namespace synth;

int main()
{
    const float sample_rate = 48000.0f;
    PolyphonicOscillator poly_osc(WaveType::SINE, sample_rate);

    // Set ADSR with long release time to test
    std::cout << "Setting ADSR: Attack=0.1s, Decay=0.2s, Sustain=0.7, Release=1.5s\n";
    poly_osc.setADSR(0.1f, 0.2f, 0.7f, 1.5f); // 1.5 second release

    std::cout << "Starting note 60 (Middle C)...\n";
    poly_osc.noteOn(60, 0.8f, 0.0f); // Note on

    std::cout << "Holding note for 0.5 seconds (through attack and part of decay)...\n";
    auto start_time = std::chrono::steady_clock::now();

    // Hold for 0.5 seconds
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start_time)
               .count() < 500)
    {

        [[maybe_unused]] float sample = poly_osc.getSample();
        std::this_thread::sleep_for(std::chrono::microseconds(20)); // ~50kHz sampling
    }

    std::cout << "Active voices before noteOff: " << poly_osc.getActiveVoiceCount() << std::endl;

    std::cout << "Calling noteOff() - should start ADSR release phase...\n";
    poly_osc.noteOff(60); // Start release phase

    std::cout << "Active voices after noteOff: " << poly_osc.getActiveVoiceCount() << std::endl;

    // Continue generating samples for release phase
    start_time = std::chrono::steady_clock::now();
    int check_count = 0;

    std::cout << "Monitoring voice during release phase:\n";
    while (poly_osc.getActiveVoiceCount() > 0 && check_count < 60)
    { // Max 3 seconds
        // Generate samples for 50ms
        for (int i = 0; i < 2400; i++)
        { // 50ms at 48kHz
            [[maybe_unused]] float sample = poly_osc.getSample();
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start_time)
                           .count();

        std::cout << "  " << elapsed << "ms: Active voices = "
                  << poly_osc.getActiveVoiceCount() << std::endl;

        check_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (poly_osc.getActiveVoiceCount() == 0)
    {
        auto total_release_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - start_time)
                                      .count();

        std::cout << "\n✓ SUCCESS: Voice deactivated after " << total_release_time
                  << "ms of release phase\n";
        std::cout << "Expected release time was 1500ms, actual was " << total_release_time << "ms\n";

        if (total_release_time > 1200)
        { // Allow some tolerance
            std::cout << "✓ ADSR release phase is working correctly!\n";
            return 0;
        }
        else
        {
            std::cout << "✗ Release phase too short - may still have crossfade issue\n";
            return 1;
        }
    }
    else
    {
        std::cout << "✗ TIMEOUT: Voice still active after 3 seconds\n";
        return 1;
    }
}
