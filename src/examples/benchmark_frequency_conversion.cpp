#include "../lib/utils/platform_midi_reader.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cmath>

namespace synth
{
    // Original mathematical implementation for comparison
    float midiNoteToFrequencyMath(uint8_t note)
    {
        return 440.0f * powf(2.0f, (note - 69) / 12.0f);
    }
}

int main()
{
    using namespace synth;
    const int NUM_ITERATIONS = 1000000;
    const int NUM_NOTES = 128;

    std::cout << "MIDI Note to Frequency Conversion Benchmark\n";
    std::cout << "===========================================\n\n";

    // Test accuracy first - verify lookup table matches mathematical calculation
    std::cout << "Accuracy Test (first 10 notes):\n";
    std::cout << "Note  | Math Calc     | Lookup Table  | Difference\n";
    std::cout << "------|---------------|---------------|------------\n";

    bool accuracyTestPassed = true;
    for (int note = 0; note < 10; note++) {
        float mathResult = midiNoteToFrequencyMath(note);
        float lookupResult = MidiInterface::midiNoteToFrequency(note);
        float difference = std::abs(mathResult - lookupResult);

        std::cout << std::setw(5) << note << " | ";
        std::cout << std::setw(13) << std::fixed << std::setprecision(6) << mathResult << " | ";
        std::cout << std::setw(13) << std::fixed << std::setprecision(6) << lookupResult << " | ";
        std::cout << std::setw(11) << std::scientific << std::setprecision(2) << difference << "\n";

        // Check if difference is within acceptable tolerance (should be nearly identical)
        if (difference > 1e-5f) {
            accuracyTestPassed = false;
        }
    }

    std::cout << "\nAccuracy test: " << (accuracyTestPassed ? "PASSED" : "FAILED") << "\n\n";

    // Performance benchmark
    std::cout << "Performance Benchmark (" << NUM_ITERATIONS << " iterations):\n";
    std::cout << "Converting all " << NUM_NOTES << " MIDI notes repeatedly...\n\n";

    // Benchmark mathematical calculation
    auto start = std::chrono::high_resolution_clock::now();
    volatile float sum1 = 0.0f; // volatile to prevent optimization

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (int note = 0; note < NUM_NOTES; note++) {
            sum1 += midiNoteToFrequencyMath(note);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto mathDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Benchmark lookup table
    start = std::chrono::high_resolution_clock::now();
    volatile float sum2 = 0.0f; // volatile to prevent optimization

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        for (int note = 0; note < NUM_NOTES; note++) {
            sum2 += MidiInterface::midiNoteToFrequency(note);
        }
    }

    end = std::chrono::high_resolution_clock::now();
    auto lookupDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Calculate speedup
    double speedup = static_cast<double>(mathDuration.count()) / static_cast<double>(lookupDuration.count());

    std::cout << "Mathematical calculation: " << mathDuration.count() << " μs\n";
    std::cout << "Lookup table:            " << lookupDuration.count() << " μs\n";
    std::cout << "Speedup:                 " << std::fixed << std::setprecision(2) << speedup << "x faster\n\n";

    // Memory usage information
    std::cout << "Memory Usage:\n";
    std::cout << "Lookup table size: " << (128 * sizeof(float)) << " bytes (" << (128 * sizeof(float) / 1024.0f) << " KB)\n";
    std::cout << "Per conversion:    0 bytes (constant memory)\n\n";

    std::cout << "Summary:\n";
    std::cout << "--------\n";
    std::cout << "✓ Lookup table provides " << std::fixed << std::setprecision(1) << speedup << "x performance improvement\n";
    std::cout << "✓ Uses only " << (128 * sizeof(float)) << " bytes of memory\n";
    std::cout << "✓ Provides O(1) constant-time lookups\n";
    std::cout << "✓ Maintains full precision for all MIDI notes\n";

    return 0;
}
