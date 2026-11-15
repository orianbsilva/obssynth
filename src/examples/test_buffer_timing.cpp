/**
 * @file test_buffer_timing.cpp
 * @brief Test to analyze DirectSound buffer timing and identify periodic patterns
 */

#include "../lib/audio/audio_backend.hpp"
#include "../lib/oscillators/oscillator.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <fstream>
#include <thread>
#include <cmath>

using namespace synth;

class TimingAnalyzer
{
private:
    std::vector<std::chrono::high_resolution_clock::time_point> writeTimes_;
    std::vector<size_t> writeSizes_;
    std::chrono::high_resolution_clock::time_point startTime_;

public:
    TimingAnalyzer()
    {
        startTime_ = std::chrono::high_resolution_clock::now();
    }

    void recordWrite(size_t samples)
    {
        auto now = std::chrono::high_resolution_clock::now();
        writeTimes_.push_back(now);
        writeSizes_.push_back(samples);
    }

    void analyzeAndSave()
    {
        std::ofstream file("buffer_timing_analysis.csv");
        file << "Time_ms,Samples,Interval_ms\n";

        for (size_t i = 0; i < writeTimes_.size(); ++i)
        {
            auto timeFromStart = std::chrono::duration_cast<std::chrono::microseconds>(
                                     writeTimes_[i] - startTime_)
                                     .count() /
                                 1000.0;

            double intervalMs = 0.0;
            if (i > 0)
            {
                intervalMs = std::chrono::duration_cast<std::chrono::microseconds>(
                                 writeTimes_[i] - writeTimes_[i - 1])
                                 .count() /
                             1000.0;
            }

            file << timeFromStart << "," << writeSizes_[i] << "," << intervalMs << "\n";
        }

        std::cout << "Timing analysis saved to buffer_timing_analysis.csv\n";
        std::cout << "Total writes: " << writeTimes_.size() << "\n";

        if (writeTimes_.size() > 1)
        {
            auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 writeTimes_.back() - writeTimes_.front())
                                 .count();
            double avgInterval = static_cast<double>(totalTime) / (writeTimes_.size() - 1);
            std::cout << "Average write interval: " << avgInterval << " ms\n";
        }
    }
};

static TimingAnalyzer *g_analyzer = nullptr;

void testCallback(float *buffer, int samples, int channels)
{
    if (g_analyzer)
    {
        g_analyzer->recordWrite(samples * channels);
    }

    // Generate simple sine wave for testing
    static float phase = 0.0f;
    const float frequency = 440.0f;
    const float sampleRate = 48000.0f;
    const float phaseIncrement = 2.0f * 3.14159265f * frequency / sampleRate;

    for (int i = 0; i < samples * channels; i += channels)
    {
        float sample = sinf(phase) * 0.3f; // Lower volume
        buffer[i] = sample;                // Left
        buffer[i + 1] = sample;            // Right
        phase += phaseIncrement;
        if (phase > 2.0f * 3.14159265f)
        {
            phase -= 2.0f * 3.14159265f;
        }
    }
}

int main()
{
    std::cout << "DirectSound Buffer Timing Analysis\n";
    std::cout << "==================================\n";

    TimingAnalyzer analyzer;
    g_analyzer = &analyzer;

    // Create DirectSound backend
    auto backend = createDirectSoundBackend();
    if (!backend)
    {
        std::cerr << "Failed to create DirectSound backend\n";
        return 1;
    }

    // Initialize
    if (backend->init() != 0)
    {
        std::cerr << "Failed to initialize DirectSound backend\n";
        return 1;
    }

    // Set callback
    backend->setCallback(testCallback);

    // Start playback
    std::cout << "Starting audio playback for timing analysis...\n";
    if (backend->start() != 0)
    {
        std::cerr << "Failed to start DirectSound backend\n";
        return 1;
    }

    // Run for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop playback
    std::cout << "Stopping playback...\n";
    backend->stop();

    // Analyze timing
    analyzer.analyzeAndSave();

    g_analyzer = nullptr;

    std::cout << "Buffer timing analysis completed.\n";
    std::cout << "Check buffer_timing_analysis.csv for detailed timing data.\n";

    return 0;
}
