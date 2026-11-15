// obssynthct1: the ObsSynthCT1 front panel (docs/mockups/obssynthct1_mockup.py --layout strip), a product of the
// panels' SDK (common/panel_app.hpp: the window, the top bar, SETUP, the CC overlay, the presets, the keyboard, the
// drawing). Across the top the five operators: STD or WT, the shape or the bank (the display's ends step through
// them), RATIO (HZ when FIX), FINE, POSITION, LEVEL, PAN, the envelope, the position's scan; under them the FM matrix
// (row modulates column, the diagonal is feedback, OUT who sounds; an algorithm to start from), FX 1 (filter, drive),
// FX 2 (delay, reverb) and the master (INDEX, velocity and wheel on it, the output).
//   A matrix cell: drag up and down, double click or right click: off.  MIDI on ObsSynthCT1:input.
//   --remote FD: the editor of ObsSynthCT1.clap (the plugin plays; sound.cpp has the link).
#include "../common/panel_app.hpp"
#include "controllers.hpp"
#include "presets_io.hpp"
#include "sound.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using namespace synth;
using namespace synth::ct1;
using namespace synth::ct1app;
using panelkit::Align;
using panelkit::Canvas;
using synth::panel::Control;
using Kind = synth::panel::Kind;

namespace
{
    constexpr float W = 1600, H = 900;
    constexpr float OX = 14, OY = 70, OG = 8, OW = (W - 2 * OX - 4 * OG) / 5, OH = 392; // the operators
    constexpr float BY = OY + OH + 8;                                                  // the row under them
    constexpr float FMX = 14, FMW = 560, BH = H - BY - 10;
    constexpr float FXX = FMX + FMW + 8, FXW = 500;
    constexpr float MXX = FXX + FXW + 8, MXW = W - MXX - 14;


    // what the drawing reads: the values (the SDK's), the tables, the algorithm chosen
    struct Ui
    {
        std::vector<float> *v = nullptr;
        std::vector<std::shared_ptr<const Table>> banks;
        std::array<std::shared_ptr<const Table>, 4> shapes;
        int algo = 0;
        Canvas *cv = nullptr; // while drawing
    };

    Control ctl(Kind k, float x, float y, float a, float b, int param, std::function<void(int)> click = {}, float travel = 200, std::string label = {}, bool on = false)
    {
        Control c;
        c.kind = k, c.x = x, c.y = y, c.a = a, c.b = b, c.param = param, c.click = std::move(click), c.travel = travel, c.label = std::move(label), c.on = on;
        return c;
    }

    float val(const Ui &u, int id) { return fromNorm(id, (*u.v)[static_cast<size_t>(id)]); }
    void setVal(Ui &u, int id, float v) { (*u.v)[static_cast<size_t>(id)] = toNorm(id, v); }
    bool isWt(const Ui &u, int o) { return val(u, opParam(o, OP_TYPE)) >= 0.5f; }
    bool isFixed(const Ui &u, int o) { return val(u, opParam(o, OP_FIXED)) >= 0.5f; }
    bool isCarrier(const Ui &u, int o) { return val(u, outParam(o)) > 0.001f; }

    std::shared_ptr<const Table> tableOf(const Ui &u, int o)
    {
        if (isWt(u, o) && !u.banks.empty())
            return u.banks[static_cast<size_t>(std::clamp(static_cast<int>(val(u, opParam(o, OP_BANK))), 0, static_cast<int>(u.banks.size()) - 1))];
        return u.shapes[static_cast<size_t>(std::clamp(static_cast<int>(val(u, opParam(o, OP_SHAPE))), 0, 3))];
    }

    std::string fmt(const char *f, double v)
    {
        char b[48];
        std::snprintf(b, sizeof b, f, v);
        return b;
    }

    // a parameter's value as the panel says it
    std::string shown(int id, float v)
    {
        const std::string &n = paramInfo()[static_cast<size_t>(id)].name;
        const double d = static_cast<double>(v);
        auto ends = [&](const char *s) { return n.size() >= std::strlen(s) && n.compare(n.size() - std::strlen(s), std::strlen(s), s) == 0; };
        if (ends("_RATIO"))
            return fmt("×%g", d);
        if (ends("_FINE"))
            return fmt("%+.0f c", d);
        if (ends("_HZ") || n == "CUTOFF" || ends("SCAN_RATE"))
            return d >= 1000 ? fmt("%.1f kHz", d / 1000) : d >= 10 ? fmt("%.0f Hz", d) : fmt("%.2f Hz", d);
        if (ends("_A") || ends("_D") || ends("_R") || n == "DLY_TIME")
            return d >= 1 ? fmt("%.2f s", d) : fmt("%.0f ms", d * 1000);
        if (ends("_PAN"))
            return std::fabs(d) < 0.01 ? std::string("C") : fmt(d < 0 ? "L%.0f" : "R%.0f", std::fabs(d) * 100);
        if (n.rfind("FM_", 0) == 0 || n == "INDEX")
            return fmt("%.2f", d);
        if (n.rfind("OUT_", 0) == 0)
            return fmt("%.0f%%", d * 50);
        return fmt("%.0f%%", d * 100);
    }

    // ── the controls ────────────────────────────────────────────────────────────────────────────
    std::vector<Control> layoutOf(Ui &u)
    {
        std::vector<Control> c;
        auto knob = [&](float x, float y, float r, int id, const std::string &label) { c.push_back(ctl(Kind::Knob, x, y, r, 0, id, {}, 200, label)); };
        auto button = [&](float x, float y, float w, float h, const std::string &label, bool on, std::function<void(int)> f)
        { c.push_back(ctl(Kind::Button, x, y, w, h, -1, std::move(f), 200, label, on)); };
        knob(1562, 29, 15, G_VOLUME, "");

        for (int o = 0; o < OPS; ++o)
        {
            const float x = OX + static_cast<float>(o) * (OW + OG), y = OY, step = (OW - 24) / 5;
            const bool wt = isWt(u, o), fixed = isFixed(u, o);
            button(x + 12, y + 40, 46, 18, "STD", !wt, [&u, o](int) { setVal(u, opParam(o, OP_TYPE), 0); });
            button(x + 62, y + 40, 46, 18, "WT", wt, [&u, o](int) { setVal(u, opParam(o, OP_TYPE), 1); });
            button(x + OW - 58, y + 40, 46, 18, "FIX", fixed, [&u, o](int) { setVal(u, opParam(o, OP_FIXED), isFixed(u, o) ? 0.0f : 1.0f); });
            // the display's ends: the shape or the bank
            auto stepTable = [&u, o](int d)
            {
                if (isWt(u, o))
                {
                    const int n = std::max(1, static_cast<int>(u.banks.size()));
                    setVal(u, opParam(o, OP_BANK), static_cast<float>(((static_cast<int>(val(u, opParam(o, OP_BANK))) + d) % n + n) % n));
                }
                else
                    setVal(u, opParam(o, OP_SHAPE), static_cast<float>(((static_cast<int>(val(u, opParam(o, OP_SHAPE))) + d) % 4 + 4) % 4));
            };
            c.push_back(ctl(Kind::Hot, x + 12, y + 64, 28, 92, -1, [stepTable](int) { stepTable(-1); }));
            c.push_back(ctl(Kind::Hot, x + OW - 40, y + 64, 28, 92, -1, [stepTable](int) { stepTable(1); }));
            const float k1 = y + 192;
            knob(x + 12 + step * 0.5f, k1, 16, opParam(o, fixed ? OP_HZ : OP_RATIO), fixed ? "HZ" : "RATIO");
            knob(x + 12 + step * 1.5f, k1, 13, opParam(o, OP_FINE), "FINE");
            if (wt)
                knob(x + 12 + step * 2.5f, k1, 16, opParam(o, OP_POS), "POSITION");
            knob(x + 12 + step * 3.5f, k1, 16, opParam(o, OP_LEVEL), "LEVEL");
            knob(x + 12 + step * 4.5f, k1, 13, opParam(o, OP_PAN), "PAN");
            const float k2 = y + 262, es = (OW - 24) / 4;
            const char *adsr[] = {"A", "D", "S", "R"};
            for (int k = 0; k < 4; ++k)
                knob(x + 12 + es * (static_cast<float>(k) + 0.5f), k2, 11, opParam(o, OP_A + k), adsr[k]);
            if (wt)
            {
                knob(x + OW - 92, y + 344, 11, opParam(o, OP_SCAN_RATE), "SCAN");
                knob(x + OW - 40, y + 344, 11, opParam(o, OP_SCAN_DEPTH), "DEPTH");
            }
        }

        // the matrix: the algorithm, then the cells (row modulates column, OUT)
        const int na = static_cast<int>(algorithms().size());
        auto algo = [&u, na](int d)
        {
            u.algo = ((u.algo + d) % na + na) % na;
            Params p{};
            for (int i = 0; i < NUM_PARAMS; ++i)
                p[static_cast<size_t>(i)] = val(u, i);
            applyAlgorithm(p, u.algo);
            for (int r = 0; r < OPS; ++r)
            {
                for (int k = 0; k < OPS; ++k)
                    setVal(u, fmParam(r, k), p[static_cast<size_t>(fmParam(r, k))]);
                setVal(u, outParam(r), p[static_cast<size_t>(outParam(r))]);
            }
        };
        c.push_back(ctl(Kind::Hot, FMX + 12, BY + 40, 40, 26, -1, [algo](int) { algo(-1); }));
        c.push_back(ctl(Kind::Hot, FMX + FMW - 52, BY + 40, 40, 26, -1, [algo](int) { algo(1); }));
        const float gx = FMX + 46, gy = BY + 96, cs = 56;
        for (int r = 0; r < OPS; ++r)
            for (int k = 0; k <= OPS; ++k)
                c.push_back(ctl(Kind::Cell, gx + static_cast<float>(k) * (cs + (k == OPS ? 10 : 0)), gy + static_cast<float>(r) * cs, cs, cs, k == OPS ? outParam(r) : fmParam(r, k), {}, 160));

        // FX 1, FX 2, the master
        const float f1 = BY + 118, f2 = BY + 318, s1 = FXW / 3, s2 = FXW / 5;
        knob(FXX + s1 * 0.5f, f1, 19, G_CUTOFF, "CUTOFF");
        knob(FXX + s1 * 1.5f, f1, 19, G_RESO, "RESO");
        knob(FXX + s1 * 2.5f, f1, 19, G_DRIVE, "DRIVE");
        const int fx2[] = {G_DLY_TIME, G_DLY_FB, G_DLY_MIX, G_REV_SIZE, G_REV_MIX};
        const char *fx2n[] = {"TIME", "FDBK", "DELAY", "SIZE", "REVERB"};
        for (int k = 0; k < 5; ++k)
            knob(FXX + s2 * (static_cast<float>(k) + 0.5f), f2, 16, fx2[k], fx2n[k]);
        knob(MXX + 70, BY + 130, 24, G_INDEX, "INDEX");
        knob(MXX + 170, BY + 130, 17, G_VEL_INDEX, "VEL > IDX");
        knob(MXX + 260, BY + 130, 17, G_WHEEL_INDEX, "WHEEL > IDX");
        knob(MXX + 70, BY + 270, 20, G_VOLUME, "VOLUME");
        return c;
    }

    std::string layoutKey(const Ui &u)
    {
        std::string k;
        for (int o = 0; o < OPS; ++o)
            k += isWt(u, o) ? 'W' : 'S';
        return k;
    }

    std::string bodyOf(const panelkit::Theme &t, const std::vector<Control> &ctls, const Ui &u)
    {
        std::ostringstream s;
        for (int o = 0; o < OPS; ++o)
        {
            const float x = OX + static_cast<float>(o) * (OW + OG);
            panelkit::svgPanel(s, x, OY, OW, OH);
            panelkit::svgRule(s, x + 10, OY + 31, OW - 20);
            panelkit::svgDisplay(s, t, x + 12, OY + 64, OW - 24, 92);
            panelkit::svgDisplay(s, t, x + 12, OY + 300, isWt(u, o) ? OW - 130 : OW - 24, 80);
        }
        panelkit::svgPanel(s, FMX, BY, FMW, BH);
        panelkit::svgRule(s, FMX + 10, BY + 31, FMW - 20);
        panelkit::svgDisplay(s, t, FMX + 12, BY + 40, FMW - 24, 26);
        panelkit::svgPanel(s, FXX, BY, FXW, 196);
        panelkit::svgRule(s, FXX + 10, BY + 31, FXW - 20);
        panelkit::svgPanel(s, FXX, BY + 204, FXW, BH - 204);
        panelkit::svgRule(s, FXX + 10, BY + 235, FXW - 20);
        panelkit::svgPanel(s, MXX, BY, MXW, BH);
        panelkit::svgRule(s, MXX + 10, BY + 31, MXW - 20);
        panelkit::svgDisplay(s, t, MXX + MXW - 110, BY + 50, 88, BH - 70);
        for (const Control &c : ctls)
            if (c.kind == Kind::Knob)
                panelkit::svgKnob(s, t, c.x, c.y, c.a);
        return s.str();
    }

    void drawTable(Ui &u, int o, float x, float y, float w, float h, float live)
    {
        Canvas &cv = *u.cv;
        const panelkit::Theme &t = cv.theme;
        const auto tb = tableOf(u, o);
        if (!tb)
            return;
        cv.say("‹ " + tb->name() + " ›", x + 10, y + 13, 8, t.accent, 0.75f, false, true, 1, Align::Left, false);
        if (!isWt(u, o))
        {
            std::vector<SDL_FPoint> pts; // two cycles
            for (int i = 0; i <= 200; ++i)
                pts.push_back({x + 10 + static_cast<float>(i) * (w - 20) / 200, y + h * 0.58f - h * 0.3f * tb->sample(0, (i * 2048 / 200) % 1024)});
            cv.lines(pts, t.accent, 1, 1.6f);
            return;
        }
        // the frames stacked in depth; the knob's frame lighter, the one playing (its scan, the newest voice) in accent
        const int nf = tb->frames(), shown = std::min(12, nf);
        const float fw = w * 0.62f, amp = h * 0.16f, span = static_cast<float>(std::max(1, shown - 1));
        auto plot = [&](int f, float k, const std::string &hex, float a, float thick)
        {
            const float bx = x + 10 + k * (w - fw - 20) / span, by = y + h - 14 - k * (h - 40) / span;
            std::vector<SDL_FPoint> pts;
            for (int i = 0; i <= 64; ++i)
                pts.push_back({bx + static_cast<float>(i) * fw / 64, by - amp * tb->sample(f, i * 16)});
            cv.lines(pts, hex, a, thick);
        };
        for (int k = shown - 1; k >= 0; --k)
            plot(static_cast<int>(std::lround(static_cast<float>(k) * static_cast<float>(nf - 1) / span)), static_cast<float>(k), t.trace, 0.25f + 0.45f * (1 - static_cast<float>(k) / static_cast<float>(shown)), 1);
        const float pos = val(u, opParam(o, OP_POS)), now = live >= 0 ? live : pos, last = static_cast<float>(nf - 1);
        const int fk = static_cast<int>(std::lround(pos * last)), fl = static_cast<int>(std::lround(now * last));
        if (fl != fk)
            plot(fk, pos * span, t.ink, 0.85f, 1.6f);
        plot(fl, now * span, t.accent, 1, 2.2f);
        cv.say(fl != fk ? fmt("%02.0f", fk + 1.0) + " → " + fmt("%02.0f", fl + 1.0) + " / " + std::to_string(nf) : fmt("%02.0f", fl + 1.0) + " / " + std::to_string(nf),
               x + w - 10, y + 13, 8, t.accent, 0.8f, false, true, 1, Align::Right, false);
        const float sx = x + 10, sw = w - 20, sy = y + h - 6;
        cv.rect(sx, sy, sw, 2, t.trace, 0.35f);
        cv.rect(std::round(sx + pos * sw) - 1, sy - 3, 2, 8, t.ink, 0.8f);
        if (live >= 0)
            cv.rect(std::round(sx + now * sw) - 2, sy - 2, 4, 6, t.accent);
    }

    void drawEnv(Ui &u, int o, float x, float y, float w, float h)
    {
        Canvas &cv = *u.cv;
        const panelkit::Theme &t = cv.theme;
        const float a = val(u, opParam(o, OP_A)), d = val(u, opParam(o, OP_D)), s = val(u, opParam(o, OP_S)), r = val(u, opParam(o, OP_R));
        // a log-ish time axis: each stage its share
        const float ta = std::log1p(a * 20), td = std::log1p(d * 20), tr = std::log1p(r * 20), hold = 0.6f, sum = ta + td + hold + tr;
        auto X = [&](float tt) { return x + 8 + tt / sum * (w - 16); };
        auto Y = [&](float v) { return y + h - 8 - v * (h - 22); };
        std::vector<SDL_FPoint> pts{{X(0), Y(0)}, {X(ta), Y(1)}, {X(ta + td), Y(s)}, {X(ta + td + hold), Y(s)}, {X(sum), Y(0)}};
        cv.lines(pts, t.traceHi, 1, 1.5f);
        cv.say(isCarrier(u, o) ? "ENV · LEVEL" : "ENV · INDEX", x + 8, y + 12, 7, t.accent, 0.75f, false, true, 1, Align::Left, false);
    }

    // a folder that holds the banks: by the program (installed), the source tree's pcm/ (built here), the working one
    std::string pcmDir(const char *argv0)
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> tries;
        if (const char *base = SDL_GetBasePath())
        {
            tries.push_back(fs::path(base) / "pcm");
            tries.push_back(fs::path(base) / ".." / ".." / ".." / "pcm");
            tries.push_back(fs::path(base) / ".." / ".." / ".." / ".." / "pcm");
        }
        tries.push_back(fs::path(argv0).parent_path() / "pcm");
        tries.push_back("pcm");
        for (const auto &p : tries)
        {
            std::error_code ec;
            if (fs::is_directory(p, ec))
                return p.string();
        }
        return "pcm";
    }

    // ── the sound: sound.cpp's engine and link; the SDK speaks 0..1, the engine physical values ──
    class Ct1Engine : public synth::panel::Engine
    {
    public:
        std::vector<std::shared_ptr<const Table>> *banks = nullptr;
        bool start(const std::vector<float> &values, const io::Settings &settings) override
        {
            Params p{};
            for (int i = 0; i < NUM_PARAMS; ++i)
                p[static_cast<size_t>(i)] = fromNorm(i, values[static_cast<size_t>(i)]);
            return sound_.start(*banks, p.data(), settings);
        }
        bool connect(const char *link) override { return sound_.connect(link); }
        bool remote() const override { return sound_.remote(); }
        bool remoteAlive() const override { return sound_.remoteAlive(); }
        bool pollRemote(int &id, float &value) override
        {
            if (!sound_.pollRemote(id, value))
                return false;
            if (id >= 0 && id < NUM_PARAMS)
                value = toNorm(id, value);
            return true;
        }
        void setParam(int id, float value) override { sound_.setParam(id, fromNorm(id, value)); }
        void noteOn(int note, float velocity) override { sound_.noteOn(note, velocity); }
        void noteOff(int note) override { sound_.noteOff(note); }
        bool pollCc(int &channel, int &cc, float &value) override { return sound_.pollCc(channel, cc, value); }
        io::MidiIn &midiIn() override { return sound_.midiIn(); }
        const io::AudioOut &audioOut() const override { return sound_.audioOut(); }
        bool reopenAudio(const io::Settings &settings) override { return sound_.reopenAudio(settings); }
        int sounding() const override { return sound_.sounding(); }
        float livePos(int op) const { return sound_.livePos(op); }
        float outPeak(int side) const { return sound_.outPeak(side); }

    private:
        Sound sound_;
    };

    class Ct1 : public synth::panel::Product
    {
    public:
        explicit Ct1(const char *argv0)
        {
            u_.banks = loadBanks(pcmDir(argv0));
            for (int s = 0; s < 4; ++s)
                u_.shapes[static_cast<size_t>(s)] = Table::fromShape(static_cast<Table::Shape>(s));
            if (u_.banks.empty())
                std::fprintf(stderr, "obssynthct1: no wavetable banks found (pcm/bank_*.wav): the WT operators fall back to the sine\n");
            engine_.banks = &u_.banks;
        }

        const char *id() const override { return "obssynthct1"; }
        const char *title() const override { return "ObsSynthCT1"; }
        const char *barNumber() const override { return "CT1"; }

        int params() const override { return NUM_PARAMS; }
        std::string paramName(int id) const override { return paramInfo()[static_cast<size_t>(id)].name; }
        float paramDefault(int id) const override { return toNorm(id, paramInfo()[static_cast<size_t>(id)].def); }
        std::string paramText(int id, float v) const override { return shown(id, fromNorm(id, v)); }

        std::vector<synth::panel::PresetEntry> presets() override
        {
            std::vector<synth::panel::PresetEntry> v;
            for (const auto &e : listPresets())
                v.push_back({e.name, e.path, e.factory});
            return v;
        }
        bool loadPreset(const synth::panel::PresetEntry &e, std::vector<float> &values, std::string &name) override
        {
            Preset pr;
            if (!ct1app::loadPreset({e.name, e.path, e.factory}, pr))
                return false;
            const Params p = resolve(pr, u_.banks);
            for (int i = 0; i < NUM_PARAMS; ++i)
                values[static_cast<size_t>(i)] = toNorm(i, p[static_cast<size_t>(i)]);
            name = pr.name;
            return true;
        }
        bool savePreset(const std::string &name, const std::vector<float> &values, std::string &path) override
        {
            u_.v = const_cast<std::vector<float> *>(&values);
            Preset pr;
            pr.name = name;
            for (int i = 0; i < NUM_PARAMS; ++i)
                pr.params[static_cast<size_t>(i)] = val(u_, i);
            for (int o = 0; o < OPS; ++o)
                if (isWt(u_, o))
                    pr.bank[static_cast<size_t>(o)] = tableOf(u_, o)->name();
            return ct1app::savePreset(pr, path);
        }

        std::vector<ccov::Map> controllers() const override { return factoryControllers(); }
        synth::panel::Engine &engine() override { return engine_; }

        std::string layoutKey(const std::vector<float> &values) const override
        {
            Ui u = u_;
            u.v = const_cast<std::vector<float> *>(&values);
            return ::layoutKey(u);
        }
        std::vector<Control> layout(std::vector<float> &values) override
        {
            u_.v = &values;
            return layoutOf(u_);
        }
        std::string bodySvg(const panelkit::Theme &t, const std::string &key) override
        {
            std::vector<float> v(static_cast<size_t>(NUM_PARAMS), 0.0f); // the layout the key names: each operator STD or WT
            for (int o = 0; o < OPS && o < static_cast<int>(key.size()); ++o)
                v[static_cast<size_t>(opParam(o, OP_TYPE))] = key[static_cast<size_t>(o)] == 'W' ? 1.0f : 0.0f;
            Ui u = u_;
            u.v = &v;
            return bodyOf(t, layoutOf(u), u);
        }

        void drawBody(Canvas &cv, const std::vector<float> &values, const synth::panel::Clip &clip) override
        {
            Ui &u = u_;
            u.v = const_cast<std::vector<float> *>(&values);
            u.cv = &cv;
            const panelkit::Theme &t = cv.theme;
            if (clip.touches(0, 0, W, 58))
                cv.say("MASTER", 1534, 34, 10, t.ink, 1, true, false, 1.6f, Align::Right);
        // the operators
        for (int o = 0; o < OPS; ++o)
        {
            const float x = OX + static_cast<float>(o) * (OW + OG), y = OY;
            cv.say("OP " + std::to_string(o + 1), x + 12, y + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
            const bool off = val(u, opParam(o, OP_LEVEL)) < 0.001f;
            cv.say(off ? "OFF" : isCarrier(u, o) ? "CARRIER" : "MODULATOR", x + OW - 12, y + 22, 8, isCarrier(u, o) && !off ? t.accent : t.dim, 1, false, false, 1.4f, Align::Right);
            drawTable(u, o, x + 12, y + 64, OW - 24, 92, isWt(u, o) ? engine_.livePos(o) : -1);
            drawEnv(u, o, x + 12, y + 300, isWt(u, o) ? OW - 130 : OW - 24, 80);
        }

        // the matrix
        cv.say("FM", FMX + 12, BY + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
        cv.say("ROW → COLUMN  ·  DIAGONAL: FEEDBACK", FMX + FMW - 12, BY + 22, 8, t.dim, 1, false, false, 1.2f, Align::Right);
        cv.say("‹", FMX + 26, BY + 59, 13, t.accent, 1, false, true, 0, Align::Centre, false);
        cv.say("›", FMX + FMW - 26, BY + 59, 13, t.accent, 1, false, true, 0, Align::Centre, false);
        // the algorithm, while the matrix is still it (its links, as amounts and OUT left aside)
        bool same = true;
        {
            const Algorithm &al = algorithms()[static_cast<size_t>(u.algo)];
            for (int r = 0; r < OPS; ++r)
            {
                for (int k = 0; k < OPS; ++k)
                    same &= ((*u.v)[static_cast<size_t>(fmParam(r, k))] > 0.001f) ==
                            (std::find(al.links.begin(), al.links.end(), std::make_pair(r, k)) != al.links.end());
                same &= ((*u.v)[static_cast<size_t>(outParam(r))] > 0.001f) == (std::find(al.outs.begin(), al.outs.end(), r) != al.outs.end());
            }
        }
        cv.say(same ? "ALGO " + fmt("%02.0f", u.algo + 1.0) + "  ·  " + algorithms()[static_cast<size_t>(u.algo)].name : std::string("CUSTOM  ·  ‹ › AN ALGORITHM"),
               FMX + FMW / 2, BY + 58, 11, t.accent, same ? 1.0f : 0.7f, false, true, 1, Align::Centre, false);
        const float gx = FMX + 46, gy = BY + 96, cs = 56;
        for (int k = 0; k <= OPS; ++k)
            cv.say(k == OPS ? "OUT" : std::to_string(k + 1), gx + static_cast<float>(k) * (cs + (k == OPS ? 10 : 0)) + cs / 2, gy - 8, 9, k == OPS ? t.accent : t.ink, 1, true, false, 1, Align::Centre);
        for (int r = 0; r < OPS; ++r)
            cv.say(std::to_string(r + 1), gx - 18, gy + static_cast<float>(r) * cs + cs / 2 + 4, 9, t.ink, 1, true, false, 1, Align::Centre);

        // FX, master
        cv.say("FX 1  ·  FILTER · DRIVE", FXX + 12, BY + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
        cv.say("FX 2  ·  DELAY · REVERB", FXX + 12, BY + 226, 12, t.ink, 1, true, false, 2.4f, Align::Left);
        cv.say("MASTER", MXX + 12, BY + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
        for (int side = 0; side < 2; ++side)
        {
            const float v = std::min(1.0f, engine_.outPeak(side)), bx = MXX + MXW - 98 + static_cast<float>(side) * 40, by = BY + 62, bh = BH - 104;
            cv.rect(bx, by + bh * (1 - v), 24, bh * v, v > 0.89f ? t.accent : t.traceHi, 0.9f);
            cv.say(side ? "R" : "L", bx + 12, by + bh + 16, 9, t.ink, 1, true, false, 0, Align::Centre);
        }
        cv.say(engine_.remote() ? "Z..M  Q..U  PLAY  ·  IN A HOST: ITS TRACK'S MIDI" : "Z..M  Q..U  PLAY  ·  MIDI: OBSSYNTHCT1:INPUT", MXX + 16, BY + BH - 20, 7, t.dim, 1,
               false, true, 1, Align::Left, false);

        }

        bool drawControl(Canvas &cv, const Control &c, float) override
        {
            Ui &u = u_;
            const panelkit::Theme &t = cv.theme;
            switch (c.kind)
            {
            case Kind::Knob:
            {
                const float n = (*u.v)[static_cast<size_t>(c.param)];
                cv.cap(c.x, c.y, c.a, n);
                if (!c.label.empty())
                {
                    cv.say(c.label, c.x, c.y + c.a + 15, 8, t.ink, 1, true, false, 1, Align::Centre);
                    cv.say(shown(c.param, fromNorm(c.param, n)), c.x, c.y + c.a + 26, 7.5f, t.accent, 1, false, true, 0, Align::Centre, false);
                }
                break;
            }
            case Kind::Button:
                cv.box(c.x, c.y, c.a, c.b, c.on);
                cv.say(c.label, c.x + c.a / 2, c.y + c.b / 2 + 3.5f, 7.5f, c.on ? t.accent : t.dim, 1, true, false, 1, Align::Centre);
                break;
            case Kind::Cell:
            {
                const bool out = c.param >= P_OUT0, diag = !out && (c.param - P_FM0) / OPS == (c.param - P_FM0) % OPS;
                cv.rect(c.x + 2, c.y + 2, c.a - 4, c.b - 4, diag ? t.row : t.btnOff, 1);
                cv.rect(c.x + 2, c.y + 2, c.a - 4, c.b - 4, "#000000", 0.8f, false);
                const float n = (*u.v)[static_cast<size_t>(c.param)];
                if (n > 0.001f)
                {
                    const float rr = 3 + n * (c.a / 2 - 7);
                    cv.rect(c.x + c.a / 2 - rr, c.y + c.b / 2 - rr, 2 * rr, 2 * rr, t.accent, 0.35f + 0.6f * n);
                    cv.say(shown(c.param, fromNorm(c.param, n)), c.x + c.a - 6, c.y + c.b - 6, 6.5f, t.ink, 0.9f, false, true, 0, Align::Right, false);
                }
                break;
            }
            case Kind::Hot: break;
            default: return false;
            }
            return true;
        }

        // a value drawn besides its control: an operator's column (its pictures, its header); the matrix's panel
        // (the algorithm's name); OUT also the operators' headers (CARRIER)
        std::vector<SDL_FRect> damageOf(int id, const std::vector<float> &) override
        {
            if (id < P_FM0)
            {
                const int o = id / OP_PARAMS;
                return {{OX + static_cast<float>(o) * (OW + OG), OY, OW, OH}};
            }
            if (id < P_OUT0)
                return {{FMX, BY, FMW, BH}};
            if (id < P_OUT0 + OPS)
                return {{FMX, BY, FMW, BH}, {OX, OY, W - 2 * OX, 32}};
            return {};
        }
        // what moves by itself: the WT operators' positions now, the output's meters
        std::vector<SDL_FRect> live() override
        {
            std::vector<SDL_FRect> r;
            for (int o = 0; o < OPS; ++o)
            {
                const int p = static_cast<int>(engine_.livePos(o) * 1000);
                if (p != lastPos_[static_cast<size_t>(o)])
                    r.push_back({OX + static_cast<float>(o) * (OW + OG) + 12, OY + 64, OW - 24, 92});
                lastPos_[static_cast<size_t>(o)] = p;
            }
            const int pk[2] = {static_cast<int>(engine_.outPeak(0) * 400), static_cast<int>(engine_.outPeak(1) * 400)};
            if (pk[0] != lastPeak_[0] || pk[1] != lastPeak_[1])
                r.push_back({MXX + MXW - 110, BY + 50, 88, BH - 50});
            lastPeak_[0] = pk[0], lastPeak_[1] = pk[1];
            return r;
        }

    private:
        Ui u_;
        Ct1Engine engine_;
        std::array<int, OPS> lastPos_{};
        int lastPeak_[2] = {0, 0};
    };
} // namespace

int main(int argc, char **argv)
{
    Ct1 ct1(argv[0]);
    return synth::panel::run(ct1, argc, argv);
}
