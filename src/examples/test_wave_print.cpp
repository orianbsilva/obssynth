#include "../lib/oscillators/oscillator.hpp"
#include <iostream>
#include <iomanip>
#include <memory>

using namespace synth;

void printWaveform(const std::string &name, WaveType wave_type, int samples_to_print = 500)
{
    std::cout << "\n"
              << name << " Wave (first " << samples_to_print << " samples):\n";
    std::cout << std::string(60, '=') << "\n";

    // Create oscillator
    auto oscillator = std::make_unique<Oscillator>(440.0f, wave_type, 48000.0f);
    oscillator->setAmplitude(1.0f); // Full amplitude for clear visualization

    // Print samples with ASCII visualization
    for (int i = 0; i < samples_to_print; i++)
    {
        float sample = oscillator->getSample();

        // Print sample number and value
        std::cout << std::setw(3) << i << ": "
                  << std::setw(8) << std::fixed << std::setprecision(4) << sample;

        // ASCII visualization (scale to 40 characters wide)
        int bar_length = static_cast<int>((sample + 1.0f) * 20.0f); // -1 to 1 -> 0 to 40
        std::cout << " |";

        for (int j = 0; j < 40; j++)
        {
            if (j < bar_length)
                std::cout << "#";
            else
                std::cout << " ";
        }

        std::cout << "|\n";
    }
}

int main()
{
    std::cout << "Wave Print Test - Oscillator Waveform Visualization\n";
    std::cout << "==================================================\n";
    std::cout << "\nThis test displays the first few samples of each wave type\n";
    std::cout << "at 440 Hz (A4) to show their characteristic shapes.\n";
    std::cout << "\nVisualization: Each line shows sample value and ASCII bar chart\n";
    std::cout << "Scale: leftmost = -1.0, center = 0.0, rightmost = +1.0\n";

    // Print each wave type
    printWaveform("SINE", WaveType::SINE);
    // printWaveform("SAWTOOTH", WaveType::SAW);
    // printWaveform("SQUARE", WaveType::SQUARE);

    std::cout << "\n"
              << std::string(60, '=') << "\n";
    std::cout << "Wave characteristics:\n";
    std::cout << "• SINE: Smooth sinusoidal wave, fundamental frequency only\n";
    std::cout << "• SAWTOOTH: Linear ramp, rich in harmonics\n";
    std::cout << "• SQUARE: Digital on/off, odd harmonics only\n\n";

    // Show frequency analysis
    std::cout << "Frequency Analysis (440 Hz A4):\n";
    std::cout << "• Period: " << std::fixed << std::setprecision(4)
              << (48000.0f / 440.0f) << " samples per cycle\n";
    std::cout << "• Wavelength: " << std::fixed << std::setprecision(6)
              << (340.29f / 440.0f) << " meters (in air at 20°C)\n\n";

    return 0;
}
