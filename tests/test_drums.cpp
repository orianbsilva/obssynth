// ObsDrums, rendered offline: every circuit and engine sounds, at a sane level, finite, and dies away; the choke
// group cuts the open hat; mute and solo; the factory kits render loud enough, under the limiter's ceiling, the
// same every time; and a kit survives its JSON file.
#include <doctest/doctest.h>

#include "../src/products/obs_drum/obs_drum_kits.hpp"
#include "../src/app/obsdrums/kits_io.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace synth;

namespace
{
    constexpr float kRate = 48000.0f;

    struct Take
    {
        float peak = 0, tail = 0; // tail: the loudest of the last 50 ms
        int bad = 0;
        double db() const { return 20 * std::log10(std::max(static_cast<double>(peak), 1e-9)); }
        double tailDb() const { return 20 * std::log10(std::max(static_cast<double>(tail), 1e-9)); }
    };

    Take render(ObsDrums &d, float seconds, std::vector<float> *keep = nullptr)
    {
        Take t;
        const int total = static_cast<int>(seconds * kRate), tailFrom = total - static_cast<int>(0.05f * kRate);
        std::vector<float> b(256 * 2);
        for (int done = 0; done < total; done += 256)
        {
            const int n = std::min(256, total - done);
            d.render(b.data(), n);
            for (int i = 0; i < n * 2; ++i)
            {
                const float v = b[static_cast<size_t>(i)];
                if (!std::isfinite(v))
                    ++t.bad;
                t.peak = std::max(t.peak, std::fabs(v));
                if (done + i / 2 >= tailFrom)
                    t.tail = std::max(t.tail, std::fabs(v));
            }
            if (keep)
                keep->insert(keep->end(), b.begin(), b.begin() + n * 2);
        }
        return t;
    }

    // a dry engine: no delay, no reverb, every pad at its defaults
    void dry(ObsDrums &d)
    {
        DrumMaster m;
        m.delayMix = 0;
        m.reverbMix = 0;
        d.setMaster(m);
    }

    std::shared_ptr<const GrainSource> tone() // a second of a decaying 220 Hz with some noise
    {
        auto s = std::make_shared<GrainSource>();
        s->rate = kRate;
        s->samples.resize(48000);
        uint32_t x = 1;
        for (size_t i = 0; i < s->samples.size(); ++i)
        {
            x ^= x << 13, x ^= x >> 17, x ^= x << 5;
            const float t = static_cast<float>(i) / kRate;
            s->samples[i] = std::exp(-3 * t) * (0.6f * std::sin(6.2831853f * 220 * t) + 0.1f * (static_cast<float>(x) / 4294967296.0f - 0.5f));
        }
        return s;
    }
} // namespace

TEST_CASE("ObsDrums: every circuit sounds, finite, and dies away")
{
    for (int k = 0; k < NUM_DRUM_KINDS; ++k)
    {
        CAPTURE(drumKindName(static_cast<DrumKind>(k)));
        ObsDrums d(kRate);
        dry(d);
        PadSettings s = d.pad(0);
        s.engine = DrumEngine::CIRCUIT;
        s.circuit = static_cast<DrumKind>(k);
        s.p.decay = 0.3f;
        d.setPad(0, s);
        d.hit(0, 1.0f);
        const Take t = render(d, 3.0f);
        CHECK(t.bad == 0);
        CHECK(t.db() > -30.0);
        CHECK(t.db() < 0.0);
        CHECK(t.tailDb() < -50.0);
    }
}

TEST_CASE("ObsDrums: FM, VA, SAMPLE and GRAIN sound")
{
    const DrumEngine engines[] = {DrumEngine::FM, DrumEngine::VA, DrumEngine::SAMPLE, DrumEngine::GRAIN};
    for (DrumEngine e : engines)
    {
        CAPTURE(drumEngineName(e));
        ObsDrums d(kRate);
        dry(d);
        d.setSource(0, tone());
        PadSettings s = d.pad(0);
        s.engine = e;
        s.p.decay = 0.3f;
        d.setPad(0, s);
        d.hit(0, 1.0f);
        const Take t = render(d, 4.0f);
        CHECK(t.bad == 0);
        CHECK(t.db() > -36.0);
        CHECK(t.db() < 0.0);
        CHECK(t.tailDb() < -50.0);
    }
}

TEST_CASE("ObsDrums: a pad plays by its note, and the bent knob changes the sound")
{
    ObsDrums a(kRate), b(kRate);
    dry(a);
    dry(b);
    PadSettings s = b.pad(0);
    s.p.bent = true;
    b.setPad(0, s);
    a.noteOn(a.pad(0).note, 1.0f);
    b.noteOn(b.pad(0).note, 1.0f);
    std::vector<float> wa, wb;
    CHECK(render(a, 0.5f, &wa).db() > -30.0);
    render(b, 0.5f, &wb);
    double diff = 0;
    for (size_t i = 0; i < wa.size(); ++i)
        diff += std::fabs(static_cast<double>(wa[i] - wb[i]));
    CHECK(diff > 1.0);
}

TEST_CASE("ObsDrums: the closed hat chokes the open one")
{
    // the open hat alone, then with the closed hat 100 ms in: 300 ms later the open hat must be (nearly) gone
    auto openHatAt = [](bool choke)
    {
        ObsDrums d(kRate);
        dry(d);
        int oh = -1, ch = -1;
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            if (d.pad(i).circuit == DrumKind::HAT_OPEN && d.pad(i).engine == DrumEngine::CIRCUIT)
                oh = i;
            if (d.pad(i).circuit == DrumKind::HAT_CLOSED && d.pad(i).engine == DrumEngine::CIRCUIT)
                ch = i;
        }
        REQUIRE(oh >= 0);
        REQUIRE(ch >= 0);
        PadSettings o = d.pad(oh), c = d.pad(ch);
        o.p.decay = 1.0f;
        o.choke = c.choke = choke ? 0 : -1;
        d.setPad(oh, o);
        d.setPad(ch, c);
        d.hit(oh, 1.0f);
        render(d, 0.1f);
        d.hit(ch, 1.0f);
        render(d, 0.3f);
        return d.padPeak(oh);
    };
    const float free = openHatAt(false), choked = openHatAt(true);
    CHECK(free > 0.01f);
    CHECK(choked < free * 0.01f);
}

TEST_CASE("ObsDrums: mute silences a pad, solo silences the others")
{
    ObsDrums d(kRate);
    dry(d);
    PadSettings s = d.pad(0);
    s.mute = true;
    d.setPad(0, s);
    d.hit(0, 1.0f);
    CHECK(render(d, 0.5f).peak < 1e-4f);

    ObsDrums e(kRate);
    dry(e);
    PadSettings t = e.pad(1);
    t.solo = true;
    e.setPad(1, t);
    e.hit(0, 1.0f);
    CHECK(render(e, 0.5f).peak < 1e-4f);
    e.hit(1, 1.0f);
    CHECK(render(e, 0.5f).peak > 0.01f);
}

TEST_CASE("ObsDrums: the factory kits, loud enough, under the ceiling, the same every time")
{
    for (const DrumKit &k : factoryDrumKits())
    {
        CAPTURE(k.name);
        auto play = [&](std::vector<float> &w)
        {
            ObsDrums d(kRate);
            applyDrumKit(d, k);
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
                d.setSource(i, tone());
            Take all;
            for (int step = 0; step < 32; ++step) // two bars of sixteenths at 120: every pad, the kick on the beat
            {
                if (step % 4 == 0)
                    d.hit(0, 1.0f);
                d.hit(1 + step % (NUM_DRUM_PADS - 1), 0.8f);
                const Take t = render(d, 0.125f, &w);
                all.peak = std::max(all.peak, t.peak);
                all.bad += t.bad;
            }
            return all;
        };
        std::vector<float> w1, w2;
        const Take t = play(w1);
        play(w2);
        CHECK(t.bad == 0);
        CHECK(t.db() > -20.0);
        CHECK(t.db() <= -0.9); // the limiter holds -1 dBFS
        CHECK(w1 == w2);
    }
}

TEST_CASE("ObsDrums: a kit survives its JSON file")
{
    DrumKit k = factoryDrumKits()[1];
    k.name = "Round Trip";
    k.pads[3].engine = DrumEngine::FM;
    k.pads[3].p.a = 0.123f;
    k.pads[5].choke = 2;
    k.pads[15].solo = false;
    k.samples[14] = "some.wav";
    k.master.glue = 0.77f;
    DrumKit back;
    REQUIRE(drums::kitFromJson(drums::kitToJson(k), back));
    CHECK(back.name == k.name);
    CHECK(back.pads[3].engine == DrumEngine::FM);
    CHECK(back.pads[3].p.a == doctest::Approx(0.123f));
    CHECK(back.pads[5].choke == 2);
    CHECK(back.samples[14] == "some.wav");
    CHECK(back.master.glue == doctest::Approx(0.77f));
    CHECK(back.routes.size() == k.routes.size());
    for (int i = 0; i < NUM_DRUM_PADS; ++i)
    {
        CHECK(back.pads[static_cast<size_t>(i)].note == k.pads[static_cast<size_t>(i)].note);
        CHECK(back.pads[static_cast<size_t>(i)].circuit == k.pads[static_cast<size_t>(i)].circuit);
    }
}
