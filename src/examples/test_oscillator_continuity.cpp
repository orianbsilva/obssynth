#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <vector>
#include "oscillators/oscillator.hpp"

using synth::Oscillator;
using synth::WaveType;

// Simple requirement macro that is active in all build types (unlike assert with NDEBUG)
#define REQUIRE(cond)                                                                       \
    do                                                                                      \
    {                                                                                       \
        if (!(cond))                                                                        \
        {                                                                                   \
            std::fprintf(stderr, "Test failure: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            std::abort();                                                                   \
        }                                                                                   \
    } while (0)

static inline float absdiff(float a, float b) { return static_cast<float>(fabs(a - b)); }

static void test_basic_continuity_sine()
{
    const float sample_rate = 48000.0f;
    const float frequency = 440.0f;
    const int N = 512;
    const int M = 512;
    const float tol = 1e-5f;

    Oscillator oscA(frequency, WaveType::SINE, sample_rate);
    Oscillator oscRef(frequency, WaveType::SINE, sample_rate);

    std::vector<float> buf1(N);
    std::vector<float> buf2(M);
    std::vector<float> ref(N + M);

    oscA.generateBuffer(buf1.data(), N);
    oscRef.generateBuffer(ref.data(), N + M);
    oscA.generateBuffer(buf2.data(), M);

    float expected_border_sample = ref[N];
    float actual_border_sample = buf2[0];

    REQUIRE(absdiff(expected_border_sample, actual_border_sample) < tol);
}

static void test_fractional_frequency_continuity()
{
    const float sample_rate = 48000.0f;
    const float frequency = 441.3f; // non-integer cycles per block
    const int N = 300;
    const int M = 300;
    const float tol = 1e-5f;

    Oscillator oscA(frequency, WaveType::SINE, sample_rate);
    Oscillator oscRef(frequency, WaveType::SINE, sample_rate);

    std::vector<float> buf1(N);
    std::vector<float> buf2(M);
    std::vector<float> ref(N + M);

    oscA.generateBuffer(buf1.data(), N);
    oscRef.generateBuffer(ref.data(), N + M);
    oscA.generateBuffer(buf2.data(), M);

    REQUIRE(absdiff(ref[N], buf2[0]) < tol);
}

static void test_all_wave_types_border()
{
    const float sample_rate = 48000.0f;
    const int N = 257;
    const int M = 257;
    const WaveType types[] = {WaveType::SINE, WaveType::SAW, WaveType::SQUARE};
    const float freq = 123.45f;
    const float tol = 1e-5f;

    for (auto wt : types)
    {
        Oscillator oscA(freq, wt, sample_rate);
        Oscillator oscRef(freq, wt, sample_rate);

        std::vector<float> buf1(N), buf2(M), ref(N + M);
        oscA.generateBuffer(buf1.data(), N);
        oscRef.generateBuffer(ref.data(), N + M);
        oscA.generateBuffer(buf2.data(), M);

        float local_tol = (wt == WaveType::SQUARE) ? 1e-4f : tol; // square has discontinuities
        REQUIRE(absdiff(ref[N], buf2[0]) < local_tol);
    }
}

static void test_multi_block_sequence()
{
    const float sample_rate = 48000.0f;
    const float frequency = 1000.0f;
    const int block = 256;
    const int blocks = 10;
    const float tol = 1e-5f;

    Oscillator oscA(frequency, WaveType::SINE, sample_rate);
    Oscillator oscRef(frequency, WaveType::SINE, sample_rate);

    std::vector<float> ref(block * blocks);
    oscRef.generateBuffer(ref.data(), (int)ref.size());

    for (int b = 0; b < blocks; ++b)
    {
        std::vector<float> tmp(block);
        oscA.generateBuffer(tmp.data(), block);
        REQUIRE(absdiff(tmp[0], ref[b * block]) < tol);
    }
}

static void test_range_limits()
{
    const float sample_rate = 48000.0f;
    const float frequency = 8000.0f; // below Nyquist/3
    const int N = 1024;

    Oscillator oscS(frequency, WaveType::SINE, sample_rate);
    Oscillator oscSa(frequency, WaveType::SAW, sample_rate);
    Oscillator oscSq(frequency, WaveType::SQUARE, sample_rate);

    std::vector<float> bS(N), bSa(N), bSq(N);
    oscS.generateBuffer(bS.data(), N);
    oscSa.generateBuffer(bSa.data(), N);
    oscSq.generateBuffer(bSq.data(), N);

    auto check_range = [](const std::vector<float> &v)
    {
        for (float s : v)
        {
            REQUIRE(s >= -1.0001f && s <= 1.0001f);
        }
    };

    check_range(bS);
    check_range(bSa);
    check_range(bSq);
}

static void test_phase_reset_behavior()
{
    const float sample_rate = 48000.0f;
    const float frequency = 220.0f;
    const int N = 64;
    const float tol = 1e-6f;

    Oscillator osc(frequency, WaveType::SINE, sample_rate);
    std::vector<float> b1(N), b2(N);
    osc.generateBuffer(b1.data(), N);
    osc.resetPhase();
    osc.generateBuffer(b2.data(), N);

    REQUIRE(absdiff(b1[0], b2[0]) < tol);
}

int main()
{
    test_basic_continuity_sine();
    test_fractional_frequency_continuity();
    test_all_wave_types_border();
    test_multi_block_sequence();
    test_range_limits();
    test_phase_reset_behavior();
    std::puts("All oscillator continuity tests passed.");
    return 0;
}
