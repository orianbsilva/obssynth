/**
 * @file tvoid testPerformance()
{
    const int BUFFER_SIZE = 512;  // Updated DirectSound buffer size
    const int CHANNELS = 2;
    const int ITERATIONS = 100;   // Number of test iterationserformance_comparison.cpp
 * @brief Performance comparison test for optimized vs legacy polyphonic oscillator methods
 *
 * This test measures the performance improvement from block-level locking
 * versus per-sample locking in the polyphonic oscillator.
 */

#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include <chrono>
#include <iostream>
#include <memory>

using namespace synth;

void testPerformance()
{
    const int BUFFER_SIZE = 6000; // Typical DirectSound buffer size
    const int CHANNELS = 2;
    const int ITERATIONS = 100; // Number of test iterations

    // Initialize polyphonic oscillator
    PolyphonicOscillator poly_osc(WaveType::SINE, 48000.0f);

    // Activate some voices for realistic test
    poly_osc.noteOn(60, 0.8f); // C4
    poly_osc.noteOn(64, 0.7f); // E4
    poly_osc.noteOn(67, 0.6f); // G4
    poly_osc.noteOn(72, 0.5f); // C5

    std::cout << "Performance Comparison Test\n";
    std::cout << "===========================\n";
    std::cout << "Buffer size: " << BUFFER_SIZE << " samples per channel\n";
    std::cout << "Channels: " << CHANNELS << " (stereo)\n";
    std::cout << "Active voices: " << poly_osc.getActiveVoiceCount() << "/" << 8 << "\n";
    std::cout << "Iterations: " << ITERATIONS << "\n\n";

    // Allocate buffers
    std::unique_ptr<float[]> mono_buffer = std::make_unique<float[]>(BUFFER_SIZE);
    std::unique_ptr<float[]> stereo_buffer = std::make_unique<float[]>(BUFFER_SIZE * CHANNELS);

    // Test 1: Legacy per-sample method (getSample() in loop)
    std::cout << "Testing legacy per-sample method...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        for (int i = 0; i < BUFFER_SIZE; ++i)
        {
            mono_buffer[i] = poly_osc.getSample();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto legacy_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::cout << "Legacy method time: " << legacy_duration.count() << " microseconds\n";

    // Test 2: Optimized block-level method
    std::cout << "Testing optimized block-level method...\n";
    start_time = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        poly_osc.generateBufferOptimized(mono_buffer.get(), BUFFER_SIZE);
    }

    end_time = std::chrono::high_resolution_clock::now();
    auto optimized_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::cout << "Optimized method time: " << optimized_duration.count() << " microseconds\n\n";

    // Test 3: Legacy stereo per-sample method
    std::cout << "Testing legacy stereo per-sample method...\n";
    start_time = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        for (int i = 0; i < BUFFER_SIZE; ++i)
        {
            StereoSample sample = poly_osc.getStereoSample();
            stereo_buffer[i * 2] = sample.left;
            stereo_buffer[i * 2 + 1] = sample.right;
        }
    }

    end_time = std::chrono::high_resolution_clock::now();
    auto legacy_stereo_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::cout << "Legacy stereo method time: " << legacy_stereo_duration.count() << " microseconds\n";

    // Test 4: Optimized stereo block-level method
    std::cout << "Testing optimized stereo block-level method...\n";
    start_time = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        poly_osc.generateStereoBufferOptimized(stereo_buffer.get(), BUFFER_SIZE);
    }

    end_time = std::chrono::high_resolution_clock::now();
    auto optimized_stereo_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::cout << "Optimized stereo method time: " << optimized_stereo_duration.count() << " microseconds\n\n";

    // Calculate performance improvements
    float mono_improvement = static_cast<float>(legacy_duration.count()) / optimized_duration.count();
    float stereo_improvement = static_cast<float>(legacy_stereo_duration.count()) / optimized_stereo_duration.count();

    std::cout << "Performance Results:\n";
    std::cout << "===================\n";
    std::cout << "Mono performance improvement: " << mono_improvement << "x faster\n";
    std::cout << "Stereo performance improvement: " << stereo_improvement << "x faster\n";

    // Estimate real-time performance
    float samples_per_second = 48000.0f;
    float buffer_duration_ms = (BUFFER_SIZE / samples_per_second) * 1000.0f;
    float optimized_buffer_time_ms = optimized_stereo_duration.count() / 1000.0f / ITERATIONS;
    float cpu_usage_percent = (optimized_buffer_time_ms / buffer_duration_ms) * 100.0f;

    std::cout << "\nReal-time Performance Estimate:\n";
    std::cout << "==============================\n";
    std::cout << "Buffer duration: " << buffer_duration_ms << " ms\n";
    std::cout << "Processing time per buffer: " << optimized_buffer_time_ms << " ms\n";
    std::cout << "Estimated CPU usage: " << cpu_usage_percent << "%\n";

    if (cpu_usage_percent < 50.0f)
    {
        std::cout << "✓ REAL-TIME CAPABLE - Low latency audio achievable\n";
    }
    else if (cpu_usage_percent < 80.0f)
    {
        std::cout << "⚠ MARGINAL - May work for real-time with optimizations\n";
    }
    else
    {
        std::cout << "✗ NOT REAL-TIME CAPABLE - Too much CPU usage\n";
    }

    // Test conversion performance
    std::cout << "\nTesting float-to-short conversion performance...\n";
    std::unique_ptr<short[]> short_buffer = std::make_unique<short[]>(BUFFER_SIZE * CHANNELS);

    start_time = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        PolyphonicOscillator::convertAndQuantize(stereo_buffer.get(), short_buffer.get(), BUFFER_SIZE * CHANNELS);
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto conversion_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    float conversion_time_ms = conversion_duration.count() / 1000.0f / ITERATIONS;
    std::cout << "Conversion time per buffer: " << conversion_time_ms << " ms\n";

    poly_osc.allNotesOff();
}

int main()
{
    try
    {
        testPerformance();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
