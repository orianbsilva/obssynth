#include "../lib/oscillators/oscillator.hpp"
#include "../lib/utils/wav_writer.hpp"
#include <iostream>
#include <vector>
#include <string>

using namespace synth;

int main()
{
    const float SAMPLE_RATE = 48000.0f;
    const float FREQUENCY = 440.0f; // A4 note
    const float DURATION = 3.0f;    // 3 seconds
    const int SAMPLES = static_cast<int>(SAMPLE_RATE * DURATION);

    std::cout << "FM Synthesis Test" << std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "Frequency: " << FREQUENCY << " Hz" << std::endl;
    std::cout << "Duration: " << DURATION << " seconds" << std::endl;
    std::cout << "Sample Rate: " << SAMPLE_RATE << " Hz" << std::endl;
    std::cout << std::endl;

    // Test different FM configurations
    std::vector<std::tuple<float, float, std::string>> configurations = {
        {1.0f, 0.0f, "carrier_only"},       // Just carrier (should sound like sine)
        {1.0f, 2.0f, "ratio1_mod2"},        // Low modulation
        {1.0f, 8.0f, "ratio1_mod8"},        // Higher modulation
        {2.0f, 5.0f, "ratio2_mod5"},        // Harmonic modulator
        {1.414f, 10.0f, "ratio1414_mod10"}, // Non-harmonic modulator
        {3.5f, 15.0f, "ratio35_mod15"},     // Complex spectrum
        {0.5f, 3.0f, "ratio05_mod3"}        // Sub-harmonic modulator
    };

    for (const auto &config : configurations)
    {
        float ratio = std::get<0>(config);
        float mod_index = std::get<1>(config);
        std::string name = std::get<2>(config);

        std::cout << "Generating FM: ratio=" << ratio << ", mod_index=" << mod_index
                  << " -> " << name << "_fm.wav" << std::endl;

        // Create oscillator
        Oscillator osc(FREQUENCY, WaveType::FM_SYNTHESIS, SAMPLE_RATE);
        osc.setAmplitude(0.4f); // Lower amplitude to prevent clipping

        // Set FM parameters
        osc.setFMModulatorRatio(ratio);
        osc.setFMModulationIndex(mod_index);

        // Generate samples
        std::vector<float> samples(SAMPLES);
        for (int i = 0; i < SAMPLES; ++i)
        {
            samples[i] = osc.getSample();
        }

        // Write to WAV file
        std::string filename = name + "_fm.wav";
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
              << "FM Synthesis test completed!" << std::endl;
    std::cout << "Listen to the different files to hear how modulator ratio and modulation index affect the sound:" << std::endl;
    std::cout << "- carrier_only_fm.wav: Pure sine wave (no modulation)" << std::endl;
    std::cout << "- ratio1_mod2_fm.wav: Gentle FM effect" << std::endl;
    std::cout << "- ratio1_mod8_fm.wav: More aggressive FM" << std::endl;
    std::cout << "- ratio2_mod5_fm.wav: Harmonic modulator creates rich overtones" << std::endl;
    std::cout << "- ratio1414_mod10_fm.wav: Non-harmonic creates complex metallic sounds" << std::endl;
    std::cout << "- ratio35_mod15_fm.wav: Very complex spectrum" << std::endl;
    std::cout << "- ratio05_mod3_fm.wav: Sub-harmonic creates darker, thicker sound" << std::endl;

    return 0;
}