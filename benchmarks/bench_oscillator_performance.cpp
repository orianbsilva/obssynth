/**
 * @file bench_oscillator_performance.cpp
 * @brief Oscillator performance benchmark for cross-platform comparison
 *
 * Measures waveform generation performance across all oscillator types.
 * This benchmark helps identify:
 * - Fastest waveform type
 * - SIMD optimization effectiveness
 * - Real-time capability at various buffer sizes
 *
 * Run with:
 *   ./build/bin/bench_oscillator_performance
 */

#include "../src/lib/oscillators/oscillator.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <iomanip>

using namespace synth;

// Platform detection
#if defined(_WIN32)
#define PLATFORM_NAME "Windows"
#elif defined(__linux__)
#define PLATFORM_NAME "Linux"
#elif defined(__APPLE__)
#define PLATFORM_NAME "macOS"
#else
#define PLATFORM_NAME "Unknown"
#endif

struct BenchmarkConfig
{
    int buffer_size;
    int iterations;
    float sample_rate;
    float frequency;
};

class OscillatorBenchmark
{
private:
    BenchmarkConfig config_;

    double benchmarkWaveType(WaveType type, const char *name)
    {
        Oscillator osc(config_.frequency, type, config_.sample_rate);
        auto buffer = std::make_unique<float[]>(config_.buffer_size);

        // Warmup
        for (int i = 0; i < 10; ++i)
        {
            osc.generateBuffer(buffer.get(), config_.buffer_size);
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < config_.iterations; ++i)
        {
            osc.generateBuffer(buffer.get(), config_.buffer_size);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double time_per_buffer_us = static_cast<double>(duration_us) / config_.iterations;
        double buffer_duration_us = (static_cast<double>(config_.buffer_size) / static_cast<double>(config_.sample_rate)) * 1000000.0;
        double cpu_percent = (time_per_buffer_us / buffer_duration_us) * 100.0;

        std::cout << std::setw(12) << name << ": "
                  << std::fixed << std::setprecision(2)
                  << std::setw(8) << time_per_buffer_us << " μs/buffer ("
                  << std::setw(5) << cpu_percent << "% CPU)\n";

        return cpu_percent;
    }

public:
    OscillatorBenchmark(int buffer_size = 256, int iterations = 10000)
        : config_{buffer_size, iterations, 48000.0f, 440.0f}
    {
    }

    void run()
    {
        std::cout << "Oscillator Performance Benchmark\n";
        std::cout << "=================================\n";
        std::cout << "Platform: " << PLATFORM_NAME << "\n";
        std::cout << "Buffer Size: " << config_.buffer_size << " samples\n";
        std::cout << "Iterations: " << config_.iterations << "\n";
        std::cout << "Sample Rate: " << config_.sample_rate << " Hz\n";
        std::cout << "Frequency: " << config_.frequency << " Hz\n";
        std::cout << "---\n\n";

        std::cout << "Waveform Generation:\n";
        double sine_cpu = benchmarkWaveType(WaveType::SINE, "Sine");
        double saw_cpu = benchmarkWaveType(WaveType::SAW, "Sawtooth");
        double square_cpu = benchmarkWaveType(WaveType::SQUARE, "Square");
        double triangle_cpu = benchmarkWaveType(WaveType::TRIANGLE, "Triangle");

        std::cout << "\n---\n";
        std::cout << "Average CPU: " << (sine_cpu + saw_cpu + square_cpu + triangle_cpu) / 4.0 << "%\n";

        // Multi-buffer test
        std::cout << "\nBuffer Size Comparison:\n";
        int sizes[] = {64, 128, 256, 512, 1024, 2048};

        for (int size : sizes)
        {
            config_.buffer_size = size;
            config_.iterations = 100000 / (size / 64);

            Oscillator osc(440.0f, WaveType::SINE, 48000.0f);
            auto buffer = std::make_unique<float[]>(size);

            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < config_.iterations; ++i)
            {
                osc.generateBuffer(buffer.get(), size);
            }
            auto end = std::chrono::high_resolution_clock::now();

            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            double samples_per_sec = (static_cast<double>(config_.iterations) * size * 1000000.0) / duration_us;

            std::cout << "Buffer " << std::setw(4) << size << ": "
                      << std::fixed << std::setprecision(1)
                      << samples_per_sec / 1000000.0 << " MSamples/sec\n";
        }

        std::cout << "\n✓ Benchmark complete.\n";
    }
};

int main()
{
    OscillatorBenchmark benchmark;
    benchmark.run();
    return 0;
}
