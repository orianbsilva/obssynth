#pragma once
// The factory's kits: every pad's engine and knobs, the master, the hit-time routes, and for SAMPLE and GRAIN pads
// the recording (a file in the samples folder, ~/.local/share/obssynth4d/samples, shared with the synth). Built on
// the UI thread; ObsDrums takes them pad by pad.
#include "obs_drums.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace synth
{
    struct DrumRoute
    {
        ObsDrumSrc src;
        DrumKind pad;
        int param; // 0 level, 1 pitch, 2 decay, 3 tone (the model's A), 4 FX, 5 pan
        float amount;
    };

    struct DrumKit
    {
        std::string name;
        std::array<PadSettings, NUM_DRUM_PADS> pads;
        std::array<std::string, NUM_DRUM_PADS> samples; // SAMPLE and GRAIN pads' recordings
        DrumMaster master;
        std::vector<DrumRoute> routes;
    };

    inline PadSettings drumPad(DrumKind k, DrumEngine e, float tune, float decay, float a, float b, float level, float pan = 0.0f,
                               float fx = 0.0f, bool bent = false)
    {
        PadSettings s;
        s.engine = e;
        s.circuit = k;
        s.p = DrumParams{tune, decay, a, b, bent};
        s.level = level;
        s.pan = pan;
        s.fx = fx;
        s.note = drumDefaultNote(k);
        if (k == DrumKind::HAT_CLOSED || k == DrumKind::HAT_OPEN)
            s.choke = 0;
        return s;
    }

    // a kit into the engine: pads, master, routes (the samples apart: loadDrumKitSamples, then setSource)
    inline void applyDrumKit(ObsDrums &d, const DrumKit &k)
    {
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
            d.setPad(i, k.pads[static_cast<size_t>(i)]);
        d.setMaster(k.master);
        d.clearRoutes();
        int slot = 0;
        for (const DrumRoute &r : k.routes)
            if (slot < MAX_MOD_SLOTS)
                d.setRoute(slot++, static_cast<int>(r.src), static_cast<int>(r.pad) * PARAMS_PER_PAD + r.param, r.amount, true);
    }

    // the kit's recordings (off the audio thread: this allocates); one per pad, nullptr if none. find: a name's path
    template <class Find>
    std::array<std::shared_ptr<const GrainSource>, NUM_DRUM_PADS> loadDrumKitSamples(const DrumKit &k, Find find)
    {
        std::array<std::shared_ptr<const GrainSource>, NUM_DRUM_PADS> v{};
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            const std::string &f = k.samples[static_cast<size_t>(i)];
            if (!f.empty())
                v[static_cast<size_t>(i)] = GrainSource::loadWav(find(f));
        }
        return v;
    }

    // the four free pads (USER 1..4) as a kit starts them: an FM bell, a VA zap, a cowbell as a sample, organ grains (the
    // two recordings ship in pcm/: VCSL, CC0)
    inline void drumFreePads(DrumKit &k)
    {
        const DrumEngine e[4] = {DrumEngine::FM, DrumEngine::VA, DrumEngine::SAMPLE, DrumEngine::GRAIN};
        const DrumParams p[4] = {{0.72f, 0.45f, 0.7f, 0.5f, false}, {0.45f, 0.3f, 0.3f, 0.7f, false}, {0.5f, 0.6f, 0.08f, 0.9f, false},
                                 {0.5f, 0.6f, 0.4f, 0.3f, false}};
        const float pan[4] = {-0.35f, 0.35f, -0.15f, 0.15f};
        for (int u = 0; u < 4; ++u)
        {
            PadSettings &s = k.pads[static_cast<size_t>(NUM_DRUM_KINDS + u)];
            s = PadSettings{};
            s.engine = e[u];
            s.circuit = DrumKind::PERC;
            s.p = p[u];
            s.level = 0.5f;
            s.pan = pan[u];
            s.fx = 0.25f;
            s.note = drumDefaultNote(NUM_DRUM_KINDS + u);
        }
        k.samples[static_cast<size_t>(NUM_DRUM_KINDS + 2)] = "vcsl-cowbell.wav";
        k.samples[static_cast<size_t>(NUM_DRUM_KINDS + 3)] = "vcsl-pipe-organ-quiet-C4.wav";
        k.pads[static_cast<size_t>(NUM_DRUM_KINDS + 3)].grainPos = 0.4f;
    }

    inline std::vector<DrumKit> factoryDrumKits()
    {
        using K = DrumKind;
        using E = DrumEngine;
        std::vector<DrumKit> v;
        // the routes every kit has: harder hits ring longer and brighter, the hats wander a little
        auto common = [](DrumKit &k)
        {
            k.routes.push_back({ObsDrumSrc::VELOCITY, K::KICK, 2, 0.4f});
            k.routes.push_back({ObsDrumSrc::VELOCITY, K::SNARE, 3, 0.4f});
            k.routes.push_back({ObsDrumSrc::RANDOM, K::HAT_CLOSED, 1, 0.08f});
            k.routes.push_back({ObsDrumSrc::RANDOM, K::HAT_CLOSED, 0, -0.15f});
            k.routes.push_back({ObsDrumSrc::MOD_WHEEL, K::HAT_OPEN, 2, 0.8f});
            k.routes.push_back({ObsDrumSrc::ACCENT, K::SNARE, 4, 0.3f});
        };
        {
            DrumKit k;
            k.name = "Night Shift"; // the circuits as they come: warm, round
            k.pads = {drumPad(K::KICK, E::CIRCUIT, 0.38f, 0.55f, 0.45f, 0.3f, 0.85f),
                      drumPad(K::SNARE, E::CIRCUIT, 0.5f, 0.5f, 0.45f, 0.6f, 0.7f, -0.05f, 0.15f),
                      drumPad(K::CLAP, E::CIRCUIT, 0.45f, 0.5f, 0.5f, 0.55f, 0.6f, 0.1f, 0.25f),
                      drumPad(K::RIM, E::CIRCUIT, 0.55f, 0.4f, 0.4f, 0.5f, 0.45f, -0.25f, 0.1f),
                      drumPad(K::TOM_LO, E::CIRCUIT, 0.4f, 0.55f, 0.4f, 0.25f, 0.6f, -0.4f, 0.1f),
                      drumPad(K::TOM_MID, E::CIRCUIT, 0.45f, 0.5f, 0.4f, 0.25f, 0.6f, 0.0f, 0.1f),
                      drumPad(K::TOM_HI, E::CIRCUIT, 0.5f, 0.45f, 0.4f, 0.25f, 0.55f, 0.4f, 0.1f),
                      drumPad(K::HAT_CLOSED, E::CIRCUIT, 0.5f, 0.35f, 0.6f, 0.5f, 0.55f, 0.2f, 0.05f),
                      drumPad(K::HAT_OPEN, E::CIRCUIT, 0.5f, 0.45f, 0.6f, 0.5f, 0.5f, 0.25f, 0.15f),
                      drumPad(K::CRASH, E::CIRCUIT, 0.5f, 0.6f, 0.5f, 0.5f, 0.45f, -0.3f, 0.3f),
                      drumPad(K::RIDE, E::CIRCUIT, 0.5f, 0.6f, 0.5f, 0.45f, 0.45f, 0.35f, 0.2f),
                      drumPad(K::PERC, E::CIRCUIT, 0.5f, 0.35f, 0.5f, 0.5f, 0.45f, 0.15f, 0.15f)};
            drumFreePads(k);
            common(k);
            v.push_back(k);
        }
        {
            DrumKit k;
            k.name = "Machine Room"; // bent and driven: the circuits past their limits, FM metal, VA toms
            k.master.drive = 0.55f;
            k.master.tubeMix = 0.6f;
            k.master.glue = 0.55f;
            k.master.delayMix = 0.18f;
            k.master.reverbMix = 0.12f;
            k.pads = {drumPad(K::KICK, E::CIRCUIT, 0.33f, 0.45f, 0.75f, 0.7f, 0.85f, 0, 0, true),
                      drumPad(K::SNARE, E::CIRCUIT, 0.6f, 0.4f, 0.35f, 0.8f, 0.7f, 0, 0.1f, true),
                      drumPad(K::CLAP, E::CIRCUIT, 0.6f, 0.35f, 0.35f, 0.7f, 0.6f, 0.15f, 0.2f, true),
                      drumPad(K::RIM, E::FM, 0.75f, 0.2f, 0.8f, 0.7f, 0.45f, -0.3f, 0.15f),
                      drumPad(K::TOM_LO, E::VA, 0.25f, 0.45f, 0.2f, 0.55f, 0.6f, -0.4f, 0.1f),
                      drumPad(K::TOM_MID, E::VA, 0.35f, 0.4f, 0.2f, 0.55f, 0.6f, 0.0f, 0.1f),
                      drumPad(K::TOM_HI, E::VA, 0.45f, 0.35f, 0.2f, 0.55f, 0.55f, 0.4f, 0.1f),
                      drumPad(K::HAT_CLOSED, E::CIRCUIT, 0.6f, 0.3f, 0.75f, 0.7f, 0.55f, 0.2f, 0.05f, true),
                      drumPad(K::HAT_OPEN, E::CIRCUIT, 0.6f, 0.4f, 0.75f, 0.7f, 0.5f, 0.25f, 0.1f, true),
                      drumPad(K::CRASH, E::FM, 0.62f, 0.75f, 0.83f, 0.65f, 0.4f, -0.3f, 0.25f),
                      drumPad(K::RIDE, E::CIRCUIT, 0.55f, 0.55f, 0.6f, 0.8f, 0.45f, 0.35f, 0.15f, true),
                      drumPad(K::PERC, E::FM, 0.55f, 0.35f, 0.55f, 0.8f, 0.5f, 0.15f, 0.2f, true)};
            drumFreePads(k);
            common(k);
            k.routes.push_back({ObsDrumSrc::VELOCITY, K::KICK, 1, 0.15f});
            v.push_back(k);
        }
        {
            DrumKit k;
            k.name = "Glass Garden"; // FM bells, VA toms, grains of a bowed vibraphone
            k.master.reverbMix = 0.4f;
            k.master.reverbSize = 0.8f;
            k.master.delayMix = 0.3f;
            k.master.drive = 0.1f;
            k.pads = {drumPad(K::KICK, E::CIRCUIT, 0.42f, 0.5f, 0.35f, 0.15f, 0.8f),
                      drumPad(K::SNARE, E::FM, 0.55f, 0.35f, 0.4f, 0.55f, 0.6f, -0.1f, 0.3f),
                      drumPad(K::CLAP, E::CIRCUIT, 0.55f, 0.6f, 0.6f, 0.65f, 0.55f, 0.1f, 0.4f),
                      drumPad(K::RIM, E::FM, 0.82f, 0.25f, 0.62f, 0.5f, 0.45f, -0.3f, 0.3f),
                      drumPad(K::TOM_LO, E::FM, 0.3f, 0.5f, 0.3f, 0.35f, 0.55f, -0.4f, 0.3f),
                      drumPad(K::TOM_MID, E::FM, 0.4f, 0.45f, 0.3f, 0.35f, 0.55f, 0.0f, 0.3f),
                      drumPad(K::TOM_HI, E::FM, 0.5f, 0.4f, 0.3f, 0.35f, 0.5f, 0.4f, 0.3f),
                      drumPad(K::HAT_CLOSED, E::CIRCUIT, 0.55f, 0.3f, 0.7f, 0.4f, 0.5f, 0.2f, 0.1f),
                      drumPad(K::HAT_OPEN, E::CIRCUIT, 0.55f, 0.5f, 0.7f, 0.4f, 0.45f, 0.25f, 0.2f),
                      drumPad(K::CRASH, E::GRAIN, 0.5f, 0.75f, 0.6f, 0.45f, 0.5f, -0.3f, 0.4f),
                      drumPad(K::RIDE, E::GRAIN, 0.62f, 0.55f, 0.35f, 0.25f, 0.45f, 0.35f, 0.3f),
                      drumPad(K::PERC, E::FM, 0.7f, 0.5f, 0.75f, 0.45f, 0.5f, 0.15f, 0.4f)};
            k.samples[static_cast<size_t>(K::CRASH)] = "vcsl-vibraphone-bowed-E5.wav";
            k.samples[static_cast<size_t>(K::RIDE)] = "vcsl-wine-glass-Ds4.wav";
            k.pads[static_cast<size_t>(K::CRASH)].grainPos = 0.25f;
            k.pads[static_cast<size_t>(K::RIDE)].grainPos = 0.15f;
            drumFreePads(k);
            common(k);
            v.push_back(k);
        }
        {
            DrumKit k;
            k.name = "Found Sounds"; // recordings where the circuits were: kalimba, bells, a gong, the sea
            k.master.reverbMix = 0.3f;
            k.pads = {drumPad(K::KICK, E::CIRCUIT, 0.36f, 0.5f, 0.4f, 0.25f, 0.85f),
                      drumPad(K::SNARE, E::CIRCUIT, 0.45f, 0.45f, 0.55f, 0.55f, 0.65f, 0, 0.15f),
                      drumPad(K::CLAP, E::SAMPLE, 0.55f, 0.35f, 0.0f, 0.8f, 0.55f, 0.1f, 0.2f),
                      drumPad(K::RIM, E::SAMPLE, 0.6f, 0.3f, 0.0f, 0.9f, 0.55f, -0.25f, 0.1f),
                      drumPad(K::TOM_LO, E::SAMPLE, 0.35f, 0.5f, 0.0f, 0.8f, 0.6f, -0.4f, 0.15f),
                      drumPad(K::TOM_MID, E::SAMPLE, 0.45f, 0.45f, 0.0f, 0.8f, 0.6f, 0.0f, 0.15f),
                      drumPad(K::TOM_HI, E::SAMPLE, 0.55f, 0.4f, 0.0f, 0.8f, 0.55f, 0.4f, 0.15f),
                      drumPad(K::HAT_CLOSED, E::CIRCUIT, 0.45f, 0.3f, 0.55f, 0.45f, 0.5f, 0.2f, 0.05f),
                      drumPad(K::HAT_OPEN, E::GRAIN, 0.6f, 0.45f, 0.2f, 0.6f, 0.45f, 0.25f, 0.2f),
                      drumPad(K::CRASH, E::SAMPLE, 0.5f, 0.9f, 0.0f, 0.9f, 0.5f, -0.3f, 0.3f),
                      drumPad(K::RIDE, E::GRAIN, 0.5f, 0.6f, 0.3f, 0.35f, 0.45f, 0.35f, 0.25f),
                      drumPad(K::PERC, E::SAMPLE, 0.5f, 0.45f, 0.0f, 0.9f, 0.5f, 0.15f, 0.2f)};
            k.samples[static_cast<size_t>(K::CLAP)] = "vcsl-psaltery-pluck-C4.wav";
            k.samples[static_cast<size_t>(K::RIM)] = "vcsl-kalimba-As2.wav";
            k.samples[static_cast<size_t>(K::TOM_LO)] = "vcsl-tubular-bells-C4.wav";
            k.samples[static_cast<size_t>(K::TOM_MID)] = "vcsl-steinway-C4.wav";
            k.samples[static_cast<size_t>(K::TOM_HI)] = "vcsl-dan-tranh-tremolo-Gs3.wav";
            k.samples[static_cast<size_t>(K::HAT_OPEN)] = "vcsl-ocean-drum.wav";
            k.samples[static_cast<size_t>(K::CRASH)] = "vcsl-gong-scrape.wav";
            k.samples[static_cast<size_t>(K::RIDE)] = "vcsl-ocean-drum.wav";
            k.samples[static_cast<size_t>(K::PERC)] = "vcsl-wine-glass-Ds4.wav";
            k.pads[static_cast<size_t>(K::HAT_OPEN)].grainPos = 0.4f;
            k.pads[static_cast<size_t>(K::RIDE)].grainPos = 0.6f;
            drumFreePads(k);
            common(k);
            v.push_back(k);
        }
        return v;
    }
} // namespace synth
