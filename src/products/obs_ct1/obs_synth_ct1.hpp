#pragma once
// ObsSynthCT1 (Cross Table One): five operators, each STD (a classic shape) or WT (a wavetable bank and its position,
// which a slow or audio-rate scan may move), cross-modulating by phase through a 5 x 5 matrix: row modulates column,
// in radians of index; the diagonal is an operator's feedback (the mean of its last two outputs); OUT says who
// sounds and how loud. Every operator's output carries its level and its envelope, so a modulator's envelope is
// its index. The matrix's index is scaled by INDEX, the velocity (VEL > IDX) and the wheel (WHEEL > IDX).
// After the voices (rendered at 2x, decimated): FX 1, a filter and a drive; FX 2, a delay and a reverb; the output
// stage (DC, limiter at -1 dBFS). FLOAT-ONLY on the audio path.
//
// The parameters are one array of physical values (Params), each with a name, a range and a kind (ParamInfo), so
// a panel, the presets (by name) and a controller's map all speak of them alike. The audio thread is the engine's
// only writer: setParam() there, from a queue (the app's sound.cpp); the tables are built before and only read.
#include "ct1_tables.hpp"
#include "../../lib/synth/core/decimator.hpp"
#include "../../lib/synth/core/output_stage.hpp"
#include "../../lib/synth/core/svf_filter.hpp"
#include "../../lib/effects/delay.hpp"
#include "../../lib/effects/reverb.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace synth::ct1
{
    constexpr int OPS = 5;
    constexpr int OVERSAMPLE = 2;
    constexpr int VOICES = 8;

    // ── the parameters ──────────────────────────────────────────────────────────────────────────
    enum OpParam
    {
        OP_TYPE,       // 0 STD, 1 WT
        OP_SHAPE,      // STD: sine, saw, square, triangle
        OP_BANK,       // WT: the bank (Engine's list)
        OP_POS,        // WT: 0..1 across the bank
        OP_RATIO,      // to the note (a list of musical ratios)
        OP_FINE,       // cents
        OP_FIXED,      // 1: HZ, not the note
        OP_HZ,
        OP_LEVEL,
        OP_PAN,
        OP_A,
        OP_D,
        OP_S,
        OP_R,
        OP_SCAN_RATE,  // the position's own sine: Hz (slow to audio rate)
        OP_SCAN_DEPTH, // how far it moves the position
        OP_PARAMS
    };
    constexpr int opParam(int op, int p) { return op * OP_PARAMS + p; }
    constexpr int P_FM0 = OPS * OP_PARAMS;     // fm(row, col) = P_FM0 + row * OPS + col
    constexpr int P_OUT0 = P_FM0 + OPS * OPS;  // out(op)
    constexpr int fmParam(int row, int col) { return P_FM0 + row * OPS + col; }
    constexpr int outParam(int op) { return P_OUT0 + op; }
    enum GlobalParam
    {
        G_INDEX = P_OUT0 + OPS,
        G_VEL_INDEX,
        G_WHEEL_INDEX,
        G_CUTOFF,
        G_RESO,
        G_DRIVE,
        G_DLY_TIME,
        G_DLY_FB,
        G_DLY_MIX,
        G_REV_SIZE,
        G_REV_MIX,
        G_VOLUME,
        NUM_PARAMS
    };
    using Params = std::array<float, NUM_PARAMS>;

    enum class ParamKind { Linear, Log, Ratio, Toggle, Choice };
    struct ParamInfo
    {
        std::string name; // "OP1_RATIO", "FM_2_1", "OUT_3", "INDEX"
        ParamKind kind;
        float lo, hi, def;
    };

    // the ratios OP_RATIO steps through
    inline const std::vector<float> &ratios()
    {
        static const std::vector<float> r{0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        return r;
    }

    inline const std::vector<ParamInfo> &paramInfo()
    {
        static const std::vector<ParamInfo> v = []
        {
            std::vector<ParamInfo> p;
            for (int o = 0; o < OPS; ++o)
            {
                const std::string n = "OP" + std::to_string(o + 1) + "_";
                p.push_back({n + "TYPE", ParamKind::Toggle, 0, 1, 0});
                p.push_back({n + "SHAPE", ParamKind::Choice, 0, 3, 0});
                p.push_back({n + "BANK", ParamKind::Choice, 0, 15, 0});
                p.push_back({n + "POS", ParamKind::Linear, 0, 1, 0});
                p.push_back({n + "RATIO", ParamKind::Ratio, 0.25f, 16, 1});
                p.push_back({n + "FINE", ParamKind::Linear, -100, 100, 0});
                p.push_back({n + "FIXED", ParamKind::Toggle, 0, 1, 0});
                p.push_back({n + "HZ", ParamKind::Log, 20, 8000, 440});
                p.push_back({n + "LEVEL", ParamKind::Linear, 0, 1, o == 0 ? 1.0f : 0.8f});
                p.push_back({n + "PAN", ParamKind::Linear, -1, 1, 0});
                p.push_back({n + "A", ParamKind::Log, 0.0005f, 10, 0.005f});
                p.push_back({n + "D", ParamKind::Log, 0.001f, 20, 0.6f});
                p.push_back({n + "S", ParamKind::Linear, 0, 1, 0.7f});
                p.push_back({n + "R", ParamKind::Log, 0.001f, 20, 0.4f});
                p.push_back({n + "SCAN_RATE", ParamKind::Log, 0.02f, 200, 0.2f});
                p.push_back({n + "SCAN_DEPTH", ParamKind::Linear, 0, 0.5f, 0});
            }
            for (int r = 0; r < OPS; ++r)
                for (int c = 0; c < OPS; ++c)
                    p.push_back({"FM_" + std::to_string(r + 1) + "_" + std::to_string(c + 1), ParamKind::Linear, 0, 4, 0});
            for (int o = 0; o < OPS; ++o)
                p.push_back({"OUT_" + std::to_string(o + 1), ParamKind::Linear, 0, 2, o == 0 ? 1.0f : 0.0f});
            p.push_back({"INDEX", ParamKind::Linear, 0, 2, 1});
            p.push_back({"VEL_INDEX", ParamKind::Linear, 0, 1, 0.3f});
            p.push_back({"WHEEL_INDEX", ParamKind::Linear, 0, 1, 0.5f});
            p.push_back({"CUTOFF", ParamKind::Log, 40, 20000, 18000});
            p.push_back({"RESO", ParamKind::Linear, 0, 1, 0.1f});
            p.push_back({"DRIVE", ParamKind::Linear, 0, 1, 0});
            p.push_back({"DLY_TIME", ParamKind::Log, 0.02f, 1.5f, 0.33f});
            p.push_back({"DLY_FB", ParamKind::Linear, 0, 0.95f, 0.35f});
            p.push_back({"DLY_MIX", ParamKind::Linear, 0, 1, 0});
            p.push_back({"REV_SIZE", ParamKind::Linear, 0, 1, 0.7f});
            p.push_back({"REV_MIX", ParamKind::Linear, 0, 1, 0.15f});
            p.push_back({"VOLUME", ParamKind::Linear, 0, 1, 0.7f});
            return p;
        }();
        return v;
    }

    inline int paramId(const std::string &name)
    {
        const auto &v = paramInfo();
        for (size_t i = 0; i < v.size(); ++i)
            if (v[i].name == name)
                return static_cast<int>(i);
        return -1;
    }

    // a value to 0..1 (a knob's turn) and back
    inline float toNorm(int id, float v)
    {
        const ParamInfo &p = paramInfo()[static_cast<size_t>(id)];
        switch (p.kind)
        {
        case ParamKind::Log: return std::clamp(std::log(v / p.lo) / std::log(p.hi / p.lo), 0.0f, 1.0f);
        case ParamKind::Ratio:
        {
            const auto &r = ratios();
            size_t best = 0;
            for (size_t i = 0; i < r.size(); ++i)
                if (std::fabs(r[i] - v) < std::fabs(r[best] - v))
                    best = i;
            return static_cast<float>(best) / static_cast<float>(r.size() - 1);
        }
        default: return std::clamp((v - p.lo) / (p.hi - p.lo), 0.0f, 1.0f);
        }
    }
    inline float fromNorm(int id, float n)
    {
        const ParamInfo &p = paramInfo()[static_cast<size_t>(id)];
        n = std::clamp(n, 0.0f, 1.0f);
        switch (p.kind)
        {
        case ParamKind::Log: return p.lo * std::pow(p.hi / p.lo, n);
        case ParamKind::Ratio:
        {
            const auto &r = ratios();
            return r[static_cast<size_t>(std::lround(n * static_cast<float>(r.size() - 1)))];
        }
        case ParamKind::Toggle: return n >= 0.5f ? 1.0f : 0.0f;
        case ParamKind::Choice: return std::round(p.lo + (p.hi - p.lo) * n);
        default: return p.lo + (p.hi - p.lo) * n;
        }
    }

    inline Params defaultParams()
    {
        Params p{};
        const auto &v = paramInfo();
        for (size_t i = 0; i < v.size(); ++i)
            p[i] = v[i].def;
        return p;
    }

    // ── the matrix's starting points ────────────────────────────────────────────────────────────
    struct Algorithm
    {
        const char *name;
        std::vector<std::pair<int, int>> links; // (row modulates column); (k, k): feedback
        std::vector<int> outs;
    };
    inline const std::vector<Algorithm> &algorithms()
    {
        static const std::vector<Algorithm> a{
            {"2 > 1", {{1, 0}}, {0}},
            {"3 > 2 > 1", {{2, 1}, {1, 0}}, {0}},
            {"5 > 4 > 3 > 2 > 1", {{4, 3}, {3, 2}, {2, 1}, {1, 0}}, {0}},
            {"TWO PAIRS", {{1, 0}, {3, 2}}, {0, 2}},
            {"TWO PAIRS + ONE", {{1, 0}, {3, 2}, {4, 0}, {4, 2}}, {0, 2}},
            {"ONE TO FOUR", {{4, 0}, {4, 1}, {4, 2}, {4, 3}}, {0, 1, 2, 3}},
            {"CROSS 1 <> 2", {{0, 1}, {1, 0}, {0, 0}, {1, 1}}, {0, 1}},
            {"RING 1 > 2 > 3 > 1", {{0, 1}, {1, 2}, {2, 0}}, {0, 1, 2}},
            {"ORGAN", {{0, 1}, {1, 2}, {2, 3}, {3, 4}}, {0, 1, 2, 3, 4}},
            {"FIVE ALONE", {}, {0, 1, 2, 3, 4}},
        };
        return a;
    }
    // an algorithm onto a patch's matrix and outs (amounts: 1 rad, feedback 0.3; the levels kept)
    inline void applyAlgorithm(Params &p, int i)
    {
        const Algorithm &a = algorithms()[static_cast<size_t>(std::clamp(i, 0, static_cast<int>(algorithms().size()) - 1))];
        for (int r = 0; r < OPS; ++r)
            for (int c = 0; c < OPS; ++c)
                p[static_cast<size_t>(fmParam(r, c))] = 0;
        for (const auto &[r, c] : a.links)
            p[static_cast<size_t>(fmParam(r, c))] = r == c ? 0.3f : 1.0f;
        for (int o = 0; o < OPS; ++o)
            p[static_cast<size_t>(outParam(o))] = std::find(a.outs.begin(), a.outs.end(), o) != a.outs.end() ? 1.0f : 0.0f;
    }

    // ── the engine ──────────────────────────────────────────────────────────────────────────────
    class Engine
    {
    public:
        // banks: the wavetables OP_BANK chooses among (built before, never changed)
        Engine(float sampleRate, std::vector<std::shared_ptr<const Table>> banks)
            : osr_(sampleRate * OVERSAMPLE), banks_(std::move(banks)), filtL_(sampleRate * OVERSAMPLE), filtR_(sampleRate * OVERSAMPLE),
              delay_(sampleRate), revL_(sampleRate), revR_(sampleRate), output_(sampleRate, 0.7f)
        {
            for (int s = 0; s < 4; ++s)
                shapes_[static_cast<size_t>(s)] = Table::fromShape(static_cast<Table::Shape>(s));
            decimator_.init(sampleRate, OVERSAMPLE);
            delay_.setMix(1.0f);
            delay_.setPingPong(true);
            for (auto *r : {&revL_, &revR_})
            {
                r->setDryLevel(0.0f);
                r->setWetLevel(1.0f);
            }
            os_.reserve(static_cast<size_t>(4096 * OVERSAMPLE * 2));
            fx_.reserve(4096 * 2);
            setAll(defaultParams());
        }

        void setAll(const Params &p)
        {
            for (int i = 0; i < NUM_PARAMS; ++i)
                setParam(i, p[static_cast<size_t>(i)]);
        }

        void setParam(int id, float v)
        {
            if (id < 0 || id >= NUM_PARAMS)
                return;
            p_[static_cast<size_t>(id)] = v;
            if (id < P_FM0)
            {
                const int o = id / OP_PARAMS, k = id % OP_PARAMS;
                if (k == OP_TYPE || k == OP_SHAPE || k == OP_BANK)
                    table_[static_cast<size_t>(o)] = tableOf(o);
                return;
            }
            switch (id)
            {
            case G_CUTOFF:
            case G_RESO:
                filtL_.setCutoff(std::min(p_[G_CUTOFF], 0.45f * osr_));
                filtR_.setCutoff(std::min(p_[G_CUTOFF], 0.45f * osr_));
                filtL_.setResonance(p_[G_RESO]);
                filtR_.setResonance(p_[G_RESO]);
                break;
            case G_DLY_TIME:
                delay_.setDelayTimeL(v);
                delay_.setDelayTimeR(v * 1.5f);
                break;
            case G_DLY_FB: delay_.setFeedback(v); break;
            case G_REV_SIZE:
                revL_.setRoomSize(v);
                revR_.setRoomSize(std::min(1.0f, v * 1.03f));
                break;
            default: break;
            }
        }
        float param(int id) const { return p_[static_cast<size_t>(std::clamp(id, 0, NUM_PARAMS - 1))]; }

        void noteOn(int note, float velocity)
        {
            Voice *v = nullptr;
            for (auto &x : voices_) // the same note again, else a free one, else the oldest
                if (x.active && x.note == note)
                    v = &x;
            for (auto &x : voices_)
                if (!v && !x.active)
                    v = &x;
            if (!v)
            {
                v = &voices_[0];
                for (auto &x : voices_)
                    if (x.started < v->started)
                        v = &x;
            }
            const bool again = v->active; // a voice taken while it sounds keeps its phases: no click
            v->active = true;
            v->released = false;
            v->note = note;
            v->vel = std::clamp(velocity, 0.0f, 1.0f);
            v->started = ++clock_;
            for (int o = 0; o < OPS; ++o)
            {
                v->stage[static_cast<size_t>(o)] = 1;
                if (!again)
                {
                    v->phase[static_cast<size_t>(o)] = 0;
                    v->scan[static_cast<size_t>(o)] = 0;
                    v->env[static_cast<size_t>(o)] = 0;
                    v->y[static_cast<size_t>(o)] = v->y1[static_cast<size_t>(o)] = 0;
                }
            }
        }

        void noteOff(int note)
        {
            for (auto &v : voices_)
                if (v.active && v.note == note && !v.released)
                {
                    v.released = true;
                    v.stage.fill(4);
                }
        }

        void allNotesOff()
        {
            for (auto &v : voices_)
                if (v.active)
                {
                    v.released = true;
                    v.stage.fill(4);
                }
        }

        void setModWheel(float w) { wheel_ = std::clamp(w, 0.0f, 1.0f); }
        void setPitchBend(float semitones) { bend_ = std::exp2(semitones / 12.0f); }

        // interleaved stereo at the native rate, at most 4096 frames
        void render(float *out, int frames)
        {
            frames = std::min(frames, 4096);
            os_.assign(static_cast<size_t>(frames * OVERSAMPLE * 2), 0.0f);
            for (auto &v : voices_)
                if (v.active)
                    voice(v, os_.data(), frames * OVERSAMPLE);
            const float drive = 1 + 8 * p_[G_DRIVE], makeup = 0.5f / std::tanh(drive * 0.5f);
            for (int i = 0; i < frames * OVERSAMPLE; ++i)
            {
                float &l = os_[static_cast<size_t>(i) * 2], &r = os_[static_cast<size_t>(i) * 2 + 1];
                l = filtL_.process(l);
                r = filtR_.process(r);
                if (p_[G_DRIVE] > 0.001f)
                {
                    l = std::tanh(l * drive) * makeup;
                    r = std::tanh(r * drive) * makeup;
                }
            }
            decimator_.process(os_.data(), out, frames);
            fx_.assign(out, out + frames * 2);
            const float rev = p_[G_REV_MIX], dly = p_[G_DLY_MIX];
            for (int i = 0; i < frames; ++i)
            {
                const float l = fx_[static_cast<size_t>(i) * 2], r = fx_[static_cast<size_t>(i) * 2 + 1];
                out[i * 2] += revL_.process(l) * rev;
                out[i * 2 + 1] += revR_.process(r) * rev;
            }
            delay_.processStereo(fx_.data(), frames);
            for (int i = 0; i < frames * 2; ++i)
                out[i] += fx_[static_cast<size_t>(i)] * dly;
            output_.process(out, frames, p_[G_VOLUME], 0.0f);
        }

        // for the panel: the newest voice's position of op o (its scan included), -1 when none sounds; voices sounding
        float livePos(int o) const
        {
            const Voice *n = nullptr;
            for (const auto &v : voices_)
                if (v.active && (!n || v.started > n->started))
                    n = &v;
            return n ? n->pos[static_cast<size_t>(std::clamp(o, 0, OPS - 1))] : -1.0f;
        }
        int sounding() const
        {
            int n = 0;
            for (const auto &v : voices_)
                n += v.active;
            return n;
        }
        const std::vector<std::shared_ptr<const Table>> &banks() const { return banks_; }
        std::shared_ptr<const Table> shape(int s) const { return shapes_[static_cast<size_t>(std::clamp(s, 0, 3))]; }

    private:
        struct Voice
        {
            bool active = false, released = false;
            int note = 0;
            float vel = 1;
            uint64_t started = 0;
            std::array<float, OPS> phase{}, scan{}, env{}, y{}, y1{}, pos{};
            std::array<int, OPS> stage{}; // 0 off, 1 attack, 2 decay, 3 sustain, 4 release
        };

        float osr_;
        std::vector<std::shared_ptr<const Table>> banks_;
        std::array<std::shared_ptr<const Table>, 4> shapes_;
        std::array<std::shared_ptr<const Table>, OPS> table_;
        Params p_{};
        std::array<Voice, VOICES> voices_{};
        uint64_t clock_ = 0;
        float wheel_ = 0, bend_ = 1;
        StereoDecimator decimator_;
        SvfFilter filtL_, filtR_;
        StereoDelay delay_;
        SchroederReverb revL_, revR_;
        OutputStage output_;
        std::vector<float> os_, fx_;

        float op(int o, int k) const { return p_[static_cast<size_t>(opParam(o, k))]; }

        std::shared_ptr<const Table> tableOf(int o) const
        {
            if (op(o, OP_TYPE) >= 0.5f && !banks_.empty())
                return banks_[static_cast<size_t>(std::clamp(static_cast<int>(std::lround(op(o, OP_BANK))), 0, static_cast<int>(banks_.size()) - 1))];
            return shapes_[static_cast<size_t>(std::clamp(static_cast<int>(std::lround(op(o, OP_SHAPE))), 0, 3))];
        }

        void voice(Voice &v, float *os, int n)
        {
            const float dt = 1.0f / osr_, twoPi = 6.2831853f;
            // per block: each operator's increment, mip, envelope rates, the matrix scaled for this voice
            const float noteHz = 440.0f * std::exp2((static_cast<float>(v.note) - 69.0f) / 12.0f) * bend_;
            std::array<float, OPS> inc{}, scanInc{}, ca{}, cd{}, cr{}, gl{}, gr{};
            std::array<int, OPS> mip{};
            std::array<bool, OPS> on{};
            float fm[OPS][OPS];
            const float idx = p_[G_INDEX] * (1 - p_[G_VEL_INDEX] * (1 - v.vel)) * (1 + p_[G_WHEEL_INDEX] * wheel_) / twoPi;
            auto coef = [dt](float t) { return 1.0f - std::exp(-6.9f * dt / std::max(t, 0.0005f)); };
            for (int o = 0; o < OPS; ++o)
            {
                const size_t u = static_cast<size_t>(o);
                const float hz = op(o, OP_FIXED) >= 0.5f ? op(o, OP_HZ) * bend_ : noteHz * op(o, OP_RATIO) * std::exp2(op(o, OP_FINE) / 1200.0f);
                inc[u] = hz * dt;
                mip[u] = Table::mipFor(hz, osr_);
                scanInc[u] = op(o, OP_SCAN_RATE) * dt;
                ca[u] = coef(op(o, OP_A));
                cd[u] = coef(op(o, OP_D));
                cr[u] = coef(op(o, OP_R));
                const float out = p_[static_cast<size_t>(outParam(o))] * v.vel * 0.35f, pan = op(o, OP_PAN);
                gl[u] = out * (0.5f - 0.5f * pan) * 2;
                gr[u] = out * (0.5f + 0.5f * pan) * 2;
                on[u] = table_[u] != nullptr;
                for (int c = 0; c < OPS; ++c)
                    fm[o][c] = p_[static_cast<size_t>(fmParam(o, c))] * idx;
            }
            for (int i = 0; i < n; ++i)
            {
                std::array<float, OPS> y{};
                bool alive = false;
                float l = 0, r = 0;
                for (int o = 0; o < OPS; ++o)
                {
                    const size_t u = static_cast<size_t>(o);
                    if (!on[u])
                        continue;
                    // the envelope: each stage closes on its target, -60 dB in its time
                    float &e = v.env[u];
                    int &st = v.stage[u];
                    switch (st)
                    {
                    case 1:
                        e += (1.2f - e) * ca[u];
                        if (e >= 1.0f)
                            e = 1.0f, st = 2;
                        break;
                    case 2:
                        e += (op(o, OP_S) - e) * cd[u];
                        if (std::fabs(e - op(o, OP_S)) < 1e-4f)
                            st = op(o, OP_S) < 1e-4f ? 0 : 3; // a sustain of 0: the envelope is over
                        break;
                    case 3:
                        e = op(o, OP_S);
                        if (e < 1e-4f)
                            st = 0;
                        break;
                    case 4:
                        e -= e * cr[u];
                        if (e < 1e-5f)
                            e = 0, st = 0;
                        break;
                    default: e = 0;
                    }
                    alive |= st != 0 && (gl[u] > 0 || gr[u] > 0);
                    if (e == 0 && st == 0)
                        continue;
                    float pm = 0;
                    for (int m = 0; m < OPS; ++m)
                        if (fm[m][o] != 0)
                            pm += fm[m][o] * (m == o ? 0.5f * (v.y[static_cast<size_t>(m)] + v.y1[static_cast<size_t>(m)]) : v.y[static_cast<size_t>(m)]);
                    float pos = op(o, OP_POS);
                    if (op(o, OP_SCAN_DEPTH) > 0)
                    {
                        pos += op(o, OP_SCAN_DEPTH) * std::sin(twoPi * v.scan[u]);
                        v.scan[u] += scanInc[u];
                        v.scan[u] -= std::floor(v.scan[u]);
                    }
                    v.pos[u] = std::clamp(pos, 0.0f, 1.0f);
                    float ph = v.phase[u] + pm;
                    ph -= std::floor(ph);
                    const float s = table_[u]->read(mip[u], v.pos[u], ph) * op(o, OP_LEVEL) * e;
                    y[u] = s;
                    v.phase[u] += inc[u];
                    v.phase[u] -= std::floor(v.phase[u]);
                    l += s * gl[u];
                    r += s * gr[u];
                }
                v.y1 = v.y;
                v.y = y;
                os[i * 2] += l;
                os[i * 2 + 1] += r;
                if (!alive) // no carrier sounds any more (released, or a sustain of 0 under the key): free it
                {
                    v.active = false;
                    return;
                }
            }
        }
    };

    // ── the factory's patches ───────────────────────────────────────────────────────────────────
    // A preset: its values, and for the WT operators the bank by name (the engine's list may differ).
    struct Preset
    {
        std::string name;
        Params params;
        std::array<std::string, OPS> bank;
    };

    // the bank's index in a list of tables, by name (-1 none)
    inline int bankIndex(const std::vector<std::shared_ptr<const Table>> &banks, const std::string &name)
    {
        for (size_t i = 0; i < banks.size(); ++i)
            if (banks[i] && banks[i]->name() == name)
                return static_cast<int>(i);
        return -1;
    }

    // a preset's values with its banks found in the list (a bank not there: the operator turns STD sine)
    inline Params resolve(const Preset &pr, const std::vector<std::shared_ptr<const Table>> &banks)
    {
        Params p = pr.params;
        for (int o = 0; o < OPS; ++o)
            if (p[static_cast<size_t>(opParam(o, OP_TYPE))] >= 0.5f)
            {
                const int b = bankIndex(banks, pr.bank[static_cast<size_t>(o)]);
                if (b >= 0)
                    p[static_cast<size_t>(opParam(o, OP_BANK))] = static_cast<float>(b);
                else
                    p[static_cast<size_t>(opParam(o, OP_TYPE))] = 0, p[static_cast<size_t>(opParam(o, OP_SHAPE))] = 0;
            }
        return p;
    }

    namespace detail
    {
        struct Op
        {
            const char *table; // "SINE" "SAW" "SQUARE" "TRIANGLE", else a bank's name
            float pos, ratio, fine, level;
            float a, d, s, r;
            float pan = 0, scanRate = 0.2f, scanDepth = 0, hz = 0; // hz > 0: fixed
        };
        inline void put(Preset &pr, int o, const Op &op)
        {
            Params &p = pr.params;
            auto set = [&](int k, float v) { p[static_cast<size_t>(opParam(o, k))] = v; };
            const std::string t = op.table;
            const char *shapes[] = {"SINE", "SAW", "SQUARE", "TRIANGLE"};
            int shape = -1;
            for (int s = 0; s < 4; ++s)
                if (t == shapes[s])
                    shape = s;
            set(OP_TYPE, shape < 0 ? 1.0f : 0.0f);
            set(OP_SHAPE, static_cast<float>(std::max(0, shape)));
            if (shape < 0)
                pr.bank[static_cast<size_t>(o)] = t;
            set(OP_POS, op.pos);
            set(OP_RATIO, op.ratio);
            set(OP_FINE, op.fine);
            set(OP_FIXED, op.hz > 0 ? 1.0f : 0.0f);
            set(OP_HZ, op.hz > 0 ? op.hz : 440.0f);
            set(OP_LEVEL, op.level);
            set(OP_A, op.a);
            set(OP_D, op.d);
            set(OP_S, op.s);
            set(OP_R, op.r);
            set(OP_PAN, op.pan);
            set(OP_SCAN_RATE, op.scanRate);
            set(OP_SCAN_DEPTH, op.scanDepth);
        }
        inline Preset blank(const char *name)
        {
            Preset pr{name, defaultParams(), {}};
            for (int o = 0; o < OPS; ++o) // the unused ones: silent sines
                pr.params[static_cast<size_t>(opParam(o, OP_LEVEL))] = 0;
            pr.params[static_cast<size_t>(outParam(0))] = 0;
            return pr;
        }
        inline void fm(Preset &pr, int r, int c, float v) { pr.params[static_cast<size_t>(fmParam(r, c))] = v; }
        inline void out(Preset &pr, int o, float v) { pr.params[static_cast<size_t>(outParam(o))] = v; }
        inline void g(Preset &pr, int id, float v) { pr.params[static_cast<size_t>(id)] = v; }
    } // namespace detail

    // the banks' names, as loadBanks() names pcm/: "MORPH", "HARMONIC", "ALL WAVES", "BASIC", "SINE TO SAW"
    inline std::vector<Preset> factoryPresets()
    {
        using namespace detail;
        std::vector<Preset> v;
        { // the start: a sine modulated by a sine
            Preset p = blank("Init");
            put(p, 0, {"SINE", 0, 1, 0, 1, 0.005f, 0.6f, 0.8f, 0.4f});
            put(p, 1, {"SINE", 0, 1, 0, 0.8f, 0.005f, 0.8f, 0.6f, 0.4f});
            fm(p, 1, 0, 1.5f);
            out(p, 0, 1);
            g(p, G_VOLUME, 0.9f);
            v.push_back(p);
        }
        { // a frame of a bright table modulating a sine: what CT1 is about
            Preset p = blank("Table Pluck");
            put(p, 0, {"SINE", 0, 1, 0, 1, 0.005f, 0.4f, 0.8f, 0.4f});
            put(p, 1, {"ALL WAVES", 4.0f / 31.0f, 1, 0, 1, 0.005f, 0.6f, 0.6f, 0.4f});
            fm(p, 1, 0, 3.0f);
            out(p, 0, 2);
            g(p, G_REV_MIX, 0.15f), g(p, G_VOLUME, 0.44f);
            v.push_back(p);
        }
        { // two tables slowly scanning, a third: a pad
            Preset p = blank("Two Tables Pad");
            put(p, 0, {"MORPH", 0.4f, 1, 0, 1, 1.2f, 1.0f, 0.85f, 2.0f, -0.2f, 0.07f, 0.35f});
            put(p, 1, {"HARMONIC", 0.3f, 1, 5, 0.9f, 1.5f, 1.0f, 0.9f, 2.0f, 0, 0.13f, 0.2f});
            put(p, 2, {"MORPH", 0.5f, 1, -7, 1, 1.2f, 1.0f, 0.85f, 2.0f, 0.2f, 0.05f, 0.3f});
            fm(p, 1, 0, 0.7f), fm(p, 1, 2, 0.5f);
            out(p, 0, 1), out(p, 2, 1);
            g(p, G_CUTOFF, 9000), g(p, G_REV_MIX, 0.35f), g(p, G_DLY_MIX, 0.12f), g(p, G_VOLUME, 0.15f);
            v.push_back(p);
        }
        { // a chain 3 > 2 > 1, short modulators: a bass
            Preset p = blank("Chain Bass");
            put(p, 0, {"BASIC", 0.25f, 1, 0, 1, 0.002f, 0.6f, 0.5f, 0.12f});
            put(p, 1, {"HARMONIC", 0.3f, 1, 0, 1, 0.001f, 0.18f, 0.15f, 0.1f});
            put(p, 2, {"SAW", 0, 2, 0, 1, 0.001f, 0.09f, 0, 0.1f});
            fm(p, 1, 0, 3.2f), fm(p, 2, 1, 2.5f), fm(p, 0, 0, 0.4f);
            out(p, 0, 2);
            g(p, G_CUTOFF, 3800), g(p, G_RESO, 0.25f), g(p, G_DRIVE, 0.35f), g(p, G_REV_MIX, 0.05f), g(p, G_VOLUME, 0.92f);
            v.push_back(p);
        }
        { // five: two pairs and one modulating both
            Preset p = blank("Five Ops Pad");
            put(p, 0, {"ALL WAVES", 0.2f, 1, 4, 1, 0.9f, 1.5f, 0.8f, 2.5f, -0.6f, 0.09f, 0.12f});
            put(p, 1, {"HARMONIC", 0.4f, 2, 0, 0.8f, 1.2f, 2.0f, 0.7f, 2.5f, 0, 0.07f, 0.2f});
            put(p, 2, {"MORPH", 0.5f, 1, -4, 1, 1.0f, 1.5f, 0.8f, 2.5f, 0.6f, 0.11f, 0.15f});
            put(p, 3, {"SINE", 0, 3, 0, 0.7f, 1.6f, 2.0f, 0.6f, 2.5f});
            put(p, 4, {"HARMONIC", 0.8f, 0.5f, 0, 0.6f, 2.5f, 2.0f, 0.8f, 3.0f, 0, 0.05f, 0.1f});
            fm(p, 1, 0, 0.6f), fm(p, 3, 2, 0.8f), fm(p, 4, 0, 0.3f), fm(p, 4, 2, 0.3f);
            out(p, 0, 1), out(p, 2, 1);
            g(p, G_CUTOFF, 7000), g(p, G_REV_MIX, 0.45f), g(p, G_DLY_MIX, 0.18f), g(p, G_DLY_TIME, 0.42f), g(p, G_VOLUME, 0.1f);
            v.push_back(p);
        }
        { // two tables crossed, each with feedback: a texture
            Preset p = blank("Cross Feedback");
            put(p, 0, {"MORPH", 0.6f, 1, 0, 1, 0.4f, 1, 1, 1.5f, -0.3f, 0.06f, 0.3f});
            put(p, 1, {"ALL WAVES", 0.45f, 1.5f, 3, 0.9f, 0.6f, 1, 1, 1.5f, 0.3f, 0.09f, 0.25f});
            fm(p, 0, 1, 0.9f), fm(p, 1, 0, 1.1f), fm(p, 0, 0, 0.4f), fm(p, 1, 1, 0.35f);
            out(p, 0, 1), out(p, 1, 0.8f);
            g(p, G_CUTOFF, 6000), g(p, G_RESO, 0.2f), g(p, G_REV_MIX, 0.3f), g(p, G_DLY_MIX, 0.2f), g(p, G_VOLUME, 0.225f);
            v.push_back(p);
        }
        { // inharmonic ratios: a bell
            Preset p = blank("Table Bell");
            put(p, 0, {"SINE", 0, 1, 0, 1, 0.001f, 2.5f, 0, 2.0f});
            put(p, 1, {"HARMONIC", 0.2f, 3.5f, 0, 1, 0.001f, 0.8f, 0, 0.8f});
            put(p, 2, {"SINE", 0, 7, 32, 1, 0.001f, 0.3f, 0, 0.3f});
            put(p, 3, {"SQUARE", 0, 2, 0, 0.5f, 0.001f, 3.5f, 0, 3.0f, 0.3f});
            fm(p, 1, 0, 2.4f), fm(p, 2, 1, 1.5f), fm(p, 2, 3, 0.8f);
            out(p, 0, 1), out(p, 3, 0.35f);
            g(p, G_REV_MIX, 0.4f), g(p, G_DLY_MIX, 0.15f), g(p, G_DLY_TIME, 0.5f), g(p, G_VOLUME, 0.3f);
            v.push_back(p);
        }
        { // the harmonic bank modulating, its position scanned: which partial, continuously
            Preset p = blank("Harmonic Scan Lead");
            put(p, 0, {"SINE", 0, 1, 0, 1, 0.01f, 0.5f, 0.9f, 0.3f, -0.2f});
            put(p, 1, {"HARMONIC", 0.5f, 1, 0, 1, 0.01f, 1.0f, 0.9f, 0.4f, 0, 0.15f, 0.5f});
            put(p, 2, {"BASIC", 0.3f, 1, 7, 0.6f, 0.01f, 0.5f, 0.8f, 0.3f, 0.25f});
            fm(p, 1, 0, 1.6f), fm(p, 1, 2, 0.6f);
            out(p, 0, 1), out(p, 2, 0.6f);
            g(p, G_CUTOFF, 12000), g(p, G_DLY_MIX, 0.22f), g(p, G_DLY_TIME, 0.28f), g(p, G_REV_MIX, 0.2f), g(p, G_VOLUME, 0.45f);
            v.push_back(p);
        }
        { // modulators at fixed frequencies: metal
            Preset p = blank("Fixed Metal");
            put(p, 0, {"SQUARE", 0, 1, 0, 1, 0.0005f, 0.25f, 0, 0.2f});
            put(p, 1, {"ALL WAVES", 0.6f, 1, 0, 1, 0.0005f, 0.12f, 0, 0.1f, 0, 0.2f, 0, 1830});
            put(p, 2, {"SINE", 0, 1, 0, 1, 0.0005f, 0.06f, 0, 0.05f, 0, 0.2f, 0, 3170});
            fm(p, 1, 0, 4.0f), fm(p, 2, 1, 3.0f);
            out(p, 0, 2);
            g(p, G_CUTOFF, 15000), g(p, G_DRIVE, 0.2f), g(p, G_REV_MIX, 0.25f), g(p, G_VOLUME, 0.39f);
            g(p, G_VEL_INDEX, 0.6f);
            v.push_back(p);
        }
        { // a sub-octave table, feedback, two carriers apart: a growl
            Preset p = blank("Growl Bass");
            put(p, 0, {"SAW", 0, 1, 0, 1, 0.005f, 0.4f, 0.8f, 0.2f, -0.3f});
            put(p, 1, {"MORPH", 0.4f, 0.5f, 0, 1, 0.005f, 0.6f, 0.7f, 0.2f, 0, 0.5f, 0.3f});
            put(p, 2, {"SINE", 0, 1, 12, 1, 0.005f, 0.4f, 0.8f, 0.2f, 0.3f});
            fm(p, 1, 0, 1.4f), fm(p, 1, 2, 1.2f), fm(p, 0, 0, 0.5f);
            out(p, 0, 2), out(p, 2, 1.6f);
            g(p, G_CUTOFF, 2600), g(p, G_RESO, 0.35f), g(p, G_DRIVE, 0.6f), g(p, G_REV_MIX, 0.05f), g(p, G_VOLUME, 0.4f);
            v.push_back(p);
        }
        { // five carriers stacked, each touching its neighbour
            Preset p = blank("Glass Organ");
            const float ratio[] = {1, 2, 3, 4, 6}, lvl[] = {1, 0.7f, 0.55f, 0.4f, 0.3f};
            for (int i = 0; i < OPS; ++i)
                put(p, i, {i % 2 ? "SINE" : "BASIC", 0.15f * static_cast<float>(i), ratio[i], static_cast<float>(i * 2 - 4), lvl[i], 0.02f, 1.0f, 0.85f, 0.6f,
                           -0.6f + 0.3f * static_cast<float>(i)});
            for (int i = 0; i + 1 < OPS; ++i)
                fm(p, i, i + 1, 0.35f), fm(p, i + 1, i, 0.2f);
            for (int i = 0; i < OPS; ++i)
                out(p, i, 1);
            g(p, G_CUTOFF, 10000), g(p, G_REV_MIX, 0.45f), g(p, G_DLY_MIX, 0.1f), g(p, G_VOLUME, 0.075f);
            v.push_back(p);
        }
        { // the position moved at audio rate: a buzz the scan makes
            Preset p = blank("Audio Rate Scan");
            put(p, 0, {"MORPH", 0.5f, 1, 0, 1, 0.01f, 1.0f, 0.9f, 0.5f, 0, 110, 0.45f});
            put(p, 1, {"SINE", 0, 2, 0, 1, 0.4f, 1.0f, 0.6f, 0.5f});
            fm(p, 1, 0, 0.6f);
            out(p, 0, 1);
            g(p, G_CUTOFF, 9000), g(p, G_REV_MIX, 0.2f), g(p, G_VOLUME, 0.4f);
            v.push_back(p);
        }
        { // a ratio-14 table that dies at once over a table carrier: an electric piano
            Preset p = blank("Table EP");
            put(p, 0, {"BASIC", 0, 1, 0, 1, 0.001f, 2.2f, 0, 0.5f, -0.25f});
            put(p, 1, {"HARMONIC", 0, 1, 0, 1, 0.001f, 1.1f, 0, 0.4f});
            put(p, 2, {"ALL WAVES", 0.2f, 14, 0, 1, 0.0005f, 0.05f, 0, 0.05f});
            put(p, 3, {"SINE", 0, 1, 7, 1, 0.001f, 2.6f, 0, 0.5f, 0.25f});
            put(p, 4, {"BASIC", 0.1f, 1, 0, 1, 0.001f, 0.9f, 0, 0.4f});
            fm(p, 1, 0, 1.1f), fm(p, 2, 0, 2.2f), fm(p, 4, 3, 0.9f);
            out(p, 0, 1), out(p, 3, 0.8f);
            g(p, G_CUTOFF, 11000), g(p, G_REV_MIX, 0.25f), g(p, G_DLY_MIX, 0.08f), g(p, G_VOLUME, 0.15f), g(p, G_VEL_INDEX, 0.7f);
            v.push_back(p);
        }
        { // a closed ring 1 > 2 > 3 > 1 of tables: a drone that keeps turning
            Preset p = blank("Ring Drone");
            put(p, 0, {"MORPH", 0.4f, 1, 0, 1, 1.5f, 1, 1, 2, -0.5f, 0.05f, 0.2f});
            put(p, 1, {"ALL WAVES", 0.5f, 1.5f, 0, 1, 2.0f, 1, 1, 2, 0.5f, 0.07f, 0.2f});
            put(p, 2, {"HARMONIC", 0.3f, 2, 2, 1, 2.5f, 1, 1, 2, 0, 0.04f, 0.2f});
            fm(p, 0, 1, 0.8f), fm(p, 1, 2, 0.8f), fm(p, 2, 0, 0.8f);
            out(p, 0, 1), out(p, 1, 0.7f), out(p, 2, 0.5f);
            g(p, G_INDEX, 1.2f), g(p, G_CUTOFF, 7000), g(p, G_RESO, 0.2f), g(p, G_REV_MIX, 0.4f), g(p, G_DLY_MIX, 0.15f), g(p, G_DLY_TIME, 0.6f), g(p, G_VOLUME, 0.15f);
            v.push_back(p);
        }
        return v;
    }
} // namespace synth::ct1
