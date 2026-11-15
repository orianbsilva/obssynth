#include "../app/common/audio_constants.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include <iostream>
#include <memory>
#include <chrono>

using namespace synth;

int main()
{
    std::cout << "SIMD Detection Test\n";
    std::cout << "==================\n\n";

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
    std::cout << "✓ SIMD (SSE2) is ENABLED for Windows\n";
#ifdef _MSC_VER
    std::cout << "  - Detected: MSVC compiler\n";
#ifdef _M_X64
    std::cout << "  - Architecture: x64 (SSE2 guaranteed)\n";
#elif defined(_M_IX86_FP)
    std::cout << "  - Architecture: x86 with FP level " << _M_IX86_FP << "\n";
#endif
#endif
#ifdef __SSE2__
    std::cout << "  - Detected: __SSE2__ macro defined\n";
#endif
#else
    std::cout << "✗ SIMD (SSE2) is DISABLED\n";
#endif

    std::cout << "\nFloat-to-Short Conversion Performance Test\n";
    std::cout << "==========================================\n";

    const int SAMPLE_COUNT = 48000; // 1 second at 48kHz
    const int ITERATIONS = 1000;

    // Create test data
    auto floatBuffer = std::make_unique<float[]>(SAMPLE_COUNT);
    auto shortBuffer = std::make_unique<short[]>(SAMPLE_COUNT);

    // Fill with test sine wave
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        floatBuffer[i] = sinf(2.0f * 3.14159f * 440.0f * i / 48000.0f) * 0.8f;
    }

    // Performance test
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
    {
        PolyphonicOscillator::convertAndQuantize(floatBuffer.get(), shortBuffer.get(), SAMPLE_COUNT);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_time = static_cast<double>(duration) / ITERATIONS;

    std::cout << "Performance Results:\n";
    std::cout << "- Sample count: " << SAMPLE_COUNT << " (1 second of audio)\n";
    std::cout << "- Iterations: " << ITERATIONS << "\n";
    std::cout << "- Total time: " << duration << " μs\n";
    std::cout << "- Average time per conversion: " << avg_time << " μs\n";
    std::cout << "- Samples per second: " << (SAMPLE_COUNT / (avg_time / 1000000.0)) << "\n";

    // Real-time analysis
    double real_time_requirement = (static_cast<double>(SAMPLE_COUNT) / 48000.0) * 1000000.0; // μs for 1 second of audio
    double cpu_usage_percent = (avg_time / real_time_requirement) * 100.0;

    std::cout << "\nReal-time Analysis:\n";
    std::cout << "- Real-time requirement: " << real_time_requirement << " μs\n";
    std::cout << "- Actual processing time: " << avg_time << " μs\n";
    std::cout << "- CPU usage for conversion: " << cpu_usage_percent << "%\n";

    if (cpu_usage_percent < 1.0)
    {
        std::cout << "✓ EXCELLENT: Conversion is very efficient for real-time audio\n";
    }
    else if (cpu_usage_percent < 5.0)
    {
        std::cout << "✓ GOOD: Conversion is suitable for real-time audio\n";
    }
    else
    {
        std::cout << "⚠ WARNING: Conversion may be too slow for real-time audio\n";
    }

    // Verify correctness (check a few samples)
    std::cout << "\nCorrectness Test:\n";
    std::cout << "- Input samples: " << floatBuffer[0] << ", " << floatBuffer[1000] << ", " << floatBuffer[2000] << "\n";
    std::cout << "- Output samples: " << shortBuffer[0] << ", " << shortBuffer[1000] << ", " << shortBuffer[2000] << "\n";

    return 0;
}
