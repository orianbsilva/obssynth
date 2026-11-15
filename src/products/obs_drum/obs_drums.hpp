#pragma once
// ObsDrums: a drum machine built from the synth's parts. Sixteen pads: twelve instruments (kick, snare, clap, rim,
// three toms, closed and open hat, crash, ride, perc) and four free ones; every pad holds every engine and every
// circuit, built here, so changing a pad's engine or circuit on the audio thread is an index (no allocation):
//   CIRCUIT  the instrument's modelled analog circuit (lib/synth/drums/circuit_*.hpp)
//   FM, VA, SAMPLE, GRAIN  (lib/synth/drums/other_drums.hpp)
// A hit evaluates the modulation matrix for its pad (velocity, random, accent, the wheel, two CCs onto the pad's
// level, pitch, decay, tone, FX send and pan: obs_drum_mod_*.hpp) and plays. Choke groups (the hats). The pads run
// at twice the rate, are panned and summed, glued (BusGlue) and saturated (TubeSaturation), decimated
// (StereoDecimator); the FX send, decimated apart, goes through a stereo delay and a reverb at the native rate;
// the OutputStage ends it (DC, volume, limiter). Only the audio thread calls render(), hit(), set*(): a product
// wrapping it queues (obsdrums app: sound.cpp). FLOAT-ONLY.
#include "obs_drum_mod_dest.hpp"
#include "obs_drum_mod_source.hpp"
#include "../../lib/synth/drums/circuit_clap.hpp"
#include "../../lib/synth/drums/circuit_kick.hpp"
#include "../../lib/synth/drums/circuit_metal.hpp"
#include "../../lib/synth/drums/circuit_snare.hpp"
#include "../../lib/synth/drums/circuit_tom.hpp"
#include "../../lib/synth/drums/other_drums.hpp"
#include "../../lib/synth/core/decimator.hpp"
#include "../../lib/synth/core/output_stage.hpp"
#include "../../lib/synth/modulation/mod_matrix.hpp"
#include "../../lib/effects/bus_glue.hpp"
#include "../../lib/effects/delay.hpp"
#include "../../lib/effects/reverb.hpp"
#include "../../lib/effects/waveshaper.hpp"

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace synth
{
    enum class DrumEngine : uint8_t
    {
        CIRCUIT,
        FM,
        VA,
        SAMPLE,
        GRAIN,
        COUNT
    };
    enum class DrumKind : uint8_t
    {
        KICK,
        SNARE,
        CLAP,
        RIM,
        TOM_LO,
        TOM_MID,
        TOM_HI,
        HAT_CLOSED,
        HAT_OPEN,
        CRASH,
        RIDE,
        PERC,
        COUNT
    };
    constexpr int NUM_DRUM_KINDS = static_cast<int>(DrumKind::COUNT); // the circuits: one per instrument
    constexpr int NUM_DRUM_PADS = 16;                                  // the twelve instruments and four free pads
    constexpr int NUM_DRUM_ENGINES = static_cast<int>(DrumEngine::COUNT);

    inline const char *drumEngineName(DrumEngine e)
    {
        static const char *n[] = {"CIRCUIT", "FM", "VA", "SAMPLE", "GRAIN"};
        return n[static_cast<int>(e) % NUM_DRUM_ENGINES];
    }
    inline const char *drumKindName(DrumKind k)
    {
        static const char *n[] = {"KICK", "SNARE", "CLAP", "RIM", "LOW TOM", "MID TOM", "HI TOM", "CL HAT", "OP HAT", "CRASH", "RIDE", "PERC"};
        return n[static_cast<int>(k) % NUM_DRUM_KINDS];
    }
    // a pad's name: its instrument, or USER 1..4 for the free ones
    inline const char *drumPadName(int pad)
    {
        static const char *user[] = {"USER 1", "USER 2", "USER 3", "USER 4"};
        return pad < NUM_DRUM_KINDS ? drumKindName(static_cast<DrumKind>(pad)) : user[(pad - NUM_DRUM_KINDS) & 3];
    }
    // General MIDI's drum notes for each pad (gridseq plays these); the free pads: tambourine, bongos, claves
    inline int drumDefaultNote(int pad)
    {
        static const int n[] = {36, 38, 39, 37, 41, 45, 50, 42, 46, 49, 51, 56, 54, 60, 63, 75};
        return n[pad & 15];
    }
    inline int drumDefaultNote(DrumKind k) { return drumDefaultNote(static_cast<int>(k)); }

    struct PadSettings
    {
        DrumEngine engine = DrumEngine::CIRCUIT;
        DrumKind circuit = DrumKind::KICK; // which circuit plays when the engine is CIRCUIT (any pad, any circuit)
        DrumParams p;
        float level = 0.75f; // 0..1
        float pan = 0.0f;    // -1..1
        float fx = 0.0f;     // the FX send, 0..1
        float grainPos = 0.3f;
        int note = 36;
        int choke = -1; // a group: hitting one pad of it silences the others
        bool mute = false, solo = false;
    };

    struct DrumMaster
    {
        float delayTime = 0.375f, delayFeedback = 0.35f, delayMix = 0.25f;
        float reverbSize = 0.6f, reverbDamp = 0.4f, reverbMix = 0.25f;
        float drive = 0.2f, glue = 0.3f, tubeMix = 0.3f;
        float volume = 0.7f;
    };

    class ObsDrums
    {
    public:
        static constexpr int OVERSAMPLE = 2;

        explicit ObsDrums(float sampleRate)
            : sr_(sampleRate), osr_(sampleRate * OVERSAMPLE), glue_(sampleRate * OVERSAMPLE), tube_(sampleRate * OVERSAMPLE),
              delay_(sampleRate), revL_(sampleRate), revR_(sampleRate), output_(sampleRate, 0.7f)
        {
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
            {
                Pad &pad = pads_[static_cast<size_t>(i)];
                for (int c = 0; c < NUM_DRUM_KINDS; ++c) // every circuit, so a pad can change its own on the audio thread
                    pad.circuits[static_cast<size_t>(c)] = circuitFor(static_cast<DrumKind>(c));
                pad.fm = std::make_unique<FmDrum>(osr_);
                pad.va = std::make_unique<VaDrum>(osr_);
                pad.sample = std::make_unique<SampleDrum>(osr_);
                pad.grain = std::make_unique<GrainDrum>(osr_);
                PadSettings s;
                s.note = drumDefaultNote(i);
                if (i < NUM_DRUM_KINDS)
                    s.circuit = static_cast<DrumKind>(i);
                else // the free pads, by default the other engines
                {
                    static const DrumEngine e[4] = {DrumEngine::FM, DrumEngine::VA, DrumEngine::SAMPLE, DrumEngine::GRAIN};
                    s.engine = e[i - NUM_DRUM_KINDS];
                    s.circuit = DrumKind::PERC;
                }
                if (i == static_cast<int>(DrumKind::HAT_CLOSED) || i == static_cast<int>(DrumKind::HAT_OPEN))
                    s.choke = 0;
                setPad(i, s);
            }
            decimator_.init(sampleRate, OVERSAMPLE);
            fxDecimator_.init(sampleRate, OVERSAMPLE);
            tube_.setModel(TubeModel::AX7);
            delay_.setMix(1.0f); // the send's return: wet only
            revL_.setDryLevel(0.0f);
            revR_.setDryLevel(0.0f);
            revL_.setWetLevel(1.0f);
            revR_.setWetLevel(1.0f);
            matrix_.setBipolarCheck([](int) { return false; }); // the drum sources are unipolar
            os_.reserve(8192 * 2 * OVERSAMPLE);
            fxOs_.reserve(8192 * 2 * OVERSAMPLE);
            fx_.reserve(8192 * 2);
            setMaster(DrumMaster{});
        }

        // ── settings (the audio thread's) ─────────────────────────────────────────────────────────
        void setPad(int i, const PadSettings &s)
        {
            if (i < 0 || i >= NUM_DRUM_PADS)
                return;
            Pad &pad = pads_[static_cast<size_t>(i)];
            pad.s = s;
            pad.current()->set(s.p);
            pad.grain->setPosition(s.grainPos);
            anySolo_ = false;
            for (const auto &p : pads_)
                anySolo_ = anySolo_ || p.s.solo;
        }
        const PadSettings &pad(int i) const { return pads_[static_cast<size_t>(std::clamp(i, 0, NUM_DRUM_PADS - 1))].s; }

        void setMaster(const DrumMaster &m)
        {
            master_ = m;
            delay_.setDelayTimeL(m.delayTime);
            delay_.setDelayTimeR(m.delayTime * 1.5f);
            delay_.setFeedback(m.delayFeedback);
            delay_.setPingPong(true);
            revL_.setRoomSize(m.reverbSize);
            revR_.setRoomSize(std::min(1.0f, m.reverbSize * 1.03f));
            revL_.setDamping(m.reverbDamp);
            revR_.setDamping(m.reverbDamp);
            glue_.setAmount(m.glue);
            tube_.setDrive(1.0f + 5.0f * m.drive);
            tube_.setMix(m.tubeMix);
            tubeMakeup_ = std::max(1.0f, (1.0f + 5.0f * m.drive) / 1.5f) * m.tubeMix + (1.0f - m.tubeMix);
        }
        const DrumMaster &master() const { return master_; }

        // a recording for a pad's SAMPLE and GRAIN engines (a pointer copy; the caller keeps the old one alive)
        void setSource(int i, const std::shared_ptr<const GrainSource> &src)
        {
            if (i < 0 || i >= NUM_DRUM_PADS)
                return;
            pads_[static_cast<size_t>(i)].sample->setSource(src);
            pads_[static_cast<size_t>(i)].grain->setSource(src);
        }

        void setRoute(int slot, int source, int dest, float amount, bool enabled)
        {
            matrix_.setSlot(slot, source, dest, amount);
            matrix_.setSlotEnabled(slot, enabled);
        }
        void clearRoutes() { matrix_.clear(); }
        void setModWheel(float v) { matrix_.setSourceValue(static_cast<int>(ObsDrumSrc::MOD_WHEEL), std::clamp(v, 0.0f, 1.0f)); }
        void setCC(int which, float v) { matrix_.setSourceValue(static_cast<int>(ObsDrumSrc::CC_A) + std::clamp(which, 0, 1), std::clamp(v, 0.0f, 1.0f)); }

        // ── playing ───────────────────────────────────────────────────────────────────────────────
        void noteOn(int note, float velocity, float accent = 0.0f)
        {
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
                if (pads_[static_cast<size_t>(i)].s.note == note)
                    hit(i, velocity, accent);
        }

        void hit(int i, float velocity, float accent = 0.0f)
        {
            if (i < 0 || i >= NUM_DRUM_PADS)
                return;
            Pad &pad = pads_[static_cast<size_t>(i)];
            // the hit's modulation: this pad's six destinations, from this hit's sources
            rnd_ ^= rnd_ << 13;
            rnd_ ^= rnd_ >> 17;
            rnd_ ^= rnd_ << 5;
            matrix_.setSourceValue(static_cast<int>(ObsDrumSrc::VELOCITY), velocity);
            matrix_.setSourceValue(static_cast<int>(ObsDrumSrc::RANDOM), static_cast<float>(rnd_ >> 8) / 16777216.0f);
            matrix_.setSourceValue(static_cast<int>(ObsDrumSrc::ACCENT), accent);
            float dst[NUM_DRUM_PADS * PARAMS_PER_PAD];
            matrix_.evaluate(matrix_.sourceValues(), dst, NUM_DRUM_PADS * PARAMS_PER_PAD);
            const float *d = dst + i * PARAMS_PER_PAD; // LEVEL, PITCH, DECAY, CUTOFF (the model's A), FX, PAN
            DrumParams p = pad.s.p;
            p.tune = std::clamp(p.tune + d[1] * 0.25f, 0.0f, 1.0f);
            p.decay = std::clamp(p.decay + d[2] * 0.3f, 0.0f, 1.0f);
            p.a = std::clamp(p.a + d[3] * 0.5f, 0.0f, 1.0f);
            DrumModel *m = pad.current();
            m->set(p);
            m->trigger(DrumHit{velocity, accent});
            pad.hitLevel = std::clamp(pad.s.level * (1.0f + d[0]), 0.0f, 1.5f);
            pad.hitFx = std::clamp(pad.s.fx + d[4], 0.0f, 1.0f);
            const float pan = std::clamp(pad.s.pan + d[5], -1.0f, 1.0f);
            pad.gainL = std::cos((pan + 1.0f) * 0.25f * 3.14159265f) * 1.41421356f; // constant power, 1 at the centre
            pad.gainR = std::sin((pan + 1.0f) * 0.25f * 3.14159265f) * 1.41421356f;
            pad.choke = 1.0f;
            pad.chokeStep = 0.0f;
            pad.flash = 1.0f;
            if (pad.s.choke >= 0) // the others of its group fade in 5 ms
                for (int j = 0; j < NUM_DRUM_PADS; ++j)
                    if (j != i && pads_[static_cast<size_t>(j)].s.choke == pad.s.choke)
                        pads_[static_cast<size_t>(j)].chokeStep = 1.0f / (0.005f * osr_);
        }

        // stereo, interleaved, at the native rate
        void render(float *out, int frames)
        {
            const size_t osn = static_cast<size_t>(frames * OVERSAMPLE * 2);
            if (osn > os_.capacity())
            {
                os_.reserve(osn);
                fxOs_.reserve(osn);
                fx_.reserve(static_cast<size_t>(frames * 2));
            }
            os_.assign(osn, 0.0f);
            fxOs_.assign(osn, 0.0f);
            for (auto &pad : pads_)
            {
                pad.peak = 0.0f;
                DrumModel *m = pad.current();
                if (!m->active())
                    continue;
                const bool heard = !pad.s.mute && (!anySolo_ || pad.s.solo);
                for (size_t k = 0; k < osn; k += 2)
                {
                    float y = m->process() * pad.choke;
                    if (pad.chokeStep > 0.0f)
                        pad.choke = std::max(0.0f, pad.choke - pad.chokeStep);
                    if (!heard)
                        continue;
                    y *= pad.hitLevel;
                    pad.peak = std::max(pad.peak, std::fabs(y));
                    os_[k] += y * pad.gainL;
                    os_[k + 1] += y * pad.gainR;
                    fxOs_[k] += y * pad.gainL * pad.hitFx;
                    fxOs_[k + 1] += y * pad.gainR * pad.hitFx;
                }
            }
            glue_.process(os_.data(), frames * OVERSAMPLE);
            if (master_.tubeMix > 0.001f)
            {
                tube_.processStereo(os_.data(), frames * OVERSAMPLE);
                for (size_t k = 0; k < osn; ++k)
                    os_[k] *= tubeMakeup_;
            }
            decimator_.process(os_.data(), out, frames);
            fx_.assign(static_cast<size_t>(frames * 2), 0.0f);
            fxDecimator_.process(fxOs_.data(), fx_.data(), frames);
            // the send: a delay and a reverb, side by side, back into the mix
            std::vector<float> &f = fx_;
            for (int i = 0; i < frames; ++i)
            {
                const float l = f[static_cast<size_t>(i) * 2], r = f[static_cast<size_t>(i) * 2 + 1];
                const float rl = revL_.process(l), rr = revR_.process(r);
                f[static_cast<size_t>(i) * 2] = l; // the delay's input stays here, processed below
                f[static_cast<size_t>(i) * 2 + 1] = r;
                out[i * 2] += rl * master_.reverbMix;
                out[i * 2 + 1] += rr * master_.reverbMix;
            }
            delay_.processStereo(f.data(), frames);
            for (int i = 0; i < frames * 2; ++i)
                out[i] += f[static_cast<size_t>(i)] * master_.delayMix;
            output_.process(out, frames, master_.volume, 0.0f);
            for (auto &pad : pads_)
                pad.flash *= 0.85f;
        }

        // for a display: a pad's loudest sample in the last block, and how lit its pad is (1 at a hit, fading)
        float padPeak(int i) const { return pads_[static_cast<size_t>(std::clamp(i, 0, NUM_DRUM_PADS - 1))].peak; }
        float padFlash(int i) const { return pads_[static_cast<size_t>(std::clamp(i, 0, NUM_DRUM_PADS - 1))].flash; }
        const char *padNameA(int i) const { return pads_[static_cast<size_t>(std::clamp(i, 0, NUM_DRUM_PADS - 1))].current()->nameA(); }
        const char *padNameB(int i) const { return pads_[static_cast<size_t>(std::clamp(i, 0, NUM_DRUM_PADS - 1))].current()->nameB(); }
        float takeLimiterLowest() { return output_.takeLimiterLowest(); }

    private:
        struct Pad
        {
            PadSettings s;
            std::array<std::unique_ptr<DrumModel>, NUM_DRUM_KINDS> circuits;
            std::unique_ptr<DrumModel> fm, va;
            std::unique_ptr<SampleDrum> sample;
            std::unique_ptr<GrainDrum> grain;
            DrumModel *current() const
            {
                switch (s.engine)
                {
                case DrumEngine::FM: return fm.get();
                case DrumEngine::VA: return va.get();
                case DrumEngine::SAMPLE: return sample.get();
                case DrumEngine::GRAIN: return grain.get();
                default: return circuits[static_cast<size_t>(s.circuit) % NUM_DRUM_KINDS].get();
                }
            }
            float hitLevel = 0.75f, hitFx = 0.0f, gainL = 1.0f, gainR = 1.0f;
            float choke = 1.0f, chokeStep = 0.0f, peak = 0.0f, flash = 0.0f;
        };

        std::unique_ptr<DrumModel> circuitFor(DrumKind k) const
        {
            switch (k)
            {
            case DrumKind::KICK: return std::make_unique<CircuitKick>(osr_);
            case DrumKind::SNARE: return std::make_unique<CircuitSnare>(osr_);
            case DrumKind::CLAP: return std::make_unique<CircuitClap>(osr_);
            case DrumKind::RIM: return std::make_unique<CircuitRim>(osr_);
            case DrumKind::TOM_LO: return std::make_unique<CircuitTom>(osr_, 55.0f, 140.0f);
            case DrumKind::TOM_MID: return std::make_unique<CircuitTom>(osr_, 80.0f, 200.0f);
            case DrumKind::TOM_HI: return std::make_unique<CircuitTom>(osr_, 110.0f, 300.0f);
            case DrumKind::HAT_CLOSED: return std::make_unique<CircuitMetal>(osr_, CircuitMetal::Kind::ClosedHat);
            case DrumKind::HAT_OPEN: return std::make_unique<CircuitMetal>(osr_, CircuitMetal::Kind::OpenHat);
            case DrumKind::CRASH: return std::make_unique<CircuitMetal>(osr_, CircuitMetal::Kind::Crash);
            case DrumKind::RIDE: return std::make_unique<CircuitMetal>(osr_, CircuitMetal::Kind::Ride);
            default: return std::make_unique<CircuitMetal>(osr_, CircuitMetal::Kind::Cowbell);
            }
        }

        [[maybe_unused]] float sr_;
        float osr_;
        std::array<Pad, NUM_DRUM_PADS> pads_;
        bool anySolo_ = false;
        DrumMaster master_;
        ModMatrix matrix_;
        uint32_t rnd_ = 0x51ED270Bu;
        std::vector<float> os_, fxOs_, fx_;
        BusGlue glue_;
        TubeSaturation tube_;
        float tubeMakeup_ = 1.0f;
        StereoDecimator decimator_, fxDecimator_;
        StereoDelay delay_;
        SchroederReverb revL_, revR_;
        OutputStage output_;
    };
} // namespace synth
