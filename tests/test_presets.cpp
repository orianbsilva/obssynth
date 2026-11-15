// The factory presets, rendered offline: each sounds, at a sane level, with no NaN and no DC; and two
// regressions found by rendering them: voice envelopes timed at 1 Hz (sustain-0 presets were silent), and a
// mix gain that moved with the number of voices.
#include <doctest/doctest.h>

#include "../src/products/obs_synth/obs_synth_4d.hpp"
#include "../src/products/obs_synth/obs_synth_preset_bank.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <vector>

using namespace synth;

namespace
{
    constexpr int kRate = 48000;

    struct Take
    {
        double peak = 0, sum = 0, dc = 0;
        int frames = 0, bad = 0;
        double db() const { return 20 * std::log10(std::max(peak, 1e-9)); }
    };

    // render seconds of s into t (left and right)
    void render(ObsSynth4D &s, double seconds, Take &t, std::vector<float> *keep = nullptr)
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
                const double v = static_cast<double>(x);
                t.peak = std::max(t.peak, std::fabs(v));
                t.sum += v * v;
                t.dc += v;
            }
            if (keep)
                keep->insert(keep->end(), b.begin(), b.end());
            t.frames += 256;
        }
    }
} // namespace

// the wavetable banks as obssynth4d loads them (the wavetable presets are made for these)
void loadPanelBanks(ObsSynth4D &s)
{
    const std::string dir = SYNTH_PCM_DIR;
    const char *names[4] = {"bank_morph_32waves_1024samples_32float.wav", "bank_morph_32waves_1024samples_32float.wav",
                            "bank_harmonic_16waves_1024samples_32float.wav", "bank_all_waves_32waves_1024samples_32float.wav"};
    for (int o = 0; o < 4; ++o)
        s.loadWtBank(o, dir + "/" + names[o]);
    for (int o = 0; o < 4; ++o) // the grains' recording as obssynth4d's default (the ALSA voice), when the system has it
        s.loadGrainSource(o, "/usr/share/sounds/alsa/Rear_Center.wav");
}

bool grainsWithout(int preset) // a granular preset, and no recording to read on this system
{
    std::FILE *f = std::fopen("/usr/share/sounds/alsa/Rear_Center.wav", "rb");
    if (f)
    {
        std::fclose(f);
        return false;
    }
    for (int o = 0; o < 4; ++o)
        if (PRESET_BANK[preset]->partTypes[o] == PartType::GRANULAR)
            return true;
    return false;
}

TEST_CASE("Presets: every factory preset sounds, finite, without DC, between -24 and 0 dBFS")
{
    for (int i = 0; i < PRESET_BANK_SIZE; ++i)
    {
        CAPTURE(PRESET_BANK[i]->name);
        if (grainsWithout(i))
            continue;
        ObsSynth4D s(static_cast<float>(kRate));
        loadPanelBanks(s);
        s.selectPreset(i);
        Take t;
        s.noteOn(60, 0.8f);
        render(s, 0.6, t);
        for (int n : {57, 60, 64, 69})
            s.noteOn(static_cast<uint8_t>(n), 0.6f);
        render(s, 1.0, t);
        for (int n : {57, 60, 64, 69})
            s.noteOff(static_cast<uint8_t>(n));
        s.noteOff(60);
        render(s, 1.0, t);
        CHECK(t.bad == 0);
        CHECK(t.db() > -24.0);
        CHECK(t.peak <= 1.0);
        CHECK(std::fabs(t.dc / (2.0 * t.frames)) < 1e-3);
    }
}

TEST_CASE("Presets: a voice envelope with sustain 0 still sounds through its decay")
{
    // Crystal Pluck: amp decays of 0.25 to 0.8 s down to sustain 0. With the envelopes timed at 1 Hz the decay
    // was instant and the note silent past its first milliseconds.
    int pluck = -1;
    for (int i = 0; i < PRESET_BANK_SIZE; ++i)
        if (std::string(PRESET_BANK[i]->name) == "Crystal Pluck")
            pluck = i;
    REQUIRE(pluck >= 0);
    ObsSynth4D s(static_cast<float>(kRate));
    s.selectPreset(pluck);
    s.noteOn(60, 0.8f);
    Take first, later;
    render(s, 0.05, first);
    render(s, 0.10, later); // 50 to 150 ms in: well inside the decays
    CHECK(first.db() > -30.0);
    CHECK(later.db() > first.db() - 12.0);
}

TEST_CASE("Presets: the voices' mix gain does not move when a second note comes in")
{
    // Single Sine: one note held; a second, an octave up, joins. The first note's level must stay where it
    // was (the mix used to be scaled by 1/sqrt(voices playing), so it dropped 3 dB at once).
    int sine = -1;
    for (int i = 0; i < PRESET_BANK_SIZE; ++i)
        if (std::string(PRESET_BANK[i]->name) == "Single Sine")
            sine = i;
    REQUIRE(sine >= 0);
    ObsSynth4D alone(static_cast<float>(kRate)), joined(static_cast<float>(kRate));
    alone.selectPreset(sine);
    joined.selectPreset(sine);
    alone.noteOn(48, 0.8f);
    joined.noteOn(48, 0.8f);
    Take a0, j0;
    render(alone, 0.5, a0);
    render(joined, 0.5, j0);
    joined.noteOn(96, 0.01f); // a second voice, nearly silent: it may only add, never scale the first down
    Take a1, j1;
    render(alone, 0.3, a1);
    render(joined, 0.3, j1);
    CHECK(std::sqrt(j1.sum) == doctest::Approx(std::sqrt(a1.sum)).epsilon(0.02));
}

TEST_CASE("Envelope: the attack peaks at its time, the release falls 60 dB in its time")
{
    ADSREnvelope e(48000.0f);
    e.setADSR(0.1f, 0.2f, 0.5f, 0.3f);
    e.noteOn();
    int peakAt = -1;
    for (int i = 0; i < 48000 && peakAt < 0; ++i)
        if (e.getNextSample() >= 1.0f)
            peakAt = i;
    CHECK(peakAt == doctest::Approx(4800).epsilon(0.05));
    for (int i = 0; i < 48000; ++i)
        e.getNextSample(); // into the sustain
    CHECK(e.getCurrentAmplitude() == doctest::Approx(0.5f).epsilon(0.01));
    e.noteOff();
    for (int i = 0; i < 14400; ++i) // 0.3 s
        e.getNextSample();
    CHECK(20.0 * std::log10(static_cast<double>(e.getCurrentAmplitude()) / 0.5) == doctest::Approx(-60.0).epsilon(0.05));
}

namespace
{
    // the share of a take above about 2 kHz (a one-pole high-pass's energy over the whole)
    double brightness(const std::vector<float> &lr)
    {
        double all = 0, high = 0, hp = 0, prev = 0;
        for (size_t i = 0; i < lr.size(); i += 2)
        {
            const double x = static_cast<double>(lr[i]);
            hp = 0.77 * (hp + x - prev);
            prev = x;
            all += x * x;
            high += hp * hp;
        }
        return high / std::max(all, 1e-12);
    }

    Preset4D quiet()
    {
        Preset4D p = *PRESET_BANK[0];
        for (int o = 0; o < 4; ++o)
        {
            p.levels[o] = 0.0f;
            p.ampA[o] = 0.001f;
            p.ampD[o] = 0.1f;
            p.ampS[o] = 1.0f;
            p.ampR[o] = 0.05f;
            p.filterEnvAmounts[o] = 0.0f;
            p.waves[o] = WaveType::SAW;
        }
        p.delay.mix = 0.0f;
        p.tube.mix = 0.0f;
        p.numRoutes = 0;
        return p;
    }
} // namespace

TEST_CASE("Per-voice: velocity reaches the matrix (a harder note opens its own filter)")
{
    Preset4D p = quiet();
    p.levels[0] = 0.8f;
    p.cutoffs[0] = 400.0f;
    p.routes[0] = {ObsSynthSrc::VELOCITY, ObsSynthDst::OSC1_CUTOFF, 0.75f}; // +3 octaves at full velocity
    p.numRoutes = 1;
    auto take = [&](float vel)
    {
        ObsSynth4D s(static_cast<float>(kRate));
        s.applyPreset(p);
        s.noteOn(48, vel);
        Take t;
        std::vector<float> keep;
        render(s, 0.3, t, &keep);
        return brightness(keep);
    };
    CHECK(take(1.0f) > 3.0 * take(0.1f));
}

TEST_CASE("Per-voice: an oscillator's pan stays its own after a note")
{
    Preset4D p = quiet();
    p.levels[0] = 0.8f;
    p.pans[0] = 0.1f; // well left
    ObsSynth4D s(static_cast<float>(kRate));
    s.applyPreset(p);
    s.noteOn(60, 0.8f);
    Take t;
    std::vector<float> lr;
    render(s, 0.3, t, &lr);
    double l = 0, r = 0;
    for (size_t i = 0; i < lr.size(); i += 2)
    {
        l += static_cast<double>(lr[i]) * static_cast<double>(lr[i]);
        r += static_cast<double>(lr[i + 1]) * static_cast<double>(lr[i + 1]);
    }
    CHECK(l > 4.0 * r);
}

TEST_CASE("Vector: the pad in a corner plays that corner's oscillator")
{
    Preset4D p = quiet();
    for (int o = 0; o < 4; ++o)
    {
        p.levels[o] = 0.8f;
        p.waves[o] = WaveType::SINE;
        p.coarseTune[o] = 12.0f * static_cast<float>(o); // 1: C4, 2: C5, 3: C6, 4: C7
    }
    p.vector.on = true;
    p.vector.count = 0; // no path: the pad alone places it
    auto pitch = [&](float x, float y)
    {
        ObsSynth4D s(static_cast<float>(kRate));
        s.applyPreset(p);
        s.getEngine().setVectorPad(x, y);
        s.noteOn(60, 0.8f);
        Take t;
        std::vector<float> lr;
        render(s, 0.4, t, &lr);
        int crossings = 0;
        for (size_t i = 2 * 4800; i + 2 < lr.size(); i += 2) // after 0.1 s
            if (lr[i] < 0.0f && lr[i + 2] >= 0.0f)
                ++crossings;
        return static_cast<double>(crossings) / 0.3; // Hz, near enough
    };
    CHECK(pitch(0.0f, 1.0f) == doctest::Approx(261.6).epsilon(0.05)); // corner 1
    CHECK(pitch(1.0f, 1.0f) == doctest::Approx(523.3).epsilon(0.05)); // corner 2
    CHECK(pitch(1.0f, 0.0f) == doctest::Approx(2093.0).epsilon(0.05)); // corner 4
}

TEST_CASE("Granular: grains follow the note's pitch, and the level holds across densities")
{
    auto src = std::make_shared<GrainSource>();
    src->rate = 48000.0f;
    src->samples.resize(48000);
    for (size_t i = 0; i < src->samples.size(); ++i) // a C4 sine: the root, at its own speed
        src->samples[i] = std::sin(6.2831853f * 261.626f * static_cast<float>(i) / 48000.0f);
    auto run = [&](float hz, float density, double &freq)
    {
        GrainEngine g;
        g.setSampleRate(96000.0f);
        g.setSource(src);
        g.setPosition(0.5f);
        g.setSize(0.1f);
        g.setDensity(density);
        g.setSpray(0.0f);
        g.noteOn(7);
        double sum = 0;
        int crossings = 0;
        float prev = 0.0f;
        for (int i = 0; i < 96000; ++i)
        {
            const float y = g.process(hz);
            if (i > 9600)
            {
                sum += static_cast<double>(y) * static_cast<double>(y);
                if (prev < 0.0f && y >= 0.0f)
                    ++crossings;
            }
            prev = y;
        }
        freq = crossings / 0.9;
        return std::sqrt(sum / (96000 - 9600));
    };
    double f1, f2, f3;
    const double r1 = run(523.25f, 20.0f, f1); // C5 from a C4 source
    const double r2 = run(261.626f, 20.0f, f2);
    const double r3 = run(261.626f, 80.0f, f3);
    CHECK(f1 == doctest::Approx(523.25).epsilon(0.03));
    CHECK(f2 == doctest::Approx(261.6).epsilon(0.03));
    CHECK(r1 > 0.2);
    CHECK(r3 / r2 == doctest::Approx(1.0).epsilon(0.5)); // four times the grains, not four times the level
}

// ── regressions found by review ─────────────────────────────────────────────────────────────────────

TEST_CASE("Wavetable: the morph reaches the whole bank")
{
    ObsSynth4D s(static_cast<float>(kRate));
    s.loadWtBank(0, std::string(SYNTH_PCM_DIR) + "/bank_all_waves_32waves_1024samples_32float.wav");
    Preset4D p = quiet();
    p.levels[0] = 0.8f;
    p.partTypes[0] = PartType::WAVETABLE;
    p.cutoffs[0] = 20000.0f;
    auto bright = [&](float table)
    {
        p.wtMorphPos[0] = table;
        ObsSynth4D t(static_cast<float>(kRate));
        t.loadWtBank(0, std::string(SYNTH_PCM_DIR) + "/bank_all_waves_32waves_1024samples_32float.wav");
        t.applyPreset(p);
        t.noteOn(48, 0.8f);
        Take k;
        std::vector<float> keep;
        render(t, 0.2, k, &keep);
        return brightness(keep);
    };
    // table 0 is a sine, table 31 a pulse: if the top half were out of reach, 31 would sound like ~15 (a square)
    CHECK(bright(31.0f) > 2.0 * bright(0.0f));
    CHECK(std::fabs(bright(31.0f) - bright(15.0f)) > 0.01);
}

TEST_CASE("Voices: a stolen voice fades instead of jumping; a release set alone takes effect")
{
    FullVoiceAllocator a(96000.0f);
    a.configureChains([](OscillatorChain &c, int) { c.setAmpAttack(0.001f); c.setAmpDecay(0.1f); c.setAmpSustain(1.0f); c.setAmpRelease(0.05f); });
    for (int n = 0; n < 32; ++n)
        a.noteOn(48 + n, 0.9f);
    float l, r, prevL = 0.0f, maxSteady = 0.0f, maxSteal = 0.0f;
    for (int i = 0; i < 9600; ++i)
    {
        a.processStereo(l, r);
        if (i > 4800)
            maxSteady = std::max(maxSteady, std::fabs(l - prevL));
        prevL = l;
    }
    a.noteOn(100, 0.9f); // one more than there are voices: a steal
    for (int i = 0; i < 2000; ++i)
    {
        a.processStereo(l, r);
        maxSteal = std::max(maxSteal, std::fabs(l - prevL));
        prevL = l;
    }
    CHECK(maxSteal < 3.0f * maxSteady);

    ADSREnvelope e(48000.0f);
    e.setADSR(0.0f, 0.0f, 1.0f, 0.0f);
    e.setReleaseTime(1.0f); // alone
    e.noteOn();
    e.getNextSample();
    e.noteOff();
    for (int i = 0; i < 24000; ++i) // half a second: still sounding
        e.getNextSample();
    CHECK(e.getCurrentAmplitude() > 0.01f);
}

TEST_CASE("Voices: oldest-first stealing keeps the newest notes of a chord played at once")
{
    FullVoiceAllocator a(96000.0f, VoiceAllocationStrategy::OLDEST);
    const int n = 32;
    for (int k = 0; k < n + 4; ++k) // all in one block: every age 0
        a.noteOn(30 + k, 0.8f);
    float l, r;
    for (int i = 0; i < 2000; ++i)
        a.processStereo(l, r);
    for (int k = 0; k < 4; ++k) // the first four went
        CHECK(a.findVoiceByNote(30 + k) == nullptr);
    for (int k = n; k < n + 4; ++k) // the last four play
        CHECK(a.findVoiceByNote(30 + k) != nullptr);
}

TEST_CASE("Engine: pitch bend moves a note; an FM preset over an FM part stays FM")
{
    Preset4D p = quiet();
    p.levels[0] = 0.8f;
    p.waves[0] = WaveType::SINE;
    auto pitch = [&](float bend)
    {
        ObsSynth4D s(static_cast<float>(kRate));
        s.applyPreset(p);
        s.noteOn(60, 0.8f);
        s.getEngine().setPitchBend(bend);
        Take t;
        std::vector<float> lr;
        render(s, 0.4, t, &lr);
        int crossings = 0;
        for (size_t i = 2 * 4800; i + 2 < lr.size(); i += 2)
            if (lr[i] < 0.0f && lr[i + 2] >= 0.0f)
                ++crossings;
        return crossings / 0.3;
    };
    CHECK(pitch(2.0f) == doctest::Approx(261.6 * 1.1225).epsilon(0.03));

    Preset4D fm = quiet();
    fm.levels[0] = 0.8f;
    fm.partTypes[0] = PartType::FM_ENGINE;
    fm.fmParams[0].algorithm = FMAlgorithm::TWO_OP_SIMPLE;
    fm.fmParams[0].modIndex = 5.0f;
    ObsSynth4D s(static_cast<float>(kRate));
    s.applyPreset(fm);
    s.applyPreset(fm); // again: the part was FM already
    s.noteOn(48, 0.8f);
    Take t;
    std::vector<float> keep;
    render(s, 0.2, t, &keep);
    CHECK(brightness(keep) > 0.05); // FM index 5: rich; a plain saw-turned-sine would not be
}

TEST_CASE("Engine: the mono path plays at the right pitch")
{
    Preset4D p = quiet();
    p.levels[0] = 0.8f;
    p.waves[0] = WaveType::SINE;
    ObsSynth4D s(static_cast<float>(kRate));
    s.applyPreset(p);
    s.noteOn(69, 0.8f);
    std::vector<float> m(48000);
    s.generateBuffer(m.data(), 48000);
    int crossings = 0;
    for (size_t i = 4800; i + 1 < m.size(); ++i)
        if (m[i] < 0.0f && m[i + 1] >= 0.0f)
            ++crossings;
    CHECK(crossings / 0.9 == doctest::Approx(440.0).epsilon(0.03));
}

TEST_CASE("Matrix: slow LFOs, a self-depth that does not feed back, the wheel bringing an LFO in")
{
    LFO slow(0.1f, LFOWaveType::SINE, 96000.0f);
    CHECK(slow.getFrequency() == doctest::Approx(0.1f)); // it was clamped to 1 Hz

    auto changes = [](float wheel, bool selfRoute)
    {
        Preset4D p = quiet();
        p.levels[0] = 0.8f;
        p.waves[0] = WaveType::SINE;
        p.lfos[0] = {5.0f, LFOWaveType::SINE};
        p.routes[0] = {ObsSynthSrc::MOD_WHEEL, ObsSynthDst::LFO1_DEPTH, 1.0f};
        p.routes[1] = {ObsSynthSrc::LFO1, ObsSynthDst::OSC1_PITCH, 0.1f};
        p.numRoutes = 2;
        if (selfRoute)
            p.routes[p.numRoutes++] = {ObsSynthSrc::LFO1, ObsSynthDst::LFO1_DEPTH, 2.0f};
        ObsSynth4D s(static_cast<float>(kRate));
        s.applyPreset(p);
        s.setModWheel(wheel);
        s.noteOn(60, 0.8f);
        std::vector<float> b(512);
        int flips = 0;
        float prev = 0.0f, lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < 400; ++i)
        {
            s.generateStereoBuffer(b.data(), 256);
            const float v = s.getModMatrix().getSourceValue((int)ObsSynthSrc::LFO1);
            if ((v > 0.0f) != (prev > 0.0f))
                ++flips;
            prev = v;
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        return std::make_pair(flips, hi - lo);
    };
    CHECK(changes(0.0f, false).second < 0.01f); // the wheel down: the LFO is not there
    CHECK(changes(1.0f, false).second > 1.5f);  // up: whole
    CHECK(changes(1.0f, true).first < 200);     // its own depth: no buzz (block-rate samples of a 5 Hz LFO)
}

TEST_CASE("Matrix: a mixer destination moves its oscillator; a muted slot stays muted; fine tune keeps the coarse")
{
    Preset4D p = quiet();
    p.levels[0] = 0.8f;
    p.routes[0] = {ObsSynthSrc::XY_X, ObsSynthDst::MIX_CH1_LVL, -1.0f};
    p.numRoutes = 1;
    auto level = [&](float x)
    {
        ObsSynth4D s(static_cast<float>(kRate));
        s.applyPreset(p);
        s.getModMatrix().setSourceValue((int)ObsSynthSrc::XY_X, x);
        s.noteOn(60, 0.8f);
        Take t;
        render(s, 0.3, t);
        return t.db();
    };
    CHECK(level(1.0f) < level(0.0f) - 20.0); // the XY at 1 takes channel 1 away

    ModMatrix m;
    m.setSlot(0, 0, 7, 0.5f);
    m.setSlotEnabled(0, false);
    m.setSlotAmount(0, 0.8f);
    CHECK_FALSE(m.getSlot(0)->enabled);
    CHECK(m.findFreeSlot() != 0); // a muted route keeps its slot

    ObsSynth4D s(static_cast<float>(kRate));
    s.setOscCoarseTune(0, 7.0f);
    s.setOscFineTune(0, 0.25f);
    CHECK(s.getEngine().getAllocator().getVoice(0).getChain(0).getCoarseTune() == doctest::Approx(7.25f));
}

TEST_CASE("Voices: a voice limit plays only that many; the newest notes keep sounding")
{
    FullVoiceAllocator a(96000.0f, VoiceAllocationStrategy::OLDEST);
    a.setVoiceLimit(6);
    for (int k = 0; k < 10; ++k)
        a.noteOn(40 + k, 0.8f);
    float l, r;
    for (int i = 0; i < 2000; ++i)
        a.processStereo(l, r);
    CHECK(a.getActiveVoiceCount() <= 6);
    for (int k = 0; k < 4; ++k) // the first four went
        CHECK(a.findVoiceByNote(40 + k) == nullptr);
    for (int k = 4; k < 10; ++k) // the last six play
        CHECK(a.findVoiceByNote(40 + k) != nullptr);
}

TEST_CASE("Profile: lite (6 voices, modulation and filters every 32 samples, 1x, silent oscillators skipped) sounds like full: finite, the same level, no clicks of its own")
{
    for (int i = 0; i < PRESET_BANK_SIZE; i += 4) // a quarter of the bank: the lite render is for a small machine's time
    {
        CAPTURE(PRESET_BANK[i]->name);
        if (grainsWithout(i))
            continue;
        double rms[2], jumps[2];
        for (int lite = 0; lite < 2; ++lite)
        {
            ObsSynth4D s(static_cast<float>(kRate), lite ? kProfileLite.oversample : ObsSynth4D::OVERSAMPLE);
            loadPanelBanks(s);
            s.selectPreset(i);
            if (lite)
                s.applyProfile(kProfileLite);
            Take t;
            std::vector<float> keep;
            for (int n : {57, 60, 64}) // three notes: within the lite polyphony, so the two play the same notes
                s.noteOn(static_cast<uint8_t>(n), 0.7f);
            render(s, 0.8, t, &keep);
            for (int n : {57, 60, 64})
                s.noteOff(static_cast<uint8_t>(n));
            render(s, 0.4, t, &keep);
            CHECK(t.bad == 0);
            CHECK(t.peak <= 1.0);
            rms[lite] = 10 * std::log10(std::max(t.sum / (2.0 * t.frames), 1e-18));
            // a click is a jump: the largest second difference of either channel
            double jump = 0;
            for (size_t k = 4; k < keep.size(); ++k)
                jump = std::max(jump, std::fabs(static_cast<double>(keep[k]) - 2.0 * static_cast<double>(keep[k - 2]) + static_cast<double>(keep[k - 4])));
            jumps[lite] = jump;
        }
        CHECK(std::fabs(rms[1] - rms[0]) < 1.5); // dB
        CHECK(jumps[1] <= jumps[0] * 1.25 + 1e-3); // no clicks of its own: the steps of the modulation glide
    }
}

// ── SUPER in ObsSynth4D: the ENGINE switch's fifth place, a preset with it, DETUNE through the matrix ─────────────
#include "../src/app/obssynth4d/parts.hpp"

TEST_CASE("SUPER: the ENGINE switch's places map to the engines and back")
{
    CHECK(obs::partTypeAt(4.0f) == PartType::SUPER);
    CHECK(obs::partTypeAt(3.0f) == PartType::GRANULAR);
    for (int i = 0; i < obs::kPartCount; ++i)
        CHECK(obs::placeOf(obs::kPartTypes[i]) == doctest::Approx(static_cast<float>(i)));
    CHECK(obs::placeOf(PartType::SAMPLE) == doctest::Approx(0.0f)); // ObsSynth One's: STD here
}

TEST_CASE("SUPER: an oscillator on it sounds, and the seven at full SIDE about as loud as the centre alone")
{
    auto take = [](float side)
    {
        Preset4D p = quiet();
        p.levels[0] = 0.8f;
        p.waves[0] = WaveType::SAW;
        p.partTypes[0] = PartType::SUPER;
        p.cutoffs[0] = 20000.0f;
        p.grain[0].position = 0.5f; // DETUNE
        p.grain[0].spray = side;    // SIDE
        ObsSynth4D s(static_cast<float>(kRate));
        s.applyPreset(p);
        s.noteOn(48, 0.8f);
        Take t;
        render(s, 0.5, t);
        CHECK(t.bad == 0);
        return t.db();
    };
    const double one = take(0.0f), seven = take(1.0f);
    CHECK(one > -30.0);
    CHECK(std::fabs(seven - one) < 3.0);
}
