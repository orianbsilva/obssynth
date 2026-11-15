// ObsSynth One: its values through the driver the plugin and the standalone panel share (app/obssynthone/one_driver.hpp),
// and its lite profile (lib/synth/engine_profile.hpp) against the full one.
#include <doctest/doctest.h>

#include "../src/app/obssynthone/one_driver.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

using namespace synth;

namespace
{
    constexpr int kRate = 48000;

    struct Take
    {
        double peak = 0, sum = 0, jump = 0;
        int frames = 0, bad = 0;
        std::vector<float> keep;
    };

    void render(ObsSynthOne &s, double seconds, Take &t)
    {
        std::vector<float> b(256 * 2);
        for (int done = 0; done < static_cast<int>(seconds * kRate); done += 256)
        {
            s.generateStereoBuffer(b.data(), 256);
            for (float x : b)
            {
                if (!std::isfinite(x))
                {
                    ++t.bad;
                    continue;
                }
                t.peak = std::max(t.peak, std::fabs(static_cast<double>(x)));
                t.sum += static_cast<double>(x) * static_cast<double>(x);
                t.keep.push_back(x);
            }
            t.frames += 256;
        }
    }
} // namespace

TEST_CASE("ObsSynth One: the driver takes every value of every factory preset, and the preset sounds")
{
    for (const auto &p : onep::presets())
    {
        CAPTURE(p.name);
        if (p.sample || std::strcmp(p.name, "Grain Cloud") == 0)
            continue; // a recording's: none loaded here
        ObsSynthOne s(static_cast<float>(kRate));
        onep::Driver dv{s};
        onep::applyAll(dv, onep::fromPreset(p, [](const char *) { return -1; }));
        Take t;
        s.noteOn(60, 0.8f);
        render(s, 0.5, t);
        CHECK(t.bad == 0);
        CHECK(t.peak > 0.01);
        CHECK(t.peak <= 1.0);
    }
}

TEST_CASE("ObsSynth One: lite (6 voices, control every 32 samples, 1x) sounds like full: finite, the same level, no clicks of its own")
{
    for (const auto &p : onep::presets())
    {
        CAPTURE(p.name);
        if (p.sample || std::strcmp(p.name, "Grain Cloud") == 0)
            continue;
        double rms[2], jumps[2];
        for (int lite = 0; lite < 2; ++lite)
        {
            const EngineProfile prof = lite ? kProfileLite : kProfileFull;
            ObsSynthOne s(static_cast<float>(kRate), prof.oversample);
            s.applyProfile(prof);
            CHECK(s.getVoiceLimit() == prof.maxVoices);
            CHECK(s.getControlRate() == prof.controlRate);
            onep::Driver dv{s};
            onep::applyAll(dv, onep::fromPreset(p, [](const char *) { return -1; }));
            Take t;
            for (int n : {57, 60, 64}) // within the lite polyphony: the two play the same notes
                s.noteOn(static_cast<uint8_t>(n), 0.7f);
            render(s, 0.8, t);
            for (int n : {57, 60, 64})
                s.noteOff(static_cast<uint8_t>(n));
            render(s, 0.4, t);
            CHECK(t.bad == 0);
            CHECK(t.peak <= 1.0);
            rms[lite] = 10 * std::log10(std::max(t.sum / (2.0 * t.frames), 1e-18));
            double jump = 0; // a click is a jump: the largest second difference of either channel
            for (size_t k = 4; k < t.keep.size(); ++k)
                jump = std::max(jump, std::fabs(static_cast<double>(t.keep[k]) - 2.0 * static_cast<double>(t.keep[k - 2]) + static_cast<double>(t.keep[k - 4])));
            jumps[lite] = jump;
        }
        CHECK(std::fabs(rms[1] - rms[0]) < 1.5); // dB
        CHECK(jumps[1] <= jumps[0] * 1.25 + 1e-3);
    }
}

TEST_CASE("ObsSynth One: SUPER is the engine's sixth place, and a state from before it keeps its engine")
{
    const int id = onep::idOf("OSC_PART_TYPE");
    const onep::Range &rg = onep::rangeOf(id);
    CHECK(static_cast<int>(std::lround(onep::toValue(rg, 1.0f))) == static_cast<int>(PartType::SUPER));
    // five places (0, 1/4 .. 1): STD WT FM GRAIN SMP, the same engines at n / 5
    for (int k = 0; k < 5; ++k)
    {
        const float n = onep::engineFromOlder(static_cast<float>(k) / 4.0f, 5);
        CHECK(static_cast<int>(std::lround(onep::toValue(rg, n))) == k);
    }
    CHECK(onep::engineFromOlder(0.6f, onep::kEngines) == doctest::Approx(0.6f));
}

TEST_CASE("ObsSynth One: SUPER sounds, bounded, and DETUNE changes it")
{
    auto take = [](float detune) {
        ObsSynthOne s(static_cast<float>(kRate));
        onep::Driver dv{s};
        onep::Values v = onep::defaults();
        v[static_cast<size_t>(onep::idOf("OSC_PART_TYPE"))] = 1.0f;
        v[static_cast<size_t>(onep::idOf("OSC_WT_MORPH"))] = detune;
        v[static_cast<size_t>(onep::idOf("OSC_GR_SPRAY"))] = 1.0f;
        onep::applyAll(dv, v);
        Take t;
        s.noteOn(57, 0.8f);
        render(s, 0.5, t);
        return t;
    };
    const Take flat = take(0.0f), wide = take(1.0f);
    CHECK(flat.bad == 0);
    CHECK(wide.bad == 0);
    CHECK(wide.peak > 0.01);
    CHECK(wide.peak <= 1.0);
    double diff = 0;
    for (size_t i = 0; i < std::min(flat.keep.size(), wide.keep.size()); ++i)
        diff += std::fabs(static_cast<double>(flat.keep[i] - wide.keep[i]));
    CHECK(diff > 1.0);
}
