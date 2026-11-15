/**
 * @file synt_math.hpp
 * @brief Float-only mathematical operations for audio synthesis
 *
 * This header enforces float-only precision in mathematical operations to
 * prevent double promotion errors and ensure consistent audio processing
 * performance. All mathematical functions are wrapped to use float variants.
 *
 * Features:
 * - Float-only mathematical constants (PI, TWO_PI)
 * - Wrapped float versions of standard math functions
 * - Compiler-specific double promotion error enforcement
 * - Static assertions for float precision validation
 *
 * @warning Include this header before any math operations to ensure float precision
 * @warning All audio calculations must use these wrapped functions, not std::
 */

#ifndef SYNTH_MATH_H
#define SYNTH_MATH_H

// This header enforces float-only precision in mathematical operations
// Include this before any math operations to ensure float precision

#include <cmath>

// For all compilers, use a consistent namespace approach to avoid conflicts
// This prevents macro redefinition issues that can break system headers

namespace synth
{
    /**
     * @brief Mathematical operations and constants for audio synthesis
     *
     * Provides float-only mathematical functions and constants optimized
     * for real-time audio processing. Prevents accidental double precision
     * usage that can degrade performance.
     *
     * @code
     * using namespace synth::math;
     * float frequency = 440.0f;
     * float phase = TWO_PI * frequency / sample_rate;
     * float sample = sin(phase);
     * @endcode
     */
    namespace math
    {
        /// Mathematical constant PI in single precision
        constexpr float PI = 3.14159265f;

        /// Mathematical constant 2*PI in single precision for frequency calculations
        constexpr float TWO_PI = 2.0f * PI;

        /**
         * @brief Single-precision sine function
         * @param x Angle in radians
         * @return Sine of x
         */
        inline float sin(float x) { return ::sinf(x); }

        // ============ Sine by table (modulation: LFOs, pan gains) ============
        // A cycle in 2048 points and a guard point, read with linear interpolation: the largest error is
        // (2 pi/2048)^2/8 ~ 1.2e-6 (-118 dB; 1024 points: 4.7e-6, -106 dB), far under what a modulation needs. 8 KB:
        // it stays in the L1 cache of a Cortex-A7 (32 KB). Not for coefficient design (filters keep sinf).
        namespace detail
        {
            struct SineTable
            {
                static constexpr int N = 2048;
                float v[N + 1];
                SineTable()
                {
                    for (int i = 0; i <= N; ++i)
                        v[i] = static_cast<float>(std::sin(6.283185307179586 * static_cast<double>(i) / N));
                }
            };
            inline const SineTable kSineTable{};
        } // namespace detail

        /// sin(2 pi t): t in turns (any value; the whole turns drop out)
        inline float sinTurns(float t)
        {
            constexpr int N = detail::SineTable::N;
            const float x = (t - std::floor(t)) * static_cast<float>(N);
            int i = static_cast<int>(x);
            float frac = x - static_cast<float>(i);
            if (i >= N) // t - floor(t) rounded up to 1 (a t just below a whole turn)
            {
                i = 0;
                frac = 0.0f;
            }
            const float *v = detail::kSineTable.v;
            return v[i] + (v[i + 1] - v[i]) * frac;
        }

        /// cos(2 pi t): the same table a quarter turn on
        inline float cosTurns(float t) { return sinTurns((t - std::floor(t)) + 0.25f); } // the whole turns out first: a quarter added to a large t rounds

        /**
         * @brief Single-precision cosine function
         * @param x Angle in radians
         * @return Cosine of x
         */
        inline float cos(float x) { return ::cosf(x); }

        /**
         * @brief Single-precision tangent function
         * @param x Angle in radians
         * @return Tangent of x
         */
        inline float tan(float x) { return ::tanf(x); }

        /**
         * @brief Single-precision floor function
         * @param x Input value
         * @return Largest integer less than or equal to x
         */
        inline float floor(float x) { return ::floorf(x); }

        /**
         * @brief Single-precision ceiling function
         * @param x Input value
         * @return Smallest integer greater than or equal to x
         */
        inline float ceil(float x) { return ::ceilf(x); }

        /**
         * @brief Single-precision power function
         * @param x Base value
         * @param y Exponent value
         * @return x raised to the power of y
         */
        inline float pow(float x, float y) { return ::powf(x, y); }

        /**
         * @brief Single-precision square root function
         * @param x Input value (must be non-negative)
         * @return Square root of x
         */
        inline float sqrt(float x) { return ::sqrtf(x); }

        /**
         * @brief Single-precision natural logarithm function
         * @param x Input value (must be positive)
         * @return Natural logarithm of x
         */
        inline float log(float x) { return ::logf(x); }

        /**
         * @brief Single-precision exponential function
         * @param x Exponent value
         * @return e raised to the power of x
         */
        inline float exp(float x) { return ::expf(x); }

        /**
         * @brief Single-precision absolute value function
         * @param x Input value
         * @return Absolute value of x
         */
        inline float fabs(float x) { return ::fabsf(x); }

        /**
         * @brief Single-precision floating-point remainder function
         * @param x Dividend
         * @param y Divisor
         * @return Floating-point remainder of x/y
         */
        inline float fmod(float x, float y) { return ::fmodf(x, y); }
    }
}

// Compiler-specific warnings for double promotion
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdouble-promotion"
#pragma GCC diagnostic error "-Wfloat-conversion"
#endif

#ifdef _MSC_VER
// Enable warnings for implicit conversions that might change values
#pragma warning(push)
#pragma warning(error : 4244) // conversion from 'double' to 'float', possible loss of data
#endif

// Static assertion to ensure float size
static_assert(sizeof(float) == 4, "Float must be 32-bit");

#endif // SYNTH_MATH_H
