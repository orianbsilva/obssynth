#include "../app/common/audio_constants.hpp"
#include "../lib/oscillators/polyphonic_oscillator.hpp"
#include "platform_utils.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <vector>

using namespace synth;
using namespace examples;

// Lock-free version for performance comparison
class LockFreePolyphonicOscillator
{
private:
    static constexpr int MAX_VOICES = 8;
    std::array<Voice, MAX_VOICES> voices_;
    [[maybe_unused]] float sample_rate_;
    [[maybe_unused]] WaveType wave_type_;
    // NO MUTEX!

public:
    LockFreePolyphonicOscillator(WaveType wave_type, float sample_rate)
        : sample_rate_(sample_rate), wave_type_(wave_type)
    {
        for (auto &voice : voices_)
        {
            voice.oscillator_ = std::make_unique<Oscillator>(440.0f, wave_type, sample_rate);
            voice.oscillator_->setAmplitude(0.0f);
            voice.is_active_ = false;
        }
    }

    void noteOn(uint8_t note_number, float velocity = 1.0f)
    {
        int voice_index = -1;
        for (int i = 0; i < MAX_VOICES; ++i)
        {
            if (!voices_[i].is_active_)
            {
                voice_index = i;
                break;
            }
        }

        if (voice_index == -1)
            voice_index = 0;

        Voice &voice = voices_[voice_index];
        voice.note_number_ = note_number;
        voice.frequency_ = PolyphonicOscillator::noteToFrequency(note_number);
        voice.velocity_ = velocity;
        voice.amplitude_ = velocity * 0.8f / MAX_VOICES;

        voice.oscillator_->setFrequency(voice.frequency_);
        voice.oscillator_->setAmplitude(voice.amplitude_);
        voice.oscillator_->resetPhase();
        voice.is_active_ = true;
    }

    // UNSAFE: No locks
    void generateBufferLockFree(float *buffer, int size)
    {
        if (!buffer || size <= 0)
            return;

        std::fill(buffer, buffer + size, 0.0f);

        for (int sample = 0; sample < size; ++sample)
        {
            float mixedSample = 0.0f;

            for (int v = 0; v < MAX_VOICES; ++v)
            {
                Voice &voice = voices_[v];
                if (voice.is_active_)
                {
                    mixedSample += voice.oscillator_->getSample();
                }
            }

            buffer[sample] = mixedSample;
        }
    }
};

int main()
{
    PlatformUtils::initialize();

    std::cout << "Lock Performance Comparison\n";
    std::cout << "===========================\n\n";

    const int BUFFER_SIZE = 6000; // DirectSound buffer size
    const int ITERATIONS = 1000;

    // Create oscillators
    PolyphonicOscillator locked_osc(WaveType::SINE, SAMPLE_RATE);
    LockFreePolyphonicOscillator lockfree_osc(WaveType::SINE, SAMPLE_RATE);

    // Set up test notes
    locked_osc.noteOn(60, 0.8f);
    locked_osc.noteOn(64, 0.7f);
    locked_osc.noteOn(67, 0.6f);
    locked_osc.noteOn(72, 0.5f);

    lockfree_osc.noteOn(60, 0.8f);
    lockfree_osc.noteOn(64, 0.7f);
    lockfree_osc.noteOn(67, 0.6f);
    lockfree_osc.noteOn(72, 0.5f);

    // Prepare buffers
    auto locked_buffer = std::make_unique<float[]>(BUFFER_SIZE);
    auto lockfree_buffer = std::make_unique<float[]>(BUFFER_SIZE);

    std::cout << "Testing with " << ITERATIONS << " iterations of " << BUFFER_SIZE << " samples...\n\n";

    // Test locked version
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
    {
        locked_osc.generateBufferOptimized(locked_buffer.get(), BUFFER_SIZE);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto locked_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Test lock-free version
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i)
    {
        lockfree_osc.generateBufferLockFree(lockfree_buffer.get(), BUFFER_SIZE);
    }
    end = std::chrono::high_resolution_clock::now();
    auto lockfree_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // Calculate performance difference
    double speedup = static_cast<double>(locked_time) / static_cast<double>(lockfree_time);
    double overhead_percent = ((static_cast<double>(locked_time) - static_cast<double>(lockfree_time)) / static_cast<double>(lockfree_time)) * 100.0;

    std::cout << "Results:\n";
    std::cout << "========\n";
    std::cout << "Locked version:    " << locked_time << " μs\n";
    std::cout << "Lock-free version: " << lockfree_time << " μs\n";
    std::cout << "Speedup:           " << speedup << "x\n";
    std::cout << "Lock overhead:     " << overhead_percent << "%\n\n";

    // Real-time analysis
    double samples_per_iteration = BUFFER_SIZE;
    double sample_rate = SAMPLE_RATE;
    double buffer_duration_ms = (samples_per_iteration / sample_rate) * 1000.0;

    double locked_processing_time_ms = static_cast<double>(locked_time) / ITERATIONS / 1000.0;
    double lockfree_processing_time_ms = static_cast<double>(lockfree_time) / ITERATIONS / 1000.0;

    double locked_cpu_usage = (locked_processing_time_ms / buffer_duration_ms) * 100.0;
    double lockfree_cpu_usage = (lockfree_processing_time_ms / buffer_duration_ms) * 100.0;

    std::cout << "Real-time Performance:\n";
    std::cout << "======================\n";
    std::cout << "Buffer duration:        " << buffer_duration_ms << " ms\n";
    std::cout << "Locked processing:      " << locked_processing_time_ms << " ms (" << locked_cpu_usage << "% CPU)\n";
    std::cout << "Lock-free processing:   " << lockfree_processing_time_ms << " ms (" << lockfree_cpu_usage << "% CPU)\n";
    std::cout << "CPU savings:            " << (locked_cpu_usage - lockfree_cpu_usage) << "%\n\n";

    std::cout << "Analysis:\n";
    std::cout << "=========\n";
    if (overhead_percent < 5.0)
    {
        std::cout << "✓ Lock overhead is minimal (" << overhead_percent << "%) - current design is efficient\n";
    }
    else if (overhead_percent < 15.0)
    {
        std::cout << "⚠ Moderate lock overhead (" << overhead_percent << "%) - consider optimization\n";
    }
    else
    {
        std::cout << "⚠ Significant lock overhead (" << overhead_percent << "%) - lock-free may be worth the complexity\n";
    }

    if (locked_cpu_usage < 10.0)
    {
        std::cout << "✓ Real-time performance is excellent - both versions are viable\n";
    }
    else
    {
        std::cout << "⚠ High CPU usage - optimization needed for real-time audio\n";
    }

    std::cout << "\nConclusion:\n";
    std::cout << "===========\n";
    std::cout << "Your hypothesis about lock performance depends on the actual overhead.\n";
    std::cout << "Race conditions in small buffers are indeed transient, but the question\n";
    std::cout << "is whether the lock overhead justifies the complexity of lock-free design.\n";
    std::cout << "The current implementation balances safety and performance well.\n";

    PlatformUtils::cleanup();
    return 0;
}
