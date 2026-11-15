#include "../lib/oscillators/oscillator.hpp"
#include <iostream>

using namespace synth;

int main()
{
    std::cout << "Testing Oscillator Frequency Modulation" << std::endl;
    std::cout << "=======================================" << std::endl;

    // Create oscillator
    Oscillator osc(440.0f, WaveType::SINE, 48000.0f);

    std::cout << "Initial frequency: " << osc.getFrequency() << " Hz" << std::endl;

    // Test frequency modulation
    std::cout << "\nTesting frequency modulation..." << std::endl;

    // No modulation (should be 440 Hz)
    osc.setFrequencyModulation(0.0f);
    std::cout << "Modulation 0%: " << osc.getFrequency() << " Hz" << std::endl;

    // Positive modulation (+50% of max = +5% frequency)
    osc.setFrequencyModulation(0.5f);
    std::cout << "Modulation +50%: " << osc.getFrequency() << " Hz (expected ~462 Hz)" << std::endl;

    // Negative modulation (-50% of max = -5% frequency)
    osc.setFrequencyModulation(-0.5f);
    std::cout << "Modulation -50%: " << osc.getFrequency() << " Hz (expected ~418 Hz)" << std::endl;

    // Maximum positive modulation (+100% of max = +10% frequency)
    osc.setFrequencyModulation(1.0f);
    std::cout << "Modulation +100%: " << osc.getFrequency() << " Hz (expected ~484 Hz)" << std::endl;

    // Maximum negative modulation (-100% of max = -10% frequency)
    osc.setFrequencyModulation(-1.0f);
    std::cout << "Modulation -100%: " << osc.getFrequency() << " Hz (expected ~396 Hz)" << std::endl;

    // Test frequency change with modulation applied
    std::cout << "\nTesting frequency change with modulation..." << std::endl;
    osc.setFrequencyModulation(0.5f); // 50% modulation
    osc.setFrequency(880.0f);         // Change to A5
    std::cout << "New base frequency 880 Hz with +50% modulation: " << osc.getFrequency() << " Hz (expected ~924 Hz)" << std::endl;

    // Reset modulation
    osc.setFrequencyModulation(0.0f);
    std::cout << "Reset modulation to 0%: " << osc.getFrequency() << " Hz (expected 880 Hz)" << std::endl;

    std::cout << "\n✓ Frequency modulation test completed!" << std::endl;

    return 0;
}
