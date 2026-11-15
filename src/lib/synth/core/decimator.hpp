#pragma once
// The way down from an oversampled render: a stereo low-pass of four biquad sections (an 8th-order Butterworth,
// both sides in one SSE2 register), then every OVERSAMPLE-th frame. Out of ObsSynth4D, for every product that
// renders oversampled (ObsDrum too). FLOAT-ONLY.
#include <cstddef>
#include <algorithm>
#include <array>
#include <cmath>

#ifndef SYNTH_FORCEINLINE
#if defined(_MSC_VER)
#include <immintrin.h>
#define SYNTH_FORCEINLINE __forceinline
#elif defined(__SSE2__)
#include <immintrin.h>
#define SYNTH_FORCEINLINE inline __attribute__((always_inline))
#else
#define SYNTH_FORCEINLINE inline
#endif
#elif defined(_MSC_VER) || defined(__SSE2__)
#include <immintrin.h>
#endif

namespace synth
{
    // ── Stereo biquad decimation filter ─────────────────────────────────
    // Holds L+R state interleaved in __m128 registers for SSE2 parallel
    // processing of both channels in one SIMD biquad computation.
    // Coefficients are shared (L and R are initialized identically).
    struct alignas(16) StereoBiquad
    {
        // Coefficients (LP, fixed after init — no smoothing needed)
        float b0{1.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};

        // Per-channel delay state as SSE registers: low two floats = {L, R}
#if defined(_MSC_VER) || defined(__SSE2__)
        __m128 x1v{}, x2v{}, y1v{}, y2v{}; // {xL, xR, 0, 0}
#else
        float x1L{}, x1R{}, x2L{}, x2R{};
        float y1L{}, y1R{}, y2L{}, y2R{};
#endif

        /**
         * @brief Compute LP biquad coefficients for the decimation anti-alias filter
         * @param cutoffHz Anti-alias cutoff frequency (Hz)
         * @param sampleRate Filter sample rate (oversampled SR)
         * @param Q Filter Q (default 0.707 = Butterworth)
         */
        void init(float cutoffHz, float sampleRate, float Q = 0.707f)
        {
            constexpr float TWOPI = 6.28318530717959f;
            const float w = TWOPI * cutoffHz / sampleRate;
            const float sw = sinf(w), cw = cosf(w);
            const float alp = sw / (2.0f * Q);
            const float ia0 = 1.0f / (1.0f + alp);
            b0 = ((1.0f - cw) * 0.5f) * ia0;
            b1 = (1.0f - cw) * ia0;
            b2 = b0;
            a1 = (-2.0f * cw) * ia0;
            a2 = (1.0f - alp) * ia0;
#if defined(_MSC_VER) || defined(__SSE2__)
            x1v = x2v = y1v = y2v = _mm_setzero_ps();
#else
            x1L = x1R = x2L = x2R = y1L = y1R = y2L = y2R = 0.f;
#endif
        }

        /**
         * @brief Process one interleaved stereo sample using SSE2 (L and R in parallel)
         * @param inL Left input sample
         * @param inR Right input sample
         * @param outL Left output sample
         * @param outR Right output sample
         */
        SYNTH_FORCEINLINE void processPair(float inL, float inR, float &outL, float &outR)
        {
#if defined(_MSC_VER) || defined(__SSE2__)
            // Pack input and broadcast coefficients
            const __m128 in = _mm_set_ps(0.f, 0.f, inR, inL);
            const __m128 b0v = _mm_set1_ps(b0);
            const __m128 b1v = _mm_set1_ps(b1);
            const __m128 b2v = _mm_set1_ps(b2);
            const __m128 a1v = _mm_set1_ps(a1);
            const __m128 a2v = _mm_set1_ps(a2);

            // y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2  (L and R in one pass)
            const __m128 out = _mm_add_ps(
                _mm_mul_ps(b0v, in),
                _mm_sub_ps(
                    _mm_add_ps(_mm_mul_ps(b1v, x1v), _mm_mul_ps(b2v, x2v)),
                    _mm_add_ps(_mm_mul_ps(a1v, y1v), _mm_mul_ps(a2v, y2v))));

            // Update state
            x2v = x1v;
            x1v = in;
            y2v = y1v;
            y1v = out;

            // Extract L (lane 0) and R (lane 1)
            outL = _mm_cvtss_f32(out);
            outR = _mm_cvtss_f32(_mm_shuffle_ps(out, out, _MM_SHUFFLE(1, 1, 1, 1)));
#else
            // Scalar fallback (non-SSE2 platforms)
            outL = b0 * inL + b1 * x1L + b2 * x2L - a1 * y1L - a2 * y2L;
            x2L = x1L;
            x1L = inL;
            y2L = y1L;
            y1L = outL;
            outR = b0 * inR + b1 * x1R + b2 * x2R - a1 * y1R - a2 * y2R;
            x2R = x1R;
            x1R = inR;
            y2R = y1R;
            y1R = outR;
#endif
        }
    };

    /**
     * @brief Decimate an oversampled stereo interleaved buffer to native rate
     *
     * All oversampled frames are fed through the SSE2 anti-alias biquad;
     * only the last output of each group is written to the output buffer.
     * AVX2 prefetch hints are issued one cache line ahead to hide memory latency.
     *
     * @param decim   StereoBiquad decimation filter state
     * @param src     Oversampled interleaved input  (osFrames * 2 floats)
     * @param dst     Native-rate interleaved output (frames   * 2 floats)
     * @param frames  Number of output frames
     * @param oversample  Oversampling factor (e.g. 2)
     */
    SYNTH_FORCEINLINE void decimateInterleaved(
        std::array<StereoBiquad, 4> &decim,
        const float *__restrict src,
        float *__restrict dst,
        int frames, int oversample)
    {
        for (int i = 0; i < frames; ++i)
        {
#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
            // Prefetch next input cache line while processing current
            _mm_prefetch(
                reinterpret_cast<const char *>(src + (i + 8) * oversample * 2),
                _MM_HINT_T0);
#endif
            float oL = 0.f, oR = 0.f;
            for (int j = 0; j < oversample; ++j)
            {
                const int k = i * oversample + j;
                float l = src[k * 2], r = src[k * 2 + 1];
                for (auto &d : decim)
                    d.processPair(l, r, l, r);
                oL = l;
                oR = r;
            }
            dst[i * 2] = oL;
            dst[i * 2 + 1] = oR;
        }
    }

    // the four sections, set for a native rate and an oversampling factor: Butterworth at 20 kHz (or 0.45 of the rate)
    class StereoDecimator
    {
    public:
        void init(float nativeRate, int oversample)
        {
            oversample_ = oversample;
            const float qs[4] = {0.5098f, 0.6013f, 0.9000f, 2.5629f};
            for (int k = 0; k < 4; ++k)
                sections_[static_cast<size_t>(k)].init(std::min(20000.0f, nativeRate * 0.45f), nativeRate * static_cast<float>(oversample), qs[k]);
        }
        // src: frames x oversample interleaved stereo frames; dst: frames interleaved stereo frames
        void process(const float *src, float *dst, int frames) { decimateInterleaved(sections_, src, dst, frames, oversample_); }

    private:
        std::array<StereoBiquad, 4> sections_{};
        int oversample_ = 2;
    };
} // namespace synth
