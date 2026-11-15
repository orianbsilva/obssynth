// obssynthone: the ObsSynth One front panel (docs/mockups/obssynthone_mockup.py), a product of the panels' SDK
// (common/panel_app.hpp: the window, the top bar, SETUP, the CC overlay, the presets, the keyboard, the drawing).
// One oscillator with five engines (STD, WT, FM, GRAIN, SMP): its display shows the engine's own picture; the big
// knob is the engine's position (the wavetable's frame, the FM index, the grains' read point, the sample's start);
// then the filter, two envelopes, the tube, the delay and the master; the MOD page has two LFOs and four routes.
// What is the One's here: its values (one_params.hpp, 0..1), its layout (laid out in 1280 x 780 units under its own
// bar and drawn at the SDK's 1600 x 900: Z, OX, OY), its displays, its presets (the factory's in one_driver.hpp,
// the person's in <data>/obssynthone/presets), its controllers, and its sound (OneEngine):
//   standing alone: sound.cpp (the engine in the machine's profile, the audio out, MIDI on ObsSynthOne:input);
//   --remote LINK: the editor of ObsSynthOne.clap (the plugin plays), lines of text both ways:
//     panel to plugin  "p ID VALUE" a parameter (0..1; ID = one_params.hpp's)
//     plugin to panel  "v ID VALUE" a value the host changed, "P INDEX NAME" the preset, "C COUNT", "L INDEX PATH" a recording
#include "../common/data_home.hpp"
#include "../common/panel_app.hpp"
#include "../common/remote_link.hpp"
#include "../../vendor/json.hpp"
#include "one_driver.hpp"
#include "sound.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace onep;
using panelkit::Align;
using panelkit::Canvas;

namespace
{
    // the body's own units (under its bar, which is the SDK's now), and how they sit in the SDK's 1600 x 900
    constexpr float W = 1280, H = 780;
    constexpr float Z = (synth::panel::H - synth::panel::BAR) / (H - 58.0f);
    constexpr float OX = (synth::panel::W - W * Z) / 2, OY = synth::panel::BAR - 58.0f * Z;
    constexpr int ENGINES = 5;
    const char *kEngine[ENGINES] = {"STD", "WT", "FM", "GRAIN", "SMP"};
    const char *kWave[6] = {"SIN", "SAW", "SQR", "TRI", "PLS", "NSE"};
    const char *kFilter[4] = {"LP", "HP", "BP", "NOTCH"};

    enum class Kind { Knob, Fader, Button };
    struct Ctl
    {
        Kind kind;
        float x, y, a, b; // knob: cx cy r; fader: x y h; button: x y w h (the body's units)
        int id = -1;
        std::string label;
        int choice = -1;  // a button: the value it sets (index); a step button: the way (-1, 1)
        int mode = 0;     // a button: 0 sets choice, 1 steps the parameter by choice, 2 toggles it
        bool on = false;
        bool big = false;
    };

    // what the One's drawing reads: the values, the page, the wavetable's frames, the recordings
    struct View
    {
        const std::vector<float> *v = nullptr;
        int page = 0; // 0: the oscillator, filter, envelopes and effects; 1: the LFOs and the routes
        std::vector<std::vector<float>> frames;
        int bankFrames = 0;
        std::mutex &listLock;
        std::vector<std::string> &samples;
        std::map<std::string, std::vector<float>> peaks;
        synth::panel::Clip clip;
        Canvas *cv = nullptr; // while drawing
        View(std::mutex &l, std::vector<std::string> &s) : listLock(l), samples(s) {}
        float get(int id) const { return v ? (*v)[static_cast<size_t>(id)] : 0.0f; }
        int engine() const { return std::clamp(static_cast<int>(std::lround(get(idOf("OSC_PART_TYPE")) * (ENGINES - 1))), 0, ENGINES - 1); }
        bool touches(float x, float y, float w, float h) const { return clip.touches(x * Z + OX, y * Z + OY, w * Z, h * Z); }
    };

    std::string findBankFile()
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> dirs;
        if (const char *e = SDL_getenv("OBS_PCM_DIR"))
            dirs.emplace_back(e);
        if (const char *b = SDL_GetBasePath())
        {
            dirs.push_back(fs::path(b) / "pcm");
            dirs.push_back(fs::path(b) / ".." / "pcm");
        }
#ifdef SYNTH_PCM_DIR
        dirs.emplace_back(SYNTH_PCM_DIR);
#endif
        for (const auto &d : dirs)
        {
            std::error_code ec;
            const fs::path f = d / "bank_morph_32waves_1024samples_32float.wav";
            if (fs::is_regular_file(f, ec))
                return f.string();
        }
        return {};
    }

    // some frames of the bank (float32 wav, frames of 1024 samples), resampled to 64 points
    void loadFrames(View &u)
    {
        const std::string path = findBankFile();
        if (path.empty())
            return;
        std::ifstream f(path, std::ios::binary);
        std::string b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        const auto at = b.find("data");
        if (at == std::string::npos)
            return;
        uint32_t n = 0;
        std::memcpy(&n, b.data() + at + 4, 4);
        const size_t count = std::min<size_t>(n, b.size() - at - 8) / 4;
        std::vector<float> smp(count);
        std::memcpy(smp.data(), b.data() + at + 8, count * 4);
        const size_t total = count / 1024;
        if (total < 2)
            return;
        u.bankFrames = static_cast<int>(total);
        for (int k = 0; k < 16; ++k)
        {
            const size_t fr = static_cast<size_t>(std::lround(static_cast<double>(k) * static_cast<double>(total - 1) / 15.0));
            std::vector<float> pts;
            for (int j = 0; j < 64; ++j)
                pts.push_back(smp[fr * 1024 + static_cast<size_t>(j * 1024 / 64)]);
            u.frames.push_back(std::move(pts));
        }
    }

    const std::vector<float> *peaksOf(View &u, const std::string &path)
    {
        auto it = u.peaks.find(path);
        if (it != u.peaks.end())
            return &it->second;
        std::vector<float> cols;
        if (auto src = synth::GrainSource::loadWav(path); src && !src->samples.empty())
        {
            cols.assign(400, 0.0f);
            const size_t n = src->samples.size();
            for (size_t i = 0; i < n; ++i)
            {
                float &c = cols[i * 400 / n];
                c = std::max(c, std::fabs(src->samples[i]));
            }
        }
        return &u.peaks.emplace(path, std::move(cols)).first->second;
    }


    // ── the layout: what is on the panel for the engine chosen ──────────────────────────────────────────
    void knob(std::vector<Ctl> &c, const char *name, float x, float y, float r, const char *label, bool big = false)
    {
        c.push_back({Kind::Knob, x, y, r, 0, idOf(name), label, -1, 0, false, big});
    }
    void fader(std::vector<Ctl> &c, const char *name, float x, float y, float h, const char *label)
    {
        c.push_back({Kind::Fader, x, y, h, 0, idOf(name), label});
    }
    void choose(std::vector<Ctl> &c, const char *name, int choice, int steps, float x, float y, float w, const char *label, bool on, int mode = 0)
    {
        (void)steps;
        c.push_back({Kind::Button, x, y, w, 18, idOf(name), label, choice, mode, on});
    }

    // the second page: two LFOs and four routes
    std::vector<Ctl> layoutMod()
    {
        std::vector<Ctl> c;
        const char *rate[2] = {"LFO1_RATE", "LFO2_RATE"}, *wave[2] = {"LFO1_WAVE", "LFO2_WAVE"};
        for (int i = 0; i < 2; ++i)
        {
            const float x0 = 14 + static_cast<float>(i) * 630;
            knob(c, rate[i], x0 + 90, 72 + 130, 40, "RATE", true);
            for (int k = 0; k < 5; ++k)
                choose(c, wave[i], k, 5, x0 + 200 + static_cast<float>(k % 3) * 130, 72 + 90 + static_cast<float>(k / 3) * 40, 120, "", false);
        }
        for (int r = 0; r < 4; ++r)
        {
            const float y = 386 + static_cast<float>(r) * 90;
            const std::string n = std::to_string(r + 1);
            const int src = idOf(("MOD" + n + "_SRC").c_str()), dst = idOf(("MOD" + n + "_DST").c_str());
            c.push_back({Kind::Button, 250, y, 26, 18, src, "<", -1, 1, false, false});
            c.push_back({Kind::Button, 440, y, 26, 18, src, ">", 1, 1, false, false});
            c.push_back({Kind::Button, 640, y, 26, 18, dst, "<", -1, 1, false, false});
            c.push_back({Kind::Button, 830, y, 26, 18, dst, ">", 1, 1, false, false});
            c.push_back({Kind::Knob, 1000, y + 12, 22, 0, idOf(("MOD" + n + "_AMT").c_str()), "AMOUNT", -1, 0, false, false});
        }
        return c;
    }

    std::vector<Ctl> layout(const View &u, int eng, int page)
    {
        if (page == 1)
            return layoutMod();
        std::vector<Ctl> c;
        const float ry = 72;
        // the oscillator
        for (int i = 0; i < ENGINES; ++i)
            choose(c, "OSC_PART_TYPE", i, ENGINES, 30 + static_cast<float>(i) * 56, ry + 52, 52, kEngine[i], i == eng);
        if (eng == 0)
        {
            const int w = std::clamp(static_cast<int>(std::lround(u.get(idOf("OSC_WAVE")) * 5)), 0, 5);
            for (int i = 0; i < 6; ++i)
                choose(c, "OSC_WAVE", i, 6, 340 + static_cast<float>(i) * 46, ry + 52, 42, kWave[i], i == w);
        }
        if (eng == 4)
        {
            choose(c, "OSC_SMP_LOOP", 1, 2, 340, ry + 52, 44, "LOOP", u.get(idOf("OSC_SMP_LOOP")) > 0.5f, 2);
            choose(c, "OSC_SMP_SAMPLE", -1, 64, 400, ry + 52, 26, "<", false, 1);
            choose(c, "OSC_SMP_SAMPLE", 1, 64, 430, ry + 52, 26, ">", false, 1);
        }
        float sx = eng == 2 ? 146.0f : 156.0f;
        const float sy = ry + 212;
        auto smallKnob = [&](const char *name, const char *label) {
            knob(c, name, sx, sy, 15, label);
            sx += eng == 2 ? 50.0f : 60.0f;
        };
        switch (eng)
        {
        case 0:
            knob(c, "OSC_TUNE", 78, ry + 206, 22, "TUNE", true);
            sx = 170;
            break;
        case 1:
            knob(c, "OSC_WT_MORPH", 78, ry + 206, 22, "POSITION", true);
            smallKnob("OSC_TUNE", "TUNE");
            break;
        case 2:
            knob(c, "OSC_FM_MOD_INDEX", 78, ry + 206, 22, "INDEX", true);
            smallKnob("OSC_TUNE", "TUNE");
            break;
        case 3:
            knob(c, "OSC_WT_MORPH", 78, ry + 206, 22, "READ", true);
            smallKnob("OSC_TUNE", "TUNE");
            break;
        default:
            knob(c, "OSC_WT_MORPH", 78, ry + 206, 22, "START", true);
            smallKnob("OSC_TUNE", "TUNE");
            break;
        }
        smallKnob("OSC_FINE_TUNE", "FINE");
        smallKnob("OSC_LEVEL", "LEVEL");
        smallKnob("OSC_PAN", "PAN");
        if (eng == 2)
        {
            smallKnob("OSC_FM_ALGORITHM", "ALGO");
            smallKnob("OSC_FM_MOD_RATIO", "RATIO");
            smallKnob("OSC_FM_FEEDBACK", "FDBK");
            smallKnob("OSC_FM_OP2_RATIO", "OP2 R");
            smallKnob("OSC_FM_OP2_LEVEL", "OP2 L");
        }
        else if (eng == 4)
            smallKnob("OSC_SMP_ROOT", "ROOT");
        else if (eng == 3)
        {
            smallKnob("OSC_GR_SIZE", "SIZE");
            smallKnob("OSC_GR_DENSITY", "DENS");
            smallKnob("OSC_GR_SPRAY", "SPRAY");
        }
        // the filter
        const int ft = std::clamp(static_cast<int>(std::lround(u.get(idOf("OSC_FILT_TYPE")) * 3)), 0, 3);
        for (int i = 0; i < 4; ++i)
            choose(c, "OSC_FILT_TYPE", i, 4, 680 + static_cast<float>(i) * 54, ry + 52, 50, kFilter[i], i == ft);
        knob(c, "OSC_CUTOFF", 776, ry + 150, 44, "CUTOFF", true);
        knob(c, "OSC_RESO", 910, ry + 140, 28, "RESONANCE");
        knob(c, "OSC_HPF", 1010, ry + 108, 17, "HPF");
        knob(c, "OSC_KEY_TRACK", 1090, ry + 108, 17, "KEYTRK");
        knob(c, "OSC_FILT_ENV_AMT", 1170, ry + 108, 17, "ENV AMT");
        // envelopes
        const float by = 350;
        const char *amp[4] = {"OSC_AMP_ATK", "OSC_AMP_DEC", "OSC_AMP_SUS", "OSC_AMP_REL"};
        const char *flt[4] = {"OSC_FILT_ATK", "OSC_FILT_DEC", "OSC_FILT_SUS", "OSC_FILT_REL"};
        const char *adsr[4] = {"A", "D", "S", "R"};
        for (int i = 0; i < 4; ++i)
        {
            fader(c, amp[i], 14 + 70 + static_cast<float>(i) * 90, by + 52, 116, adsr[i]);
            fader(c, flt[i], 435 + 70 + static_cast<float>(i) * 90, by + 52, 116, adsr[i]);
        }
        // the tube
        knob(c, "GLB_TUBE_DRV", 936, by + 100, 24, "DRIVE");
        knob(c, "GLB_TUBE_ASYM", 1030, by + 106, 18, "ASYM");
        knob(c, "GLB_TUBE_MIX", 1120, by + 106, 18, "MIX");
        // the delay and the master
        const float fy = 580;
        knob(c, "GLB_DLY_L", 70, fy + 100, 24, "LEFT");
        knob(c, "GLB_DLY_R", 170, fy + 106, 18, "RIGHT");
        knob(c, "GLB_DLY_FB", 262, fy + 106, 18, "FEEDBACK");
        knob(c, "GLB_DLY_DMP", 354, fy + 106, 18, "DAMP");
        knob(c, "GLB_DLY_MIX", 446, fy + 106, 18, "MIX");
        choose(c, "GLB_DLY_PING", 1, 2, 540, fy + 96, 64, "PING-PONG", u.get(idOf("GLB_DLY_PING")) > 0.5f);
        knob(c, "GLB_VOLUME", 936, fy + 100, 24, "VOLUME");
        return c;
    }

    void panel(std::ostringstream &s, const panelkit::Theme &t, float x, float y, float w, float h)
    {
        (void)t;
        panelkit::svgPanel(s, x, y, w, h);
        panelkit::svgRule(s, x + 12, y + 31, w - 24);
    }


    // ── the displays ────────────────────────────────────────────────────────────────────────────────
    // ── the displays ────────────────────────────────────────────────────────────────────────────────────
    void showWave(Canvas &cv, float dx, float dy, float dw, float dh, int wave)
    {
        std::vector<SDL_FPoint> pts;
        for (int i = 0; i <= 240; ++i)
        {
            const float p = std::fmod(static_cast<float>(i) / 120.0f, 1.0f);
            float v = 0;
            switch (wave)
            {
            case 0: v = std::sin(2 * static_cast<float>(M_PI) * p); break;
            case 1: v = 2 * p - 1; break;
            case 2: v = p < 0.5f ? 1.0f : -1.0f; break;
            case 3: v = 1 - 4 * std::fabs(p - 0.5f); break;
            case 4: v = p < 0.15f ? 1.0f : -1.0f; break;
            default: v = std::sin(static_cast<float>(i) * 12.9898f) * 43758.5453f; v -= std::floor(v); v = v * 2 - 1; break;
            }
            pts.push_back({dx + 12 + static_cast<float>(i) * (dw - 24) / 240, dy + dh / 2 + 6 - (dh / 2 - 24) * v});
        }
        cv.lines(pts, cv.theme.accent, 1, 1.8f);
    }

    void showFrames(View &u, float dx, float dy, float dw, float dh, float pos)
    {
        Canvas &cv = *u.cv;
        const int nf = static_cast<int>(u.frames.size());
        if (nf == 0)
        {
            cv.say("NO WAVETABLE BANK FOUND", dx + dw / 2, dy + dh / 2, 9, cv.theme.accent, 0.6f, false, true, 2, Align::Centre);
            return;
        }
        const int cur = static_cast<int>(std::lround(pos * static_cast<float>(nf - 1)));
        const float fw = dw * 0.56f, amp = 16;
        for (int k = nf - 1; k >= 0; --k)
        {
            const float ox = dx + 12 + static_cast<float>(k) * (dw - fw - 24) / static_cast<float>(nf - 1);
            const float oy = dy + dh - 20 - static_cast<float>(k) * (dh - 50) / static_cast<float>(nf - 1);
            std::vector<SDL_FPoint> pts;
            for (size_t j = 0; j < u.frames[static_cast<size_t>(k)].size(); ++j)
                pts.push_back({ox + static_cast<float>(j) * fw / 63.0f, oy - amp * u.frames[static_cast<size_t>(k)][j]});
            const bool on = k == cur;
            cv.lines(pts, on ? cv.theme.accent : cv.theme.trace, on ? 1.0f : 0.25f + 0.45f * (1 - static_cast<float>(k) / static_cast<float>(nf)), on ? 1.8f : 0.8f);
        }
    }

    // the FM algorithms as the engine computes them: operators (column, row from the bottom), links
    struct Algo
    {
        const char *name;
        std::vector<std::tuple<char, float, int>> ops;
        std::vector<std::pair<char, char>> links;
    };
    const std::vector<Algo> &algos()
    {
        static const std::vector<Algo> a = {
            {"2OP", {{'C', 0, 0}, {'M', 0, 1}}, {{'M', 'C'}}},
            {"2OP+FB", {{'C', 0, 0}, {'M', 0, 1}}, {{'M', 'C'}}},
            {"4OP SER", {{'C', 0, 0}, {'M', 0, 1}, {'2', 0, 2}, {'3', 0, 3}, {'4', 0, 4}}, {{'4', '3'}, {'3', '2'}, {'2', 'M'}, {'M', 'C'}}},
            {"4OP PAR", {{'C', 0, 0}, {'M', -.6f, 1}, {'2', .6f, 1}, {'3', -.6f, 2}, {'4', .6f, 2}}, {{'3', 'M'}, {'4', '2'}, {'M', 'C'}, {'2', 'C'}}},
            {"4OP MIX", {{'C', 0, 0}, {'M', -.6f, 1}, {'3', -.6f, 2}, {'4', -.6f, 3}, {'2', .6f, 1}}, {{'4', '3'}, {'3', 'M'}, {'M', 'C'}, {'2', 'C'}}},
        };
        return a;
    }

    void showAlgo(Canvas &cv, float dx, float dy, float dw, float dh, int ai)
    {
        const Algo &a = algos()[static_cast<size_t>(std::clamp(ai, 0, 4))];
        int rows = 1;
        for (const auto &o : a.ops)
            rows = std::max(rows, std::get<2>(o) + 1);
        const float step = std::min(18.0f, (dh - 30) / static_cast<float>(rows)), box = std::min(14.0f, step * 0.8f);
        const float cx = dx + dw * 0.5f, bottom = dy + dh - 22;
        auto pos = [&](char n) {
            for (const auto &o : a.ops)
                if (std::get<0>(o) == n)
                    return SDL_FPoint{cx + std::get<1>(o) * box * 1.9f, bottom - static_cast<float>(std::get<2>(o)) * step};
            return SDL_FPoint{cx, bottom};
        };
        for (const auto &[f, t] : a.links)
        {
            const SDL_FPoint p = pos(f), q = pos(t);
            cv.lines({{p.x, p.y + box / 2}, {q.x, q.y - box / 2}}, cv.theme.accent, 0.8f, 1.3f);
        }
        for (const auto &o : a.ops)
        {
            const SDL_FPoint p = pos(std::get<0>(o));
            const bool carrier = std::get<0>(o) == 'C';
            cv.rect(p.x - box / 2, p.y - box / 2, box, box, cv.theme.display, 1, true);
            cv.rect(p.x - box / 2, p.y - box / 2, box, box, carrier ? cv.theme.accent : cv.theme.trace, 1, false);
            cv.say(std::string(1, std::get<0>(o)), p.x, p.y + box * 0.3f, box * 0.55f, carrier ? cv.theme.accent : cv.theme.traceHi, 1, true, true, 0, Align::Centre);
        }
    }

    void showGrains(Canvas &cv, float dx, float dy, float dw, float dh, float pos, float spray, float size)
    {
        const float mid = dy + dh / 2 + 8;
        std::vector<SDL_FPoint> w;
        for (int i = 0; i <= 300; ++i)
            w.push_back({dx + 10 + static_cast<float>(i) * (dw - 20) / 300, mid - 16 * std::sin(static_cast<float>(i) / 9) * std::sin(static_cast<float>(i) / 70 + 0.5f)});
        cv.lines(w, cv.theme.trace, 0.5f, 1.0f);
        const float cx = dx + 10 + pos * (dw - 20);
        uint32_t seed = 7;
        auto rnd = [&seed] { seed = seed * 1103515245u + 12345u; return static_cast<float>((seed >> 8) & 0xffff) / 65535.0f; };
        for (int k = 0; k < 26; ++k)
        {
            const float off = (rnd() - 0.5f) * (10 + spray * dw * 0.6f), wd = 8 + size * 80 + rnd() * 10, hh = 8 + rnd() * 26;
            std::vector<SDL_FPoint> g;
            for (int j = 0; j <= 10; ++j)
            {
                const float p = static_cast<float>(j) / 10;
                g.push_back({cx + off - wd / 2 + p * wd, mid + 2 - hh * 4 * p * (1 - p)});
            }
            cv.lines(g, cv.theme.traceHi, 0.35f + 0.5f * (1 - std::min(1.0f, std::fabs(off) / (dw * 0.3f))), 0.8f);
        }
        cv.rect(cx - 0.8f, dy + 22, 1.6f, dh - 30, cv.theme.accent, 1, true);
    }

    // a control's area (panel units): what a change of its value redraws

    // ── the sound: the engine standing alone, or the plugin through the link ────────────────────────────
    class OneEngine : public synth::panel::Engine
    {
    public:
        std::string bank, grain;               // standing alone: what the engine loads
        std::mutex listLock;
        std::vector<std::string> samples;      // the recordings, by index (the plugin's list when remote)
        std::atomic<bool> listChanged{false};

        bool start(const std::vector<float> &values, const synth::io::Settings &settings) override
        {
            std::vector<std::string> rec;
            {
                std::lock_guard<std::mutex> g(listLock);
                rec = samples;
            }
            return sound_.start(values.data(), bank, rec, grain, settings);
        }
        bool connect(const char *link) override
        {
            link_.onLine = [this](const std::string &line) {
                int id = 0, k = 0;
                float x = 0;
                if (std::sscanf(line.c_str(), "v %d %f", &id, &x) == 2 && id >= 0 && id < NPARAMS)
                    push(id, x);
                else if (std::sscanf(line.c_str(), "P %d", &k) == 1)
                    push(-1, static_cast<float>(k));
                else if (std::sscanf(line.c_str(), "L %d", &k) == 1 && k >= 0 && k < 64)
                {
                    const auto at = line.find(' ', 2);
                    std::lock_guard<std::mutex> g(listLock);
                    if (samples.size() <= static_cast<size_t>(k))
                        samples.resize(static_cast<size_t>(k) + 1);
                    samples[static_cast<size_t>(k)] = at == std::string::npos ? "" : line.substr(at + 1);
                    listChanged = true;
                }
            };
            return link_.connect(link);
        }
        bool remote() const override { return link_.linked(); }
        bool remoteAlive() const override { return link_.alive(); }
        bool pollRemote(int &id, float &value) override
        {
            std::lock_guard<std::mutex> g(inLock_);
            if (in_.empty())
                return false;
            id = in_.front().first, value = in_.front().second;
            in_.pop_front();
            return true;
        }
        void setParam(int id, float value) override
        {
            if (remote())
            {
                char b[64];
                std::snprintf(b, sizeof b, "p %d %.9g\n", id, static_cast<double>(value));
                link_.send(b);
            }
            else
                sound_.setParam(id, value);
        }
        void noteOn(int note, float velocity) override
        {
            if (!remote()) // in a host the host plays
                sound_.noteOn(note, velocity);
        }
        void noteOff(int note) override
        {
            if (!remote())
                sound_.noteOff(note);
        }
        bool pollCc(int &channel, int &cc, float &value) override { return !remote() && sound_.pollCc(channel, cc, value); }
        synth::io::MidiIn &midiIn() override { return sound_.midiIn(); }
        const synth::io::AudioOut &audioOut() const override { return sound_.audioOut(); }
        bool reopenAudio(const synth::io::Settings &settings) override { return sound_.reopenAudio(settings); }
        int sounding() const override { return remote() ? 0 : sound_.sounding(); }
        const char *profile() const { return sound_.profile(); }

    private:
        void push(int id, float v)
        {
            std::lock_guard<std::mutex> g(inLock_);
            in_.emplace_back(id, v);
        }
        synth::oneapp::Sound sound_;
        synth::panel::RemoteLink link_;
        std::mutex inLock_;
        std::deque<std::pair<int, float>> in_;
    };

    std::string presetsDir() { return synth::dataHome() + "/obssynthone/presets"; }

    // ── the product ─────────────────────────────────────────────────────────────────────────────────
    class One : public synth::panel::Product
    {
    public:
        One() : view_(engine_.listLock, engine_.samples)
        {
            namespace fs = std::filesystem;
            defaults_ = onep::defaults();
            const std::string bank = findBankFile();
            engine_.bank = bank;
            loadFrames(view_);
            // the recordings the sample engine can play (as the plugin lists them: the pcm/ ones, not the wavetables)
            std::string grain = "/usr/share/sounds/alsa/Rear_Center.wav";
            std::vector<std::string> rec;
            if (!bank.empty())
            {
                std::error_code ec;
                for (const auto &f : fs::directory_iterator(fs::path(bank).parent_path(), ec))
                {
                    const std::string n = f.path().filename().string();
                    if (f.path().extension() == ".wav" && n.rfind("bank_", 0) != 0 && n.rfind("cycle_", 0) != 0 && n.find("_wave_") == std::string::npos)
                        rec.push_back(f.path().string());
                }
                std::sort(rec.begin(), rec.end(), [](const std::string &a, const std::string &b) { return fs::path(a).filename() < fs::path(b).filename(); });
                if (rec.size() > 64)
                    rec.resize(64);
                for (const char *g : {"alsa-rear-center.wav", "vcsl-pipe-organ-quiet-C4.wav"})
                    if (const fs::path c = fs::path(bank).parent_path() / g; !fs::exists(grain, ec) && fs::is_regular_file(c, ec))
                        grain = c.string();
            }
            if (std::error_code ec; !fs::exists(grain, ec))
                grain.clear();
            engine_.grain = grain;
            std::lock_guard<std::mutex> g(engine_.listLock);
            engine_.samples = rec; // the plugin's list replaces it when remote
        }

        const char *id() const override { return "obssynthone"; }
        const char *title() const override { return "ObsSynth One"; }
        const char *barNumber() const override { return "ONE"; }

        int params() const override { return NPARAMS; }
        std::string paramName(int id) const override { return DEFS[id].name; }
        float paramDefault(int id) const override { return defaults_[static_cast<size_t>(id)]; }
        std::string paramText(int id, float v) const override { return shown(id, v); }
        int paramSteps(int id) const override { return rangeOf(id).steps; }

        std::vector<synth::panel::PresetEntry> presets() override
        {
            std::vector<synth::panel::PresetEntry> v;
            for (size_t i = 0; i < onep::presets().size(); ++i)
                v.push_back({onep::presets()[i].name, "", static_cast<int>(i)});
            std::vector<synth::panel::PresetEntry> mine;
            std::error_code ec;
            for (const auto &e : std::filesystem::directory_iterator(presetsDir(), ec))
                if (e.path().extension() == ".json")
                {
                    std::ifstream in(e.path());
                    try
                    {
                        nlohmann::json j;
                        in >> j;
                        mine.push_back({j.value("name", e.path().stem().string()), e.path().string(), -1});
                    }
                    catch (...)
                    {
                    }
                }
            std::sort(mine.begin(), mine.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
            v.insert(v.end(), mine.begin(), mine.end());
            return v;
        }
        bool loadPreset(const synth::panel::PresetEntry &e, std::vector<float> &values, std::string &name) override
        {
            auto indexOf = [this](const std::string &file) {
                std::lock_guard<std::mutex> g(engine_.listLock);
                for (size_t i = 0; i < engine_.samples.size(); ++i)
                    if (std::filesystem::path(engine_.samples[i]).filename() == file)
                        return static_cast<int>(i);
                return -1;
            };
            if (e.factory >= 0)
            {
                const Preset &p = onep::presets()[static_cast<size_t>(e.factory)];
                const Values v = fromPreset(p, [&](const char *f) { return indexOf(f); });
                values.assign(v.begin(), v.end());
                name = p.name;
                return true;
            }
            try // the person's: every parameter by name, in its own units; the recording by its file's name
            {
                std::ifstream in(e.path);
                nlohmann::json j;
                in >> j;
                values.assign(defaults_.begin(), defaults_.end());
                if (j.contains("params"))
                    for (auto it = j["params"].begin(); it != j["params"].end(); ++it)
                        if (const int i = idOf(it.key()); i >= 0 && DEFS[i].idx != X_SMP_SAMPLE)
                        {
                            const Range &rg = rangeOf(i);
                            values[static_cast<size_t>(i)] = std::clamp(toNorm(rg, std::clamp(it.value().get<float>(), std::min(rg.lo, rg.hi), std::max(rg.lo, rg.hi))), 0.0f, 1.0f);
                        }
                if (const int k = indexOf(j.value("sample", "")); k >= 0)
                    values[static_cast<size_t>(idOf("OSC_SMP_SAMPLE"))] = static_cast<float>(k) / 63.0f;
                name = j.value("name", "Untitled");
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
        bool savePreset(const std::string &name, const std::vector<float> &values, std::string &path) override
        {
            nlohmann::json j;
            j["name"] = name;
            nlohmann::json params = nlohmann::json::object();
            for (int i = 0; i < NPARAMS; ++i)
                if (DEFS[i].idx != X_SMP_SAMPLE)
                    params[DEFS[i].name] = toValue(rangeOf(i), values[static_cast<size_t>(i)]);
            j["params"] = params;
            {
                const int k = static_cast<int>(std::lround(values[static_cast<size_t>(idOf("OSC_SMP_SAMPLE"))] * 63));
                std::lock_guard<std::mutex> g(engine_.listLock);
                if (k >= 0 && static_cast<size_t>(k) < engine_.samples.size())
                    j["sample"] = std::filesystem::path(engine_.samples[static_cast<size_t>(k)]).filename().string();
            }
            std::string slug;
            for (char c : name)
                if (std::isalnum(static_cast<unsigned char>(c)))
                    slug += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                else if (!slug.empty() && slug.back() != '-')
                    slug += '-';
            while (!slug.empty() && slug.back() == '-')
                slug.pop_back();
            std::error_code ec;
            std::filesystem::create_directories(presetsDir(), ec);
            path = presetsDir() + "/" + (slug.empty() ? std::string("preset") : slug) + ".json";
            std::ofstream out(path);
            out << j.dump(2);
            return static_cast<bool>(out);
        }

        // Novation Launchkey: the encoders 21-28 the filter, the position, the envelope's ends, the effects; the
        // fader 79 (and 7) the volume; the track buttons the presets. Launchpad X: its arrows ‹ › step the presets
        // (the pads play: Note mode).
        std::vector<ccov::Map> controllers() const override
        {
            auto keys = [](const char *name, const char *match) {
                ccov::Map m;
                m.name = name;
                m.match = match;
                const std::pair<int, const char *> cc[] = {{21, "p.OSC_CUTOFF"}, {22, "p.OSC_RESO"}, {23, "p.OSC_FILT_ENV_AMT"}, {24, "p.OSC_WT_MORPH"},
                                                           {25, "p.OSC_AMP_ATK"}, {26, "p.OSC_AMP_REL"}, {27, "p.GLB_TUBE_MIX"}, {28, "p.GLB_DLY_MIX"},
                                                           {79, "p.GLB_VOLUME"}, {7, "p.GLB_VOLUME"}, {103, "@preset-"}, {102, "@preset+"},
                                                           {106, "@preset-"}, {107, "@preset+"}};
                for (const auto &[n, t] : cc)
                    m.cc.push_back({n, -1, t});
                return m;
            };
            ccov::Map pad;
            pad.name = "Launchpad X";
            pad.match = "Launchpad X";
            pad.cc = {{93, -1, "@preset-"}, {94, -1, "@preset+"}, {95, -1, "@overlay"}};
            return {keys("Launchkey 49", "Launchkey"), pad, keys("Generic", "")};
        }

        synth::panel::Engine &engine() override { return engine_; }

        std::string layoutKey(const std::vector<float> &values) const override
        {
            View &u = const_cast<View &>(view_);
            u.v = &values;
            return u.page == 1 ? std::string("mod") : "e" + std::to_string(u.engine());
        }
        std::vector<std::string> layoutKeys() const override { return {"e0", "e1", "e2", "e3", "e4", "mod"}; }

        std::vector<synth::panel::Control> layout(std::vector<float> &values) override
        {
            view_.v = &values;
            std::vector<synth::panel::Control> out;
            for (const Ctl &c : ::layout(view_, view_.engine(), view_.page))
                out.push_back(control(c, values));
            synth::panel::Control master; // the master's knob in the bar (the SDK's units)
            master.kind = synth::panel::Kind::Knob, master.x = 1562, master.y = 29, master.a = 15, master.param = idOf("GLB_VOLUME");
            out.push_back(master);
            return out;
        }

        std::string bodySvg(const panelkit::Theme &t, const std::string &key) override
        {
            const int page = key == "mod" ? 1 : 0, eng = key.size() > 1 && key[0] == 'e' ? key[1] - '0' : 0;
            std::vector<float> keep(defaults_.begin(), defaults_.end());
            const std::vector<float> *was = view_.v;
            view_.v = &keep;
            const std::vector<Ctl> ctls = ::layout(view_, eng, page);
            view_.v = was;
            std::ostringstream s;
            s << "<g transform='translate(" << OX << " " << OY << ") scale(" << Z << ")'>";
            if (page == 1)
            {
                panel(s, t, 14, 72, 622, 230);
                panel(s, t, 644, 72, 622, 230);
                panel(s, t, 14, 316, 1252, 454);
                for (int r = 0; r < 4; ++r)
                    for (float x : {282.0f, 672.0f})
                        panelkit::svgDisplay(s, t, x, 386 - 4 + static_cast<float>(r) * 90, 150, 26);
            }
            else
            {
                panel(s, t, 14, 72, 640, 268);
                panelkit::svgDisplay(s, t, 28, 152, 612, 92);
                panel(s, t, 664, 72, 602, 268);
                panel(s, t, 14, 350, 410, 220);
                panel(s, t, 435, 350, 410, 220);
                panel(s, t, 856, 350, 410, 220);
                panel(s, t, 14, 580, 832, 190);
                panel(s, t, 856, 580, 410, 190);
            }
            for (const Ctl &c : ctls)
                if (c.kind == Kind::Knob)
                    panelkit::svgKnob(s, t, c.x, c.y, c.a);
                else if (c.kind == Kind::Fader)
                    panelkit::svgFader(s, t, c.x, c.y, c.a);
            s << "</g>";
            panelkit::svgKnob(s, t, 1562, 29, 15); // the master, in the bar
            return s.str();
        }

        void drawBody(Canvas &cv, const std::vector<float> &values, const synth::panel::Clip &clip) override
        {
            View &u = view_;
            u.v = &values;
            u.clip = clip;
            u.cv = &cv;
            const panelkit::Theme &t = cv.theme;
            const int eng = u.engine();
            if (clip.touches(0, 0, synth::panel::W, synth::panel::BAR)) // the bar's part that is the One's: the pages, MASTER
            {
                cv.say("MAIN", 330, 35, 12, u.page == 0 ? t.ink : t.dim, 1, true, false, 2.5f, Align::Left);
                cv.say("MOD", 410, 35, 12, u.page == 1 ? t.ink : t.dim, 1, true, false, 2.5f, Align::Left);
                cv.rect(u.page == 0 ? 330.0f : 410.0f, 44, 40, 2, t.accent);
                cv.say("MASTER", 1534, 34, 10, t.ink, 1, true, false, 1.6f, Align::Right);
            }
            cv.zoom = Z, cv.offX = OX, cv.offY = OY; // the body, in its own units
        if (u.page == 1)
        {
            for (int i = 0; i < 2; ++i)
            {
                const float x0 = 14 + static_cast<float>(i) * 630;
                cv.say("LFO " + std::to_string(i + 1), x0 + 14, 72 + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
                cv.say(kLfoWaves[std::clamp(static_cast<int>(std::lround(u.get(idOf(i ? "LFO2_WAVE" : "LFO1_WAVE")) * 4)), 0, 4)], x0 + 622 - 14, 72 + 22, 10, t.dim, 1, false, false, 1.5f, Align::Right);
                cv.say("SHAPE", x0 + 200, 72 + 84, 10, t.ink, 1, true, false, 1.6f, Align::Left);
            }
            cv.say("ROUTES", 28, 316 + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
            cv.say("a source moves a destination of the oscillator; nothing runs until a route has a source and an amount", 1252, 316 + 22, 9, t.dim, 1, false, false, 1.2f, Align::Right);
            for (int r = 0; r < 4; ++r)
            {
                const float y = 386 + static_cast<float>(r) * 90;
                const std::string n = std::to_string(r + 1);
                const int sk = std::clamp(static_cast<int>(std::lround(u.get(idOf(("MOD" + n + "_SRC").c_str())) * 8)), 0, 8);
                const int dk = std::clamp(static_cast<int>(std::lround(u.get(idOf(("MOD" + n + "_DST").c_str())) * 7)), 0, 7);
                cv.say("ROUTE " + n, 28, y + 13, 10, t.ink, 1, true, false, 1.6f, Align::Left);
                cv.say("FROM", 250, y - 8, 9, t.dim, 1, false, false, 1.5f, Align::Left);
                cv.say(kModSources[sk], 357, y + 14, 11, sk == 0 ? t.dim : t.accent, 1, false, true, 0, Align::Centre);
                cv.say("TO", 640, y - 8, 9, t.dim, 1, false, false, 1.5f, Align::Left);
                cv.say(kModDests[dk], 747, y + 14, 11, t.accent, 1, false, true, 0, Align::Centre);
            }
        }
        else
        {
        // the panels' titles
        struct Title { const char *t; float x, y; const char *sub; float w; };
        for (const Title &p : {Title{"OSCILLATOR", 14, 72, kEngine[eng], 640}, Title{"FILTER", 664, 72, kFilter[std::clamp(static_cast<int>(std::lround(u.get(idOf("OSC_FILT_TYPE")) * 3)), 0, 3)], 602},
                               Title{"AMP ENV", 14, 350, "ADSR", 410}, Title{"FILTER ENV", 435, 350, "ADSR", 410}, Title{"DRIVE", 856, 350, "TUBE", 410},
                               Title{"DELAY", 14, 580, "STEREO", 832}, Title{"MASTER", 856, 580, "", 410}})
        {
            cv.say(p.t, p.x + 14, p.y + 22, 12, t.ink, 1, true, false, 2.4f, Align::Left);
            cv.say(p.sub, p.x + p.w - 14, p.y + 22, 10, t.dim, 1, false, false, 1.5f, Align::Right);
        }
        cv.say("ENGINE", 30, 72 + 46, 10, t.ink, 1, true, false, 1.6f, Align::Left);
        if (eng == 0)
            cv.say("WAVE", 290, 72 + 46, 10, t.ink, 1, true, false, 1.6f, Align::Left);
        cv.say("TYPE", 680, 72 + 46, 10, t.ink, 1, true, false, 1.6f, Align::Left);

        // the oscillator's display: the engine's picture
        const float dx = 28, dy = 152, dw = 612, dh = 92;
        const float pos = u.get(idOf("OSC_WT_MORPH"));
        if (u.touches(dx, dy, dw, dh))
        {
        cv.rect(dx + 1, dy + 1, dw - 2, dh - 2, t.accent, 0.18f, false);
        switch (eng)
        {
        case 0:
            cv.say("◀ " + std::string(kWave[std::clamp(static_cast<int>(std::lround(u.get(idOf("OSC_WAVE")) * 5)), 0, 5)]) + " ▶", dx + 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Left);
            showWave(cv, dx, dy, dw, dh, std::clamp(static_cast<int>(std::lround(u.get(idOf("OSC_WAVE")) * 5)), 0, 5));
            break;
        case 1:
        {
            showFrames(u, dx, dy, dw, dh, pos);
            const int total = std::max(1, u.bankFrames);
            cv.say("◀ MORPH ▶", dx + 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Left);
            char b[32];
            std::snprintf(b, sizeof b, "%02d / %d", static_cast<int>(std::lround(pos * static_cast<float>(total - 1))) + 1, total);
            cv.say(b, dx + dw - 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Right);
            break;
        }
        case 2:
        {
            const int ai = static_cast<int>(std::lround(u.get(idOf("OSC_FM_ALGORITHM")) * 4));
            cv.say("◀ ALGO " + std::to_string(ai + 1) + " ▶", dx + 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Left);
            cv.say(algos()[static_cast<size_t>(std::clamp(ai, 0, 4))].name, dx + dw - 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Right);
            showAlgo(cv, dx, dy, dw, dh, ai);
            break;
        }
        case 3:
            cv.say("◀ GRAINS ▶", dx + 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Left);
            showGrains(cv, dx, dy, dw, dh, pos, u.get(idOf("OSC_GR_SPRAY")), u.get(idOf("OSC_GR_SIZE")));
            break;
        default:
        {
            std::string path;
            const int si = static_cast<int>(std::lround(u.get(idOf("OSC_SMP_SAMPLE")) * 63));
            {
                std::lock_guard<std::mutex> g(u.listLock);
                if (si >= 0 && static_cast<size_t>(si) < u.samples.size())
                    path = u.samples[static_cast<size_t>(si)];
            }
            const std::string nm = path.empty() ? "NO SAMPLE" : std::filesystem::path(path).filename().string();
            cv.say("◀ " + nm + " ▶", dx + 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Left);
            cv.say(u.get(idOf("OSC_SMP_LOOP")) > 0.5f ? "LOOP" : "ONE SHOT", dx + dw - 8, dy + 14, 9, t.accent, 0.6f, false, true, 2, Align::Right);
            if (!path.empty())
                if (const std::vector<float> *cols = peaksOf(u, path); cols && !cols->empty())
                {
                    const float mid = dy + dh / 2 + 8;
                    for (size_t i = 0; i < cols->size(); ++i)
                    {
                        const float x = dx + 10 + static_cast<float>(i) * (dw - 20) / static_cast<float>(cols->size());
                        const float a = (*cols)[i] * (dh / 2 - 22);
                        cv.rect(x, mid - a, std::max(1.0f, (dw - 20) / 400.0f), a * 2 + 1, t.trace, 0.8f, true);
                    }
                }
            const float sx = dx + 10 + pos * (dw - 20);
            if (u.get(idOf("OSC_SMP_LOOP")) > 0.5f)
                cv.rect(sx, dy + 20, dx + dw - 10 - sx, dh - 28, t.accent, 0.10f, true);
            cv.rect(sx - 0.8f, dy + 20, 1.6f, dh - 28, t.accent, 1, true);
            break;
        }
        }
        }

        // the effects' notes: dry means not running
        if (u.get(idOf("GLB_DLY_MIX")) <= 0.0f)
            cv.say("DRY: NOT RUNNING", 830, 580 + 180, 9, t.dim, 0.8f, false, true, 1.5f, Align::Right);
        if (u.get(idOf("GLB_TUBE_MIX")) <= 0.0f)
            cv.say("DRY: NOT RUNNING", 1252, 350 + 210, 9, t.dim, 0.8f, false, true, 1.5f, Align::Right);
        }
            cv.zoom = 1, cv.offX = cv.offY = 0;
        }

        std::vector<SDL_FRect> damageOf(int id, const std::vector<float> &) override
        {
            const std::string n = DEFS[id].name;
            if (n == "OSC_FILT_TYPE" || n == "GLB_DLY_MIX" || n == "GLB_TUBE_MIX" || n.find("_SRC") != std::string::npos || n.find("_DST") != std::string::npos ||
                n.find("_WAVE") != std::string::npos)
                return {{0, 0, synth::panel::W, synth::panel::H}}; // drawn in a title, a note, a route's name
            if (n == "OSC_WT_MORPH" || n == "OSC_FM_ALGORITHM" || n == "OSC_GR_SPRAY" || n == "OSC_GR_SIZE" || n.rfind("OSC_SMP_", 0) == 0)
                return {{28 * Z + OX, 152 * Z + OY, 612 * Z, 92 * Z}}; // the display
            return {};
        }
        bool clickBar(float x, float, std::vector<float> &) override
        {
            const int to = x >= 330 && x < 390 ? 0 : x >= 410 && x < 470 ? 1 : -1;
            if (to < 0 || to == view_.page)
                return false;
            view_.page = to;
            return true;
        }
        bool wantsWhole() override { return engine_.listChanged.exchange(false); }
        std::string status() override
        {
            return (engine_.remote() ? std::string("PLUGIN") : std::to_string(engine_.sounding()) + " VOICES · " + engine_.profile()) + " · " + kEngine[view_.engine()];
        }

    private:
        // a control of the body, in the SDK's units; a button's click does what its mode says
        synth::panel::Control control(const Ctl &c, std::vector<float> &values)
        {
            synth::panel::Control o;
            o.param = c.id;
            o.label = c.label;
            o.on = c.on;
            o.text = Z;
            o.x = c.x * Z + OX, o.y = c.y * Z + OY;
            switch (c.kind)
            {
            case Kind::Knob:
                o.kind = synth::panel::Kind::Knob, o.a = c.a * Z, o.travel = 160 * Z;
                break;
            case Kind::Fader:
                o.kind = synth::panel::Kind::Fader, o.a = c.a * Z, o.travel = c.a * Z;
                break;
            case Kind::Button:
            {
                o.kind = synth::panel::Kind::Button, o.a = c.a * Z, o.b = c.b * Z;
                const bool lfo = c.label.empty() && c.mode == 0; // an LFO's shape: its name, lit when chosen
                if (lfo)
                {
                    o.label = kLfoWaves[std::clamp(c.choice, 0, 4)];
                    o.on = static_cast<int>(std::lround(values[static_cast<size_t>(c.id)] * 4)) == c.choice;
                }
                const int id = c.id, choice = c.choice, mode = c.mode;
                o.click = [this, &values, id, choice, mode](int) {
                    float &v = values[static_cast<size_t>(id)];
                    const int steps = rangeOf(id).steps;
                    if (mode == 2)
                        v = v > 0.5f ? 0.0f : 1.0f;
                    else if (mode == 1) // a step through a list (the recordings: the plugin's or this one's)
                    {
                        const bool sample = DEFS[id].idx == X_SMP_SAMPLE;
                        size_t n = static_cast<size_t>(std::max(1, steps));
                        if (sample)
                        {
                            std::lock_guard<std::mutex> g(engine_.listLock);
                            n = engine_.samples.size();
                        }
                        const float span = sample ? 63.0f : static_cast<float>(std::max(1, steps - 1));
                        if (n > 0)
                            v = static_cast<float>(std::clamp(static_cast<int>(std::lround(v * span)) + choice, 0, static_cast<int>(n) - 1)) / span;
                    }
                    else
                        v = steps > 1 ? static_cast<float>(choice) / static_cast<float>(steps - 1) : static_cast<float>(choice);
                };
                break;
            }
            }
            return o;
        }

        OneEngine engine_;
        View view_;
        Values defaults_{};
    };
} // namespace

int main(int argc, char **argv)
{
    One one;
    return synth::panel::run(one, argc, argv);
}
