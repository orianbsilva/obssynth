/**
 * @file audio_backend_comparison.cpp
 * @brief Comprehensive test comparing all available audio backends
 *
 * This program tests and compares the performance characteristics of all
 * available audio backends on the system, including the new DirectSound
 * implementation. It provides latency and performance metrics.
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include "audio/audio_sink.hpp"
#include "oscillators/oscillator.hpp"

using namespace synth;

// Test duration and frequency
constexpr int TEST_DURATION_SECONDS = 3;
constexpr float TEST_FREQUENCY = 440.0f;
constexpr float TEST_VOLUME = 0.2f;

// Audio generator for testing
class TestAudioGenerator
{
public:
    TestAudioGenerator(float frequency = TEST_FREQUENCY) : oscillator_(frequency, WaveType::SINE, 48000.0f) {}

    void audioCallback(float *buffer, int frameCount, int channelCount)
    {
        for (int frame = 0; frame < frameCount; ++frame)
        {
            float sample = oscillator_.getSample() * TEST_VOLUME;

            // Fill all channels with the same sample
            for (int channel = 0; channel < channelCount; ++channel)
            {
                buffer[frame * channelCount + channel] = sample;
            }
        }
    }

private:
    Oscillator oscillator_;
};

// Test a specific audio backend
bool testBackend(AudioBackendType backendType, const std::string &backendName)
{
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "Testing: " << backendName << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";

    AudioSink audioSink;
    TestAudioGenerator generator;

    // Set backend type
    audioSink.setBackend(backendType);

    // Set up audio callback
    audioSink.setAudioCallback([&generator](float *buffer, int frameCount, int channelCount)
                               { generator.audioCallback(buffer, frameCount, channelCount); });

    // Measure initialization time
    auto startTime = std::chrono::high_resolution_clock::now();

    std::cout << "Initializing " << backendName << "...\n";
    if (audioSink.init() != 0)
    {
        std::cout << "❌ Failed to initialize " << backendName << "\n";
        return false;
    }

    auto initTime = std::chrono::high_resolution_clock::now();
    auto initDuration = std::chrono::duration_cast<std::chrono::milliseconds>(initTime - startTime).count();
    std::cout << "✅ Initialization took: " << initDuration << " ms\n";

    // Measure startup time
    startTime = std::chrono::high_resolution_clock::now();
    std::cout << "Starting playback...\n";
    if (audioSink.start() != 0)
    {
        std::cout << "❌ Failed to start " << backendName << " playback\n";
        return false;
    }

    auto startupTime = std::chrono::high_resolution_clock::now();
    auto startupDuration = std::chrono::duration_cast<std::chrono::milliseconds>(startupTime - startTime).count();
    std::cout << "✅ Startup took: " << startupDuration << " ms\n";

    std::cout << "🎵 Playing " << TEST_FREQUENCY << "Hz sine wave for " << TEST_DURATION_SECONDS << " seconds...\n";

    // Play audio for test duration
    std::this_thread::sleep_for(std::chrono::seconds(TEST_DURATION_SECONDS));

    // Measure cleanup time
    startTime = std::chrono::high_resolution_clock::now();
    audioSink.stop();
    auto cleanupTime = std::chrono::high_resolution_clock::now();
    auto cleanupDuration = std::chrono::duration_cast<std::chrono::milliseconds>(cleanupTime - startTime).count();

    std::cout << "✅ " << backendName << " test completed successfully!\n";
    std::cout << "📊 Performance Summary:\n";
    std::cout << "   • Initialization: " << initDuration << " ms\n";
    std::cout << "   • Startup: " << startupDuration << " ms\n";
    std::cout << "   • Cleanup: " << cleanupDuration << " ms\n";
    std::cout << "   • Total overhead: " << (initDuration + startupDuration + cleanupDuration) << " ms\n";

    return true;
}

int main()
{
    std::cout << "Audio Backend Comparison Test\n";
    std::cout << "════════════════════════════════════════════════════\n";
    std::cout << "This program tests all available audio backends and\n";
    std::cout << "provides performance metrics for comparison.\n\n";

    // List available backends for Windows
    std::vector<std::pair<AudioBackendType, std::string>> backends;

#ifdef _WIN32
    backends.push_back({AudioBackendType::WaveOut, "WinMM WaveOut"});
    backends.push_back({AudioBackendType::DirectSound, "DirectSound"});
    backends.push_back({AudioBackendType::WASAPI, "WASAPI"});
    // Note: ASIO might not be available without drivers
    // backends.push_back({AudioBackendType::ASIO, "ASIO"});
#elif defined(__linux__)
    backends.push_back({AudioBackendType::ALSA, "ALSA"});
#ifdef HAVE_PULSEAUDIO
    backends.push_back({AudioBackendType::PulseAudio, "PulseAudio"});
#endif
#endif

    std::cout << "Found " << backends.size() << " audio backends to test.\n";

    // Test each backend
    int successCount = 0;
    int totalCount = backends.size();

    for (const auto &backend : backends)
    {
        bool success = testBackend(backend.first, backend.second);
        if (success)
        {
            successCount++;
        }

        // Wait between tests
        if (&backend != &backends.back()) // Not the last one
        {
            std::cout << "\nWaiting 2 seconds before next test...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    // Final summary
    std::cout << "\n════════════════════════════════════════════════════\n";
    std::cout << "FINAL RESULTS\n";
    std::cout << "════════════════════════════════════════════════════\n";
    std::cout << "✅ Successful backends: " << successCount << "/" << totalCount << "\n";
    std::cout << "❌ Failed backends: " << (totalCount - successCount) << "/" << totalCount << "\n";

    if (successCount == totalCount)
    {
        std::cout << "\n🎉 All audio backends are working correctly!\n";
        std::cout << "The DirectSound implementation is ready for use.\n";
    }
    else
    {
        std::cout << "\n⚠️  Some audio backends failed. Check the output above for details.\n";
    }

    std::cout << "\nRecommendations:\n";
    std::cout << "• DirectSound: Good balance of performance and compatibility\n";
    std::cout << "• WASAPI: Lowest latency, best for professional audio\n";
    std::cout << "• WaveOut: Most compatible, higher latency\n";

    return (successCount == totalCount) ? 0 : 1;
}
