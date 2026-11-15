/**
 * @file bench_lock_overhead.cpp
 * @brief Lock overhead benchmark for cross-platform performance comparison
 *
 * Measures the performance impact of mutex locking in audio processing.
 * This benchmark compares:
 * - Locked polyphonic oscillator (thread-safe)
 * - Lock-free equivalent (single-threaded)
 *
 * Run with:
 *   ./build/bin/bench_lock_overhead
 *
 * Use this benchmark to:
 * - Measure lock contention overhead on different platforms
 * - Determine if lock-free design is worth the complexity
 * - Compare mutex implementations across compilers
 */

#include "../src/app/common/audio_constants.hpp"
#include "../src/lib/oscillators/polyphonic_oscillator.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <vector>
#include <array>
#include <iomanip>

using namespace synth;

// Platform detection
#if defined(_WIN32)
#define PLATFORM_NAME "Windows"
#define MUTEX_TYPE "Win32 Critical Section (std::mutex)"
#elif defined(__linux__)
#define PLATFORM_NAME "Linux"
#define MUTEX_TYPE "pthreads mutex"
#elif defined(__APPLE__)
#define PLATFORM_NAME "macOS"
#define MUTEX_TYPE "pthreads mutex"
#else
#define PLATFORM_NAME "Unknown"
#define MUTEX_TYPE "std::mutex"
#endif

// Lock-free implementation for baseline comparison
class LockFreePolyphonicOscillator
{
private:
    static constexpr int MAX_VOICES = 8;

    struct VoiceState
    {
        std::unique_ptr<Oscillator> oscillator;
        bool is_active = false;
        uint8_t note_number = 0;
        float frequency = 440.0f;
        float velocity = 0.0f;
        float amplitude = 0.0f;
    };

    std::array<VoiceState, MAX_VOICES> voices_;

public:
    LockFreePolyphonicOscillator(WaveType wave_type, float sample_rate)
    {
        for (auto &voice : voices_)
        {
            voice.oscillator = std::make_unique<Oscillator>(440.0f, wave_type, sample_rate);
            voice.oscillator->setAmplitude(0.0f);
            voice.is_active = false;
        }
    }

    void noteOn(uint8_t note_number, float velocity = 1.0f)
    {
        int voice_index = -1;
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (!voices_[i].is_active)
            {
                voice_index = i;
                break;
            }
        }

        if (voice_index == -1)
            voice_index = 0;

        VoiceState &voice = voices_[voice_index];
        voice.note_number = note_number;
        voice.frequency = 440.0f * std::pow(2.0f, (static_cast<float>(note_number) - 69.0f) / 12.0f);
        voice.velocity = velocity;
        voice.amplitude = velocity * 0.8f / MAX_VOICES;

        voice.oscillator->setFrequency(voice.frequency);
        voice.oscillator->setAmplitude(voice.amplitude);
        voice.oscillator->resetPhase();
        voice.is_active = true;
    }

    void generateBuffer(float *buffer, int size)
    {
        if (!buffer || size <= 0)
            return;

        std::fill(buffer, buffer + size, 0.0f);

        for (int sample = 0; sample < size; ++sample)
        {
            float mixedSample = 0.0f;
            for (int v = 0; v < MAX_VOICES; ++v)
            {
                if (voices_[v].is_active)
                {
                    mixedSample += voices_[v].oscillator->getSample();
                }
            }
            buffer[sample] = mixedSample;
        }
    }
};

struct BenchmarkResult
{
    std::string name;
    long long time_us;
    double cpu_percent;
};

class LockOverheadBenchmark
{
private:
    int buffer_size_;
    int iterations_;
    float sample_rate_;

public:
    LockOverheadBenchmark(int buffer_size = 6000, int iterations = 1000)
        : buffer_size_(buffer_size), iterations_(iterations), sample_rate_(48000.0f)
    {
    }

    void run()
    {
        std::cout << "Lock Overhead Benchmark\n";
        std::cout << "=======================\n";
        std::cout << "Platform: " << PLATFORM_NAME << "\n";
        std::cout << "Mutex Type: " << MUTEX_TYPE << "\n";
        std::cout << "Buffer Size: " << buffer_size_ << " samples\n";
        std::cout << "Iterations: " << iterations_ << "\n";
        std::cout << "Sample Rate: " << sample_rate_ << " Hz\n";
        std::cout << "---\n\n";

        // Create both oscillator types
        PolyphonicOscillator locked_osc(WaveType::SINE, sample_rate_);
        LockFreePolyphonicOscillator lockfree_osc(WaveType::SINE, sample_rate_);

        // Set up identical test notes
        locked_osc.noteOn(60, 0.8f);
        locked_osc.noteOn(64, 0.7f);
        locked_osc.noteOn(67, 0.6f);
        locked_osc.noteOn(72, 0.5f);

        lockfree_osc.noteOn(60, 0.8f);
        lockfree_osc.noteOn(64, 0.7f);
        lockfree_osc.noteOn(67, 0.6f);
        lockfree_osc.noteOn(72, 0.5f);

        auto locked_buffer = std::make_unique<float[]>(buffer_size_);
        auto lockfree_buffer = std::make_unique<float[]>(buffer_size_);

        std::cout << "Testing with 4 active voices...\n\n";

        // Warmup
        for (int i = 0; i < 100; ++i)
        {
            locked_osc.generateBufferOptimized(locked_buffer.get(), buffer_size_);
            lockfree_osc.generateBuffer(lockfree_buffer.get(), buffer_size_);
        }

        // Benchmark locked version
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations_; ++i)
        {
            locked_osc.generateBufferOptimized(locked_buffer.get(), buffer_size_);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto locked_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // Benchmark lock-free version
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations_; ++i)
        {
            lockfree_osc.generateBuffer(lockfree_buffer.get(), buffer_size_);
        }
        end = std::chrono::high_resolution_clock::now();
        auto lockfree_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // Calculate metrics
        double speedup = static_cast<double>(locked_time) / static_cast<double>(lockfree_time);
        double overhead_percent = ((static_cast<double>(locked_time) - static_cast<double>(lockfree_time)) / static_cast<double>(lockfree_time)) * 100.0;
        double buffer_duration_ms = (static_cast<double>(buffer_size_) / static_cast<double>(sample_rate_)) * 1000.0;
        double locked_cpu = (static_cast<double>(locked_time) / iterations_ / 1000.0) / buffer_duration_ms * 100.0;
        double lockfree_cpu = (static_cast<double>(lockfree_time) / iterations_ / 1000.0) / buffer_duration_ms * 100.0;

        std::cout << "Results:\n";
        std::cout << "========\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Locked version:    " << locked_time << " μs total\n";
        std::cout << "Lock-free version: " << lockfree_time << " μs total\n";
        std::cout << "Speedup:           " << speedup << "x\n";
        std::cout << "Lock overhead:     " << overhead_percent << "%\n\n";

        std::cout << "Real-time Performance:\n";
        std::cout << "======================\n";
        std::cout << "Buffer duration:   " << buffer_duration_ms << " ms\n";
        std::cout << "Locked CPU usage:  " << locked_cpu << "%\n";
        std::cout << "Lock-free CPU:     " << lockfree_cpu << "%\n";
        std::cout << "CPU savings:       " << (locked_cpu - lockfree_cpu) << "%\n\n";

        std::cout << "Analysis:\n";
        std::cout << "=========\n";
        if (overhead_percent < 5.0)
        {
            std::cout << "✓ Lock overhead is MINIMAL (" << overhead_percent << "%) - mutex is efficient on this platform\n";
        }
        else if (overhead_percent < 15.0)
        {
            std::cout << "⚠ MODERATE lock overhead (" << overhead_percent << "%) - consider lock-free for high-frequency access\n";
        }
        else
        {
            std::cout << "✗ SIGNIFICANT lock overhead (" << overhead_percent << "%) - lock-free design recommended\n";
        }

        if (locked_cpu < 10.0)
        {
            std::cout << "✓ Real-time safe: CPU usage under 10%\n";
        }
        else if (locked_cpu < 25.0)
        {
            std::cout << "⚠ Marginal for real-time: consider smaller buffer or optimization\n";
        }
        else
        {
            std::cout << "✗ Not suitable for real-time audio processing\n";
        }

        std::cout << "\n✓ Benchmark complete.\n";
    }
};

int main()
{
    LockOverheadBenchmark benchmark(6000, 1000);
    benchmark.run();
    return 0;
}
