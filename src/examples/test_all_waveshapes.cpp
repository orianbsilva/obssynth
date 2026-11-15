#include "../lib/oscillators/oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include <iostream>
#include <vector>

using namespace synth;

int main()
{
    const float SAMPLE_RATE = 48000.0f;
    const float FREQUENCY = 440.0f; // A4 note
    const float DURATION = 2.0f;    // 2 seconds
    const int SAMPLES = static_cast<int>(SAMPLE_RATE * DURATION);

    // List of all wave types with names
    std::vector<std::pair<WaveType, std::string>> waveTypes = {
        {WaveType::SINE, "sine"},
        {WaveType::SAW, "sawtooth"},
        {WaveType::SQUARE, "square"},
        {WaveType::TRIANGLE, "triangle"},
        {WaveType::PULSE, "pulse"},
        {WaveType::NOISE, "noise"},
        {WaveType::RAMP_UP, "ramp_up"},
        {WaveType::RAMP_DOWN, "ramp_down"},
        {WaveType::FM_SYNTHESIS, "fm_synthesis"}};

    std::cout << "Testing All Wave Shapes" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Frequency: " << FREQUENCY << " Hz" << std::endl;
    std::cout << "Duration: " << DURATION << " seconds" << std::endl;
    std::cout << "Sample Rate: " << SAMPLE_RATE << " Hz" << std::endl;
    std::cout << std::endl;

    // Generate a WAV file for each wave type
    for (const auto &waveInfo : waveTypes)
    {
        WaveType waveType = waveInfo.first;
        std::string waveName = waveInfo.second;

        std::cout << "Generating " << waveName << " wave..." << std::endl;

        // Create oscillator
        Oscillator osc(FREQUENCY, waveType, SAMPLE_RATE);
        osc.setAmplitude(0.5f); // 50% amplitude to prevent clipping

        // Configure FM synthesis parameters if needed
        if (waveType == WaveType::FM_SYNTHESIS)
        {
            osc.setFMModulatorRatio(2.0f);  // Modulator frequency is 2x carrier
            osc.setFMModulationIndex(5.0f); // Strong modulation for clear effect
        }

        // Generate samples
        std::vector<float> samples(SAMPLES);
        for (int i = 0; i < SAMPLES; ++i)
        {
            samples[i] = osc.getSample();
        }

        // Write to WAV file
        std::string filename = waveName + "_test.wav";
        std::vector<float> sampleVector(samples.begin(), samples.end());
        int result = WAVWriter::writeFileSimple(filename, sampleVector, SAMPLES, static_cast<int>(SAMPLE_RATE));
        if (result == 0)
        {
            std::cout << "✓ Successfully created " << filename << std::endl;
        }
        else
        {
            std::cout << "✗ Failed to create " << filename << " (error: " << result << ")" << std::endl;
        }
    }

    std::cout << std::endl
              << "All wave shapes generated successfully!" << std::endl;
    std::cout << "Files created:" << std::endl;
    for (const auto &waveInfo : waveTypes)
    {
        std::cout << "  - " << waveInfo.second << "_test.wav" << std::endl;
    }

    return 0;
}
