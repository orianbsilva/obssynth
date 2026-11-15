#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include "audio/audio_sink.hpp"
#include "oscillators/oscillator.hpp"

#ifdef _WIN32

int main()
{
    std::cout << "ASIO Audio Backend Test\n";
    std::cout << "======================\n\n";

    // List available audio devices
    std::cout << "Available audio devices:\n";
    synth::AudioSink::listDevices();
    std::cout << "\n";

    // Create audio sink with ASIO backend
    synth::AudioSink audioSink;
    audioSink.setBackend(synth::AudioBackendType::ASIO);

    // Create an oscillator for testing
    synth::Oscillator oscillator(440.0f, synth::WaveType::SINE, 48000.0f); // A4 note

    // Initialize audio sink
    std::cout << "Initializing ASIO backend...\n";
    if (audioSink.init() != 0)
    {
        std::cerr << "Failed to initialize ASIO backend\n";
        std::cout << "This could be because:\n";
        std::cout << "- No ASIO drivers are installed\n";
        std::cout << "- ASIO driver is in use by another application\n";
        std::cout << "- ASIO driver failed to initialize\n";
        std::cout << "\nFor a full ASIO implementation, you would need:\n";
        std::cout << "- Steinberg ASIO SDK\n";
        std::cout << "- Professional audio interface with ASIO drivers\n";
        std::cout << "- Proper ASIO driver installation\n";
        return 1;
    }

    // Set up audio callback
    std::cout << "Setting up audio callback...\n";
    audioSink.setAudioCallback([&oscillator](float *buffer, int bufferSize, int /*numChannels*/)
                               {
        // Generate audio samples
        for (int i = 0; i < bufferSize; ++i) {
            buffer[i] = oscillator.getSample() * 0.3f; // Reduce volume to 30%
        } });

    // Start audio playback
    std::cout << "Starting ASIO audio playback...\n";
    if (audioSink.start() != 0)
    {
        std::cerr << "Failed to start ASIO audio playback\n";
        return 1;
    }

    std::cout << "\nPlaying 440 Hz sine wave through ASIO...\n";
    std::cout << "Benefits of ASIO:\n";
    std::cout << "- Ultra-low latency (typically 1-10ms)\n";
    std::cout << "- Direct hardware access\n";
    std::cout << "- Professional audio interface support\n";
    std::cout << "- Stable timing for real-time audio\n";
    std::cout << "\nPress Enter to stop...\n";

    // Wait for user input
    std::cin.get();

    // Stop audio playback
    std::cout << "Stopping ASIO audio playback...\n";
    audioSink.stop();

    std::cout << "ASIO test completed.\n";
    return 0;
}

#else

int main()
{
    std::cout << "ASIO Audio Backend Test\n";
    std::cout << "======================\n\n";
    std::cout << "ASIO is only available on Windows platforms.\n";
    std::cout << "This test program requires Windows with ASIO drivers.\n";
    return 0;
}

#endif
