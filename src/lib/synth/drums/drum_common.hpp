#pragma once
// What the drum models share. The models (circuit_*.hpp) rebuild the behaviour of classic analog drum circuits from
// the library's parts: a ringing resonator struck by a pulse (a sine whose pitch falls), noise through a filter,
// banks of square waves at inharmonic ratios, envelopes that only decay. They run at the product's rate (2x
// oversampled in ObsDrums), mono; the product pans and mixes. FLOAT-ONLY.
#include <cstdint>

namespace synth
{
    // white noise, each instance its own sequence (the Oscillator's NOISE shares one static seed and follows the phase)
    class DrumNoise
    {
    public:
        explicit DrumNoise(uint32_t seed = 0x6C8E9CF5u) : s_(seed | 1u) {}
        float next()
        {
            s_ ^= s_ << 13;
            s_ ^= s_ >> 17;
            s_ ^= s_ << 5;
            return static_cast<float>(s_ >> 8) / 8388608.0f - 1.0f; // -1..1
        }

    private:
        uint32_t s_;
    };

    // a hit: how hard (0..1), and the accent (0 or 1) a sequencer may add
    struct DrumHit
    {
        float velocity = 0.8f;
        float accent = 0.0f;
        float strength() const { return velocity < 1.0f ? velocity + (1.0f - velocity) * 0.5f * accent : 1.0f; }
    };
} // namespace synth
