#include "utils/logger.hpp"
#include "oscillators/oscillator.hpp"
#include <iostream>

using namespace synth;

int main()
{
    try
    {
        // Create a logger instance that logs to stdout
        Logger logger(Logger::LogLevel::INFO);

        logger.info("Synthesizer demo starting");

        // Create an oscillator and log its operations
        Oscillator osc(440.0f, WaveType::SINE, 48000.0f); // A4 note, sine wave, 44.1kHz
        logger.info("Created sine wave oscillator with frequency: 440.0f Hz");

        // Generate some samples and log progress
        logger.debug("Generating audio samples...");

        float samples[10];
        for (int i = 0; i < 10; ++i)
        {
            samples[i] = osc.getSample();
        }

        logger.info("Generated 10 audio samples");

        // Log first and last sample values to demonstrate the samples are being used
        logger.debug("First sample: " + std::to_string(samples[0]));
        logger.debug("Last sample: " + std::to_string(samples[9]));

        // Change frequency and log the change
        osc.setFrequency(880.0f); // A5 note
        logger.warning("Frequency changed to 880.0f Hz");

        // Change wave type
        osc.setWaveType(synth::WaveType::SAW);
        logger.info("Wave type changed to sawtooth");

        // Generate a buffer of samples
        float buffer[512];
        osc.generateBuffer(buffer, 512);
        logger.info("Generated buffer of 512 samples");

        // Test with file logging as well
        synth::Logger file_logger("synth_demo.log", synth::Logger::LogLevel::DEBUG);
        file_logger.info("File logging initialized for synthesizer demo");
        file_logger.debug("Sample values range from -1.0 to 1.0");
        file_logger.debug("Current oscillator frequency: " + std::to_string(osc.getFrequency()) + " Hz");

        logger.info("Synthesizer demo completed successfully");

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
