// ObsSynthCT1, offline: the parameters (names, ranges, a knob's turn and back), the banks as pcm/ names them, the
// factory presets (each sounds, finite, under the limiter's ceiling, the same every time), the voices (freed when
// their carriers die), an algorithm on the matrix, a preset through its JSON.
#include <doctest/doctest.h>

#include "../src/products/obs_ct1/obs_synth_ct1.hpp"
#include "../src/app/obssynthct1/presets_io.hpp"

#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace synth::ct1;

namespace
{
    constexpr float kRate = 48000.0f;

    const std::vector<std::shared_ptr<const Table>> &banks()
    {
        static const auto b = loadBanks(SYNTH_PCM_DIR);
        return b;
    }

    // a chord held, then let go: the peak and whether anything was not finite
    std::vector<float> play(Engine &e, float hold, float tail)
    {
        std::vector<float> w;
        std::vector<float> b(512 * 2);
        for (int n : {48, 55, 60, 64})
            e.noteOn(n, 0.8f);
        for (int done = 0; done < static_cast<int>((hold + tail) * kRate); done += 512)
        {
            if (done >= static_cast<int>(hold * kRate) && done < static_cast<int>(hold * kRate) + 512)
                for (int n : {48, 55, 60, 64})
                    e.noteOff(n);
            e.render(b.data(), 512);
            w.insert(w.end(), b.begin(), b.end());
        }
        return w;
    }
} // namespace

TEST_CASE("CT1: every parameter has a name of its own, and a knob's turn comes back")
{
    std::set<std::string> names;
    for (const ParamInfo &p : paramInfo())
        names.insert(p.name);
    CHECK(names.size() == static_cast<size_t>(NUM_PARAMS));
    CHECK(paramId("OP3_POS") == opParam(2, OP_POS));
    CHECK(paramId("FM_2_1") == fmParam(1, 0));
    CHECK(paramId("VOLUME") == G_VOLUME);
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        CAPTURE(paramInfo()[static_cast<size_t>(i)].name);
        const float d = paramInfo()[static_cast<size_t>(i)].def;
        CHECK(fromNorm(i, toNorm(i, d)) == doctest::Approx(d).epsilon(0.01));
    }
    CHECK(fromNorm(opParam(0, OP_RATIO), toNorm(opParam(0, OP_RATIO), 3.5f)) == doctest::Approx(3.5f));
}

TEST_CASE("CT1: the banks of pcm/, by name, without their silent frames")
{
    REQUIRE(banks().size() >= 4);
    std::set<std::string> names;
    for (const auto &b : banks())
        names.insert(b->name());
    CHECK(names.count("ALL WAVES") == 1);
    CHECK(names.count("MORPH") == 1);
    CHECK(names.count("HARMONIC") == 1);
    CHECK(bankName("bank_sine_to_saw_16waves_2048samples_32float.wav") == "SINE TO SAW");
    for (const auto &b : banks())
    {
        CAPTURE(b->name());
        float last = 0; // the last frame sounds
        for (int i = 0; i < TABLE_N; ++i)
            last = std::max(last, std::fabs(b->sample(b->frames() - 1, i)));
        CHECK(last > 0.1f);
    }
}

TEST_CASE("CT1: the factory presets sound, finite, under the ceiling, the same every time")
{
    for (const Preset &pr : factoryPresets())
    {
        CAPTURE(pr.name);
        auto render = [&]
        {
            Engine e(kRate, banks());
            e.setAll(resolve(pr, banks()));
            return play(e, 1.5f, 1.0f);
        };
        const auto a = render(), b = render();
        float peak = 0;
        int bad = 0;
        for (float v : a)
        {
            bad += !std::isfinite(v);
            peak = std::max(peak, std::fabs(v));
        }
        CHECK(bad == 0);
        CHECK(20 * std::log10(std::max(1e-9f, peak)) > -30.0f);
        CHECK(20 * std::log10(peak) <= -0.9f);
        CHECK(a == b);
    }
}

TEST_CASE("CT1: a voice is freed when its carriers die, held or let go")
{
    Engine e(kRate, banks());
    Preset pr = factoryPresets()[0];
    Params p = resolve(pr, banks());
    p[static_cast<size_t>(opParam(0, OP_S))] = 0; // the carrier dies under the key
    p[static_cast<size_t>(opParam(0, OP_D))] = 0.05f;
    e.setAll(p);
    e.noteOn(60, 1.0f);
    std::vector<float> b(512 * 2);
    for (int i = 0; i < 20; ++i)
        e.render(b.data(), 512);
    CHECK(e.sounding() == 0);

    e.setAll(resolve(pr, banks()));
    e.noteOn(60, 1.0f);
    e.render(b.data(), 512);
    CHECK(e.sounding() == 1);
    e.noteOff(60);
    for (int i = 0; i < 300; ++i)
        e.render(b.data(), 512);
    CHECK(e.sounding() == 0);
}

TEST_CASE("CT1: an algorithm sets the matrix and the outs")
{
    Params p = defaultParams();
    p[static_cast<size_t>(fmParam(3, 3))] = 2.0f;
    for (size_t i = 0; i < algorithms().size(); ++i)
        if (std::string(algorithms()[i].name) == "TWO PAIRS")
            applyAlgorithm(p, static_cast<int>(i));
    CHECK(p[static_cast<size_t>(fmParam(1, 0))] == doctest::Approx(1.0f));
    CHECK(p[static_cast<size_t>(fmParam(3, 2))] == doctest::Approx(1.0f));
    CHECK(p[static_cast<size_t>(fmParam(3, 3))] == 0.0f); // what was there goes
    CHECK(p[static_cast<size_t>(outParam(0))] == 1.0f);
    CHECK(p[static_cast<size_t>(outParam(2))] == 1.0f);
    CHECK(p[static_cast<size_t>(outParam(1))] == 0.0f);
}

TEST_CASE("CT1: a preset survives its JSON, banks by name")
{
    Preset p = factoryPresets()[4];
    p.name = "Round Trip";
    p.params[static_cast<size_t>(fmParam(4, 1))] = 1.25f;
    Preset back;
    REQUIRE(synth::ct1app::presetFromJson(synth::ct1app::presetToJson(p), back));
    CHECK(back.name == "Round Trip");
    CHECK(back.bank == p.bank);
    for (int i = 0; i < NUM_PARAMS; ++i)
        CHECK(back.params[static_cast<size_t>(i)] == doctest::Approx(p.params[static_cast<size_t>(i)]));
}
