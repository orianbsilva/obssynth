#pragma once
#include <cstdint>

#include "../math/synt_math.hpp"
#include <functional>

namespace synth
{
    // LFO wave types (extended from basic oscillator types)
    enum class LFOWaveType
    {
        SINE,
        TRIANGLE,
        SQUARE,
        SAW,
        RANDOM
    };

    // Forward declaration for LFO wave generator function pointer

    // Low Frequency Oscillator class for modulation
    class LFO
    {
    private:
        float frequency_;                     // LFO frequency in Hz
        float phase_;                         // Current phase (0 to TWO_PI)
        float sample_rate_;                   // Sample rate
        LFOWaveType wave_type_;               // Current waveform
        float amplitude_;                     // LFO output amplitude (0.0 to 1.0)
        float phase_increment_;               // Phase increment per sample
        float last_random_value_;             // For random wave type
        uint32_t rng_ = 0x2545F491u; // the random wave's own sequence

        // Wave generator functions
        static float sine_wave(float normalized_phase);
        static float triangle_wave(float normalized_phase);
        static float square_wave(float normalized_phase);
        static float saw_wave(float normalized_phase);
        float random_wave(float normalized_phase);

    public:
        // Constructor
        LFO(float frequency, LFOWaveType wave_type, float sample_rate);

        // Destructor
        ~LFO() = default;

        // Parameter setters
        void setFrequency(float frequency);
        void setWaveType(LFOWaveType wave_type);
        void setAmplitude(float amplitude);
        void resetPhase();

        // Sample generation
        float getSample();

        // Getters
        float getFrequency() const { return frequency_; }
        float getPhase() const { return phase_; }
        float getSampleRate() const { return sample_rate_; }
        /// A new rate (an engine that advances its LFOs once per control step): same frequency, same phase
        void setSampleRate(float sample_rate)
        {
            sample_rate_ = sample_rate;
            setFrequency(frequency_);
        }
        LFOWaveType getWaveType() const { return wave_type_; }
        float getAmplitude() const { return amplitude_; }

        // Utility functions
        static const char *waveTypeToString(LFOWaveType type);
    };

} // namespace synth