#include "lfo.hpp"
#include "oscillator.hpp" // To access PI and TWO_PI constants
#include "../math/synt_math.hpp"
#include <iostream>
#include <random>

namespace synth
{
    // Wave generator functions
    float LFO::sine_wave(float normalized_phase)
    {
        return math::sinTurns(normalized_phase); // the table: a modulation needs no libm sine (synt_math.hpp)
    }

    float LFO::triangle_wave(float normalized_phase)
    {
        // Triangle wave: -1 to +1
        if (normalized_phase < 0.5f)
        {
            return 4.0f * normalized_phase - 1.0f;
        }
        else
        {
            return 3.0f - 4.0f * normalized_phase;
        }
    }

    float LFO::square_wave(float normalized_phase)
    {
        return (normalized_phase < 0.5f) ? -1.0f : 1.0f;
    }

    float LFO::saw_wave(float normalized_phase)
    {
        return 2.0f * normalized_phase - 1.0f;
    }

    float LFO::random_wave(float normalized_phase)
    {
        // Sample and hold random values at zero crossings

        // Suppress unused parameter warning
        (void)normalized_phase;

        // Generate new random value when phase crosses zero
        if (phase_ < phase_increment_)
        {
            // its own xorshift (a static random_device + mt19937 was shared by every LFO, seeded anew each run)
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            last_random_value_ = static_cast<float>(rng_ >> 8) / 8388608.0f - 1.0f;
        }

        return last_random_value_;
    }

    // Constructor
    LFO::LFO(float frequency, LFOWaveType wave_type, float sample_rate)
        : frequency_(frequency), phase_(0.0f), sample_rate_(sample_rate),
          wave_type_(wave_type), amplitude_(1.0f), phase_increment_(0.0f),
          last_random_value_(0.0f)
    {

        // Update phase increment
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;

        std::cout << "LFO initialized: " << frequency << " Hz, Type: "
                  << waveTypeToString(wave_type) << std::endl;
    }

    // Set LFO frequency
    void LFO::setFrequency(float frequency)
    {
        // 0 (stopped) to 120 Hz (it was clamped at 1 Hz: every slow LFO of the presets ran at 1 Hz)
        if (frequency < 0.0f)
            frequency = 0.0f;
        if (frequency > 120.0f)
            frequency = 120.0f;

        frequency_ = frequency;
        phase_increment_ = math::TWO_PI * frequency_ / sample_rate_;
    }

    // Set wave type
    void LFO::setWaveType(LFOWaveType wave_type)
    {
        wave_type_ = wave_type;
    }

    // Set amplitude (0.0 to 1.0)
    void LFO::setAmplitude(float amplitude)
    {
        if (amplitude < 0.0f)
            amplitude = 0.0f;
        if (amplitude > 1.0f)
            amplitude = 1.0f;
        amplitude_ = amplitude;
    }

    // Reset phase to zero
    void LFO::resetPhase()
    {
        phase_ = 0.0f;
        last_random_value_ = 0.0f;
    }

    // Generate single sample
    float LFO::getSample()
    {
        float normalized_phase = phase_ / math::TWO_PI;

        // the wave by a switch (it was a std::function: an indirect call the compiler cannot inline, 1.5-3x the
        // time on a Cortex-A7)
        float sample;
        switch (wave_type_)
        {
        case LFOWaveType::TRIANGLE: sample = triangle_wave(normalized_phase); break;
        case LFOWaveType::SQUARE: sample = square_wave(normalized_phase); break;
        case LFOWaveType::SAW: sample = saw_wave(normalized_phase); break;
        case LFOWaveType::RANDOM: sample = random_wave(normalized_phase); break;
        default: sample = sine_wave(normalized_phase); break;
        }

        // Apply amplitude
        sample *= amplitude_;

        // Advance phase (less than a turn a sample: up to 120 Hz; a subtraction, not fmodf)
        phase_ += phase_increment_;
        while (phase_ >= math::TWO_PI)
            phase_ -= math::TWO_PI;

        return sample;
    }

    // Utility function to convert wave type to string
    const char *LFO::waveTypeToString(LFOWaveType type)
    {
        switch (type)
        {
        case LFOWaveType::SINE:
            return "Sine";
        case LFOWaveType::TRIANGLE:
            return "Triangle";
        case LFOWaveType::SQUARE:
            return "Square";
        case LFOWaveType::SAW:
            return "Sawtooth";
        case LFOWaveType::RANDOM:
            return "Random";
        default:
            return "Unknown";
        }
    }

} // namespace synth
