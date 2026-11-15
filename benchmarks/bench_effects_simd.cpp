/**
 * @file bench_effects_simd.cpp
 * @brief SIMD performance benchmark for audio effects
 *
 * Measures the performance of SIMD-optimized effects processing.
 * Use this benchmark to compare performance across platforms:
 * - Windows (MSVC SSE2)
 * - Linux (GCC/Clang SSE2/AVX)
 * - ARM (NEON when available)
 *
 * Run with:
 *   ./build/bin/bench_effects_simd
 *
 * Expected output format (for cross-platform comparison):
 *   Platform: [Windows|Linux|macOS]
 *   SIMD: [SSE2|AVX|NEON|None]
 *   Buffer Size: N samples
 *   Iterations: M
 *   ---
 *   Gain: X.XX μs/buffer (Y.YY% CPU)
 *   Overdrive: X.XX μs/buffer (Y.YY% CPU)
 *   Mixer: X.XX μs/buffer (Y.YY% CPU)
 */

#include "../src/app/common/audio_constants.hpp"
#include "../src/lib/effects/gain.hpp"
#include "../src/lib/effects/overdrive.hpp"
#include "../src/lib/effects/mixer.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <cmath>

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

// SIMD detection
#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#define SIMD_TYPE "SSE2"
#elif defined(__ARM_NEON)
#define SIMD_TYPE "NEON"
#else
#define SIMD_TYPE "None"
#endif

struct BenchmarkResult
{
    std::string name;
    double time_us;
    double cpu_percent;
};

class EffectsBenchmark
{
private:
    int buffer_size_;
    int iterations_;
    float sample_rate_;
    std::vector<BenchmarkResult> results_;

public:
    EffectsBenchmark(int buffer_size = 6000, int iterations = 1000)
        : buffer_size_(buffer_size), iterations_(iterations), sample_rate_(48000.0f)
    {
    }

    void printHeader()
    {
        std::cout << "Effects SIMD Benchmark\n";
        std::cout << "======================\n";
        std::cout << "Platform: " << PLATFORM_NAME << "\n";
        std::cout << "SIMD: " << SIMD_TYPE << "\n";
        std::cout << "Buffer Size: " << buffer_size_ << " samples\n";
        std::cout << "Iterations: " << iterations_ << "\n";
        std::cout << "Sample Rate: " << sample_rate_ << " Hz\n";
        std::cout << "---\n";
    }

    void benchmarkGain()
    {
        auto input = std::make_unique<float[]>(buffer_size_);
        auto output = std::make_unique<float[]>(buffer_size_);

        // Fill with test sine wave
        for (int i = 0; i < buffer_size_; ++i)
        {
            input[i] = sinf(2.0f * 3.14159f * 440.0f * i / sample_rate_) * 0.5f;
        }

        Gain gain_effect(0.8f);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations_; ++i)
        {
            gain_effect.process(input.get(), output.get(), buffer_size_);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double time_per_buffer = static_cast<double>(duration) / iterations_;
        double buffer_duration_ms = (static_cast<double>(buffer_size_) / static_cast<double>(sample_rate_)) * 1000.0;
        double cpu_percent = (time_per_buffer / 1000.0) / buffer_duration_ms * 100.0;

        results_.push_back({"Gain", time_per_buffer, cpu_percent});
        std::cout << "Gain: " << time_per_buffer << " μs/buffer (" << cpu_percent << "% CPU)\n";
    }

    void benchmarkOverdrive()
    {
        auto input = std::make_unique<float[]>(buffer_size_);
        auto output = std::make_unique<float[]>(buffer_size_);

        for (int i = 0; i < buffer_size_; ++i)
        {
            input[i] = sinf(2.0f * 3.14159f * 440.0f * i / sample_rate_) * 0.5f;
        }

        Overdrive overdrive_effect(2.0f, 0.8f, 0.5f);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations_; ++i)
        {
            overdrive_effect.process(input.get(), output.get(), buffer_size_);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double time_per_buffer = static_cast<double>(duration) / iterations_;
        double buffer_duration_ms = (static_cast<double>(buffer_size_) / static_cast<double>(sample_rate_)) * 1000.0;
        double cpu_percent = (time_per_buffer / 1000.0) / buffer_duration_ms * 100.0;

        results_.push_back({"Overdrive", time_per_buffer, cpu_percent});
        std::cout << "Overdrive: " << time_per_buffer << " μs/buffer (" << cpu_percent << "% CPU)\n";
    }

    void benchmarkMixer()
    {
        auto input1 = std::make_unique<float[]>(buffer_size_);
        auto input2 = std::make_unique<float[]>(buffer_size_);
        auto input3 = std::make_unique<float[]>(buffer_size_);
        auto input4 = std::make_unique<float[]>(buffer_size_);
        auto output = std::make_unique<float[]>(buffer_size_);

        for (int i = 0; i < buffer_size_; ++i)
        {
            input1[i] = sinf(2.0f * 3.14159f * 220.0f * i / sample_rate_) * 0.3f;
            input2[i] = sinf(2.0f * 3.14159f * 330.0f * i / sample_rate_) * 0.3f;
            input3[i] = sinf(2.0f * 3.14159f * 440.0f * i / sample_rate_) * 0.3f;
            input4[i] = sinf(2.0f * 3.14159f * 550.0f * i / sample_rate_) * 0.3f;
        }

        Mixer mixer(4, 0.25f);
        std::vector<const float *> inputs = {input1.get(), input2.get(), input3.get(), input4.get()};

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations_; ++i)
        {
            mixer.process(inputs, output.get(), buffer_size_);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        double time_per_buffer = static_cast<double>(duration) / iterations_;
        double buffer_duration_ms = (static_cast<double>(buffer_size_) / static_cast<double>(sample_rate_)) * 1000.0;
        double cpu_percent = (time_per_buffer / 1000.0) / buffer_duration_ms * 100.0;

        results_.push_back({"Mixer (4ch)", time_per_buffer, cpu_percent});
        std::cout << "Mixer (4ch): " << time_per_buffer << " μs/buffer (" << cpu_percent << "% CPU)\n";
    }

    void printSummary()
    {
        std::cout << "---\n";
        std::cout << "Summary:\n";

        double total_cpu = 0.0;
        for (const auto &r : results_)
        {
            total_cpu += r.cpu_percent;
        }

        std::cout << "Total Effects CPU: " << total_cpu << "%\n";

        if (total_cpu < 5.0)
        {
            std::cout << "Rating: EXCELLENT - Very efficient for real-time audio\n";
        }
        else if (total_cpu < 10.0)
        {
            std::cout << "Rating: GOOD - Suitable for real-time audio\n";
        }
        else if (total_cpu < 25.0)
        {
            std::cout << "Rating: MODERATE - May need optimization\n";
        }
        else
        {
            std::cout << "Rating: POOR - Too CPU-intensive for real-time\n";
        }
    }

    void run()
    {
        printHeader();
        benchmarkGain();
        benchmarkOverdrive();
        benchmarkMixer();
        printSummary();
    }
};

int main()
{
    EffectsBenchmark benchmark(6000, 1000);
    benchmark.run();
    return 0;
}
