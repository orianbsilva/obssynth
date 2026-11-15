#include "../app/common/audio_constants.hpp"
#include "../lib/effects/gain.hpp"
#include "../lib/effects/overdrive.hpp"
#include "../lib/effects/mixer.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <vector>

using namespace synth;
using namespace examples;

int main()
{
    PlatformUtils::initialize();

    std::cout << "Effects SIMD Performance Test\n";
    std::cout << "=============================\n\n";

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
    std::cout << "✓ SIMD (SSE2) is ENABLED for effects\n";
#else
    std::cout << "✗ SIMD (SSE2) is DISABLED for effects\n";
#endif

    const int SAMPLE_COUNT = 6000; // DirectSound buffer size
    const int ITERATIONS = 1000;

    // Create test data
    auto input_buffer = std::make_unique<float[]>(SAMPLE_COUNT);
    auto output_buffer = std::make_unique<float[]>(SAMPLE_COUNT);

    // Fill with test sine wave
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        input_buffer[i] = sinf(2.0f * 3.14159f * 440.0f * i / 48000.0f) * 0.5f;
    }

    std::cout << "\n1. Testing Gain Effect\n";
    std::cout << "======================\n";

    Gain gain_effect(0.8f);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
    {
        gain_effect.process(input_buffer.get(), output_buffer.get(), SAMPLE_COUNT);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto gain_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Gain processing time: " << gain_time << " μs (" << (gain_time / static_cast<double>(ITERATIONS)) << " μs per buffer)\n";

    std::cout << "\n2. Testing Overdrive Effect\n";
    std::cout << "============================\n";

    Overdrive overdrive_effect(2.0f, 0.8f, 0.5f);

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
    {
        overdrive_effect.process(input_buffer.get(), output_buffer.get(), SAMPLE_COUNT);
    }
    end = std::chrono::high_resolution_clock::now();

    auto overdrive_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Overdrive processing time: " << overdrive_time << " μs (" << (overdrive_time / static_cast<double>(ITERATIONS)) << " μs per buffer)\n";

    std::cout << "\n3. Testing Mixer Effect\n";
    std::cout << "========================\n";

    Mixer mixer(4, 0.25f); // 4 channels, each at 25%

    // Create multiple input buffers
    auto input1 = std::make_unique<float[]>(SAMPLE_COUNT);
    auto input2 = std::make_unique<float[]>(SAMPLE_COUNT);
    auto input3 = std::make_unique<float[]>(SAMPLE_COUNT);
    auto input4 = std::make_unique<float[]>(SAMPLE_COUNT);

    // Fill with different frequencies
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        input1[i] = sinf(2.0f * 3.14159f * 220.0f * i / 48000.0f) * 0.3f; // A3
        input2[i] = sinf(2.0f * 3.14159f * 330.0f * i / 48000.0f) * 0.3f; // E4
        input3[i] = sinf(2.0f * 3.14159f * 440.0f * i / 48000.0f) * 0.3f; // A4
        input4[i] = sinf(2.0f * 3.14159f * 550.0f * i / 48000.0f) * 0.3f; // C#5
    }

    std::vector<const float *> inputs = {input1.get(), input2.get(), input3.get(), input4.get()};

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
    {
        mixer.process(inputs, output_buffer.get(), SAMPLE_COUNT);
    }
    end = std::chrono::high_resolution_clock::now();

    auto mixer_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "Mixer processing time: " << mixer_time << " μs (" << (mixer_time / static_cast<double>(ITERATIONS)) << " μs per buffer)\n";

    // Real-time analysis
    double buffer_duration_ms = (static_cast<double>(SAMPLE_COUNT) / 48000.0) * 1000.0;

    std::cout << "\nReal-time Analysis:\n";
    std::cout << "===================\n";
    std::cout << "Buffer duration: " << buffer_duration_ms << " ms\n";

    double gain_cpu = ((gain_time / static_cast<double>(ITERATIONS)) / 1000.0) / buffer_duration_ms * 100.0;
    double overdrive_cpu = ((overdrive_time / static_cast<double>(ITERATIONS)) / 1000.0) / buffer_duration_ms * 100.0;
    double mixer_cpu = ((mixer_time / static_cast<double>(ITERATIONS)) / 1000.0) / buffer_duration_ms * 100.0;

    std::cout << "Gain CPU usage: " << gain_cpu << "%\n";
    std::cout << "Overdrive CPU usage: " << overdrive_cpu << "%\n";
    std::cout << "Mixer CPU usage: " << mixer_cpu << "%\n";

    double total_cpu = gain_cpu + overdrive_cpu + mixer_cpu;
    std::cout << "Total effects CPU usage: " << total_cpu << "%\n";

    if (total_cpu < 5.0)
    {
        std::cout << "✓ EXCELLENT: Effects are very efficient for real-time audio\n";
    }
    else if (total_cpu < 10.0)
    {
        std::cout << "✓ GOOD: Effects are suitable for real-time audio\n";
    }
    else
    {
        std::cout << "⚠ WARNING: Effects may be too CPU-intensive for real-time audio\n";
    }

    // Verify correctness
    std::cout << "\nCorrectness Test:\n";
    std::cout << "=================\n";

    // Test gain effect
    float test_input = 0.5f;
    float test_output;
    gain_effect.process(&test_input, &test_output, 1);
    std::cout << "Gain: " << test_input << " * 0.8 = " << test_output << " (expected: " << (test_input * 0.8f) << ")\n";

    // Test mixer
    std::vector<const float *> test_inputs = {&test_input, &test_input};
    mixer.set_input_count(2);
    mixer.set_gain(0, 0.5f);
    mixer.set_gain(1, 0.3f);
    mixer.process(test_inputs, &test_output, 1);
    std::cout << "Mixer: " << test_input << " * 0.5 + " << test_input << " * 0.3 = " << test_output
              << " (expected: " << (test_input * 0.5f + test_input * 0.3f) << ")\n";

    PlatformUtils::cleanup();
    return 0;
}
