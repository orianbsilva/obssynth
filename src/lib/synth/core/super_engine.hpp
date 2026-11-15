/**
 * @file super_engine.hpp
 * @brief SUPER: seven voices of one shape, the centre one and six detuned around it (after the ObsSynth FM-1's)
 *
 * The shape is the oscillator's own wave: SAW, SQUARE (PULSE plays as SQUARE), TRIANGLE or SINE (another: SAW).
 * DETUNE spreads the six side voices up to +-60 cents (unevenly, so they never beat together); SIDE is their level
 * against the centre's, at equal power (the seven at full SIDE are as loud as one voice). Each shape has its own
 * character: the squares' widths spread with DETUNE (50 +- 30 %), the triangles spread 3/5 as far (they beat slowly)
 * and their sum goes through a sine shaper that DETUNE drives from a soft rounding into folding.
 *
 * FLOAT-ONLY: All calculations use float, never double.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include "../../math/synt_math.hpp"
#include "../../oscillators/oscillator.hpp"

namespace synth
{
    class SuperEngine
    {
    public:
        static constexpr int kSides = 6;

        void setSampleRate(float sr) { sample_rate_ = sr > 0.0f ? sr : 48000.0f; }

        /// @brief The shape: the oscillator's wave
        void setWave(WaveType w)
        {
            switch (w)
            {
            case WaveType::SQUARE:
            case WaveType::PULSE: shape_ = Shape::Square; break;
            case WaveType::TRIANGLE: shape_ = Shape::Triangle; break;
            case WaveType::SINE: shape_ = Shape::Sine; break;
            default: shape_ = Shape::Saw; break;
            }
        }

        /// @brief DETUNE 0..1 (+-60 cents for the outer voices at 1)
        void setDetune(float d) { detune_ = std::clamp(d, 0.0f, 1.0f); }
        /// @brief What the matrix adds to DETUNE (1: the whole range)
        void setDetuneMod(float m) { detune_mod_ = m; }
        /// @brief SIDE 0..1: the six side voices' level against the centre's
        void setSide(float s) { side_ = std::clamp(s, 0.0f, 1.0f); }

        float detune() const { return detune_; }
        float side() const { return side_; }

        /// @brief A note from silence: the centre from 0, the sides from phases of their own (a seed per note)
        void noteOn(uint32_t seed)
        {
            centre_ = 0.0f;
            uint32_t x = seed | 1u;
            for (float &p : sides_)
            {
                x ^= x << 13, x ^= x >> 17, x ^= x << 5;
                p = static_cast<float>(x >> 8) * (1.0f / 16777216.0f);
            }
        }

        /// @brief One sample at the oscillator's frequency
        float process(float freq)
        {
            const float det = std::clamp(detune_ + detune_mod_, 0.0f, 1.0f);
            const float spread = (shape_ == Shape::Triangle ? 0.6f : 1.0f) * det * 0.0353f; // 60 cents: x1.0353
            const float dt = std::clamp(freq / sample_rate_, 0.0f, 0.5f);
            // equal power: the centre 1 .. 0.6, each side 0 .. 0.327 (0.6^2 + 6 x 0.327^2 = 1)
            const float g_centre = 1.0f - 0.4f * side_, g_side = 0.327f * side_;

            float s = 0.0f;
            for (int k = 0; k < kSides; ++k)
            {
                const float dk = dt * (1.0f + kOffsets[k] * spread);
                s += voice(sides_[k], dk, 0.5f + kOffsets[k] * det * 0.3f);
                sides_[k] += dk;
                sides_[k] -= static_cast<float>(static_cast<int>(sides_[k]));
            }
            float x = voice(centre_, dt, 0.5f) * g_centre + s * g_side;
            centre_ += dt;
            centre_ -= static_cast<float>(static_cast<int>(centre_));

            if (shape_ == Shape::Triangle) // a sine shaper: a soft rounding at DETUNE 0, folding at 1 (x 2.55)
                x = math::sinTurns(std::clamp(x, -1.0f, 1.0f) * (1.0f + det * 1.55f) * 0.25f);
            return x;
        }

    private:
        enum class Shape : uint8_t
        {
            Saw,
            Square,
            Triangle,
            Sine
        };

        // the side voices' places in the spread (never symmetric: no two beat together)
        static constexpr float kOffsets[kSides] = {-1.0f, -0.57f, -0.18f, 0.18f, 0.56f, 0.98f};

        static float blep(float t, float dt)
        {
            if (t < dt)
            {
                t /= dt;
                return t + t - t * t - 1.0f;
            }
            if (t > 1.0f - dt)
            {
                t = (t - 1.0f) / dt;
                return t * t + t + t + 1.0f;
            }
            return 0.0f;
        }

        // one voice at phase t (0..1), increment dt; pw: a square's width
        float voice(float t, float dt, float pw) const
        {
            switch (shape_)
            {
            case Shape::Square:
            {
                float u = t - pw;
                if (u < 0.0f)
                    u += 1.0f;
                return (t < pw ? 1.0f : -1.0f) + blep(t, dt) - blep(u, dt);
            }
            case Shape::Triangle: return t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
            case Shape::Sine: return math::sinTurns(t);
            default: return 2.0f * t - 1.0f - blep(t, dt);
            }
        }

        float sample_rate_ = 48000.0f;
        Shape shape_ = Shape::Saw;
        float detune_ = 0.4f, detune_mod_ = 0.0f, side_ = 0.63f;
        float centre_ = 0.0f;
        float sides_[kSides] = {0.11f, 0.47f, 0.83f, 0.29f, 0.65f, 0.02f};
    };
} // namespace synth
