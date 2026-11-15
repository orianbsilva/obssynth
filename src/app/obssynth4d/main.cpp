// obssynth4d: the ObsSynth4D front panel drawn as hardware (docs/mockups), a product of the panels' SDK
// (common/panel_app.hpp: the window, the top bar, SETUP, the CC overlay, the presets, the drawing).
//
// Two pages: MAIN (4 oscillators, each STD, WT, FM or GR; the filter and envelopes of the chosen oscillator; LFO;
// mixer, tube, delay, master filter; XY and scope) and MOD MATRIX (sources down, destinations across; a cell is a
// route, its amount; the chosen route on the left, every route under it).
//   knob, fader: drag (Shift: fine), wheel, double click: default.  Switches, LEDs: click (a switch: the wheel steps
//   it).  A matrix cell: click adds the route, drag and wheel its amount, double click mutes, right click removes.
//   The rings by LFO, ENV, XY: drag one onto a knob (a route); Ctrl + wheel on a knob: its modulation; right click
//   on a knob, or its halo: its routes in the matrix.  The preset's name: the list.  Tab: the other page.
//   --page matrix: open on the mod matrix. --grain FILE.wav: the recording for the granular engine. --preset NAME or
//   NUMBER: start with that preset. --profile lite|full: the engine's. OBS_PANEL_FLIP=ms flips the page by itself,
//   each flip timed (stderr); OBS_PANEL_DEBUG=1 the audio thread's line each second. The computer keyboard plays
//   (Z..M from C3, Q..U the octave up); MIDI comes in on ObsSynth4D:input (gridseq joins it by itself).
//   F2 (or CC in the top bar): the CC overlay, each control's CC from the controller's map (controllers.hpp); a click
//   there learns, a right click forgets; Shift F2: the next map.  --cc: start with it shown.
//   SETUP (top bar): the audio output, the MIDI inputs, the channel, the CC map (common/setup_page.hpp).  --setup: open on it.
//   --remote FD: the editor of ObsSynth4D.clap/.vst3 (sound.cpp has the link); in a host, MAIN leaves PLAY out.
//
// Threads: this one draws and reads the input; sound.cpp's queues carry every change to the audio thread, the
// engine's only writer.
#include "../common/editor_log.hpp"
#include "../common/panel_app.hpp"
#include "../common/sample_find.hpp"
#include "../../vendor/json.hpp"
#include "controllers.hpp"
#include "params.hpp"
#include "parts.hpp"
#include "presets.hpp"
#include "sound.hpp"
#include "synth/core/grain_engine.hpp"
#include "../../products/obs_synth/obs_synth_mod_source.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace obs;
using panelkit::Canvas;
using panelkit::Centre;
using panelkit::Left;
using panelkit::Right;
using synth::panel::Control;
using Kind = synth::panel::Kind;

namespace
{
    // ── the wavetable a WT oscillator shows (the bank's frames, silent ones at the end left out) ────────
    // ── the wavetable a WT oscillator shows (the bank's frames, silent ones at the end left out) ────────
    struct Table
    {
        std::vector<std::vector<float>> frames; // 128 points each
        std::string name;
    };

    Table loadTable(const std::string &path)
    {
        Table t;
        std::ifstream f(path, std::ios::binary);
        std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
        if (b.size() < 44)
            return t;
        uint16_t fmt, bits;
        std::memcpy(&fmt, &b[20], 2);
        std::memcpy(&bits, &b[34], 2);
        size_t i = 12;
        while (i + 8 <= b.size() && std::memcmp(&b[i], "data", 4) != 0)
        {
            uint32_t sz;
            std::memcpy(&sz, &b[i + 4], 4);
            i += 8 + sz;
        }
        if (i + 8 > b.size())
            return t;
        uint32_t n;
        std::memcpy(&n, &b[i + 4], 4);
        std::vector<float> s;
        const char *d = &b[i + 8];
        if (fmt == 3 || bits == 32)
            for (uint32_t k = 0; k + 4 <= n; k += 4)
            {
                float v;
                std::memcpy(&v, d + k, 4);
                s.push_back(v);
            }
        else
            for (uint32_t k = 0; k + 2 <= n; k += 2)
            {
                int16_t v;
                std::memcpy(&v, d + k, 2);
                s.push_back(static_cast<float>(v) / 32768.0f);
            }
        size_t total = s.size() / 1024;
        while (total > 1)
        {
            float m = 0;
            for (size_t k = (total - 1) * 1024; k < total * 1024; ++k)
                m = std::max(m, std::fabs(s[k]));
            if (m > 1e-6f)
                break;
            --total;
        }
        for (size_t fi = 0; fi < total; ++fi)
        {
            std::vector<float> fr(128);
            for (size_t j = 0; j < 128; ++j)
                fr[j] = s[fi * 1024 + j * 8];
            t.frames.push_back(std::move(fr));
        }
        t.name = std::filesystem::path(path).stem().string();
        if (t.name.rfind("bank_", 0) == 0)
            t.name = t.name.substr(5);
        t.name = t.name.substr(0, t.name.find('_'));
        for (auto &c : t.name)
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return t;
    }

    // ── the mod matrix's choices ─────────────────────────────────────────────────────────────────────
    struct Choice
    {
        int id;
        std::string name;
    };

    std::vector<Choice> modSources()
    {
        std::vector<Choice> v;
        for (int i = 0; i < synth::NUM_OBS_SYNTH_SOURCES; ++i)
        {
            std::string n = synth::obsSynthSrcToString(static_cast<synth::ObsSynthSrc>(i));
            for (auto &ch : n)
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            v.push_back({i, n});
        }
        return v;
    }

    // a route: a source, a destination (the engine's ids), an amount 0..1 bipolar around 0.5 (engine -2..+2)
    struct Route
    {
        int src = 0, dst = -1; // dst -1: the slot is free
        float amount = 0.5f;
        bool on = false;
    };
    constexpr int kSlots = 32; // the engine's MAX_MOD_SLOTS

    // the matrix: sources down, destinations across; a page per oscillator (its columns follow its engine)
    // and one for the rest
    struct MxCol
    {
        int dst;
        std::string label, group;
    };
    const char *kMxPages[] = {"OSC 1", "OSC 2", "OSC 3", "OSC 4", "GLOBAL"};
    const char *kOscDst[] = {"PITCH", "FINE", "WAVE", "LEVEL", "PAN", "PWM", "FM INDEX", "CUTOFF", "RESO", "F.ENV AMT",
                             "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "FX MIX", "FX P1", "FX P2"};

    // only what the engine applies (synth_engine.hpp applyModMatrixToChains, obs_synth_4d.hpp): the rest of
    // ObsSynthDst (wave, PWM, the envelopes' times, FX, mixer, MFX) is named there but nothing reads it yet
    std::vector<MxCol> mxCols(int page, int part)
    {
        std::vector<MxCol> v;
        if (page < 4)
        {
            const int b = page * 17;
            v.push_back({b + 0, "PITCH", "OSC"});
            v.push_back({b + 1, "FINE", "OSC"});
            if (part == 1)
                v.push_back({68 + page, "MORPH", "OSC"});
            else if (part == 3)
                v.push_back({68 + page, "GRAIN POS", "OSC"});
            else if (part == 4)
                v.push_back({68 + page, "DETUNE", "OSC"});
            else if (part == 2)
                v.push_back({b + 6, "FM INDEX", "OSC"});
            v.push_back({b + 3, "LEVEL", "MIX"});
            v.push_back({b + 4, "PAN", "MIX"});
            v.push_back({b + 7, "CUTOFF", "FILTER"});
            v.push_back({b + 8, "RESO", "FILTER"});
            v.push_back({b + 9, "ENV AMT", "FILTER"});
            return v;
        }
        for (int l = 0; l < 4; ++l)
            v.push_back({230 + l, std::to_string(l + 1), "LFO RATE"});
        for (int l = 0; l < 4; ++l)
            v.push_back({234 + l, std::to_string(l + 1), "LFO DEPTH"});
        for (int l = 0; l < 4; ++l)
            v.push_back({200 + l, std::to_string(l + 1), "MIX LEVEL"});
        for (int l = 0; l < 4; ++l)
            v.push_back({204 + l, std::to_string(l + 1), "MIX PAN"});
        v.push_back({224, "VOLUME", "MASTER"});
        v.push_back({225, "PAN", "MASTER"});
        v.push_back({226, "DRIVE", "TUBE"});
        v.push_back({227, "ASYM", "TUBE"});
        v.push_back({228, "CHAOS", "TUBE"});
        return v;
    }

    bool wired(int dst)
    {
        if (dst < 68)
        {
            const int o = dst % 17;
            return o == 0 || o == 1 || o == 3 || o == 4 || o == 6 || o == 7 || o == 8 || o == 9;
        }
        return dst < 72 || (dst >= 200 && dst <= 207) || dst == 224 || dst == 225 || (dst >= 226 && dst <= 228) || (dst >= 230 && dst <= 237);
    }

    // a destination's value in its control's units once the matrix adds mod (engine units, the routes' sum of
    // source x amount), as the engine does it
    float modded(int dst, float v, float mod)
    {
        if (dst < 68)
        {
            switch (dst % 17)
            {
            case 0: return v + mod * 12;   // semitones
            case 7: return v * std::exp2(mod * 4); // Hz, in octaves: ±4 per unit
            case 6: return v + mod * 10;   // FM index
            default: return v + mod;        // fine (semitone), level, pan, reso, env amount
            }
        }
        if (dst == 226)
            return v + mod * 4;
        if (dst == 227 || dst == 228)
            return v + mod * 0.5f;
        if (dst >= 230 && dst <= 233)
            return v * std::exp2(mod * 3);
        return v + mod; // wavetable position (the whole bank), volume
    }

    std::string destName(int id)
    {
        if (id < 68)
            return "OSC" + std::to_string(id / 17 + 1) + " " + kOscDst[id % 17];
        if (id < 72)
            return "OSC" + std::to_string(id - 67) + " WT MORPH";
        for (const MxCol &c : mxCols(4, 0))
            if (c.dst == id)
                return c.group + " " + c.label;
        return "?";
    }

    // the sources' groups (a gap between them down the matrix) and which swing both ways
    int srcGroup(int id)
    {
        const int starts[] = {0, 4, 8, 12, 15, 17, 22, 24};
        int g = 0;
        for (int k = 0; k < 8; ++k)
            if (id >= starts[k])
                g = k;
        return g;
    }
    bool srcBipolar(int id) { return id < 4 || id == 20; }

    const char *kSrcGroups[] = {"LFO", "FILTER ENV", "AMP ENV", "MOD ENV", "XY PAD", "MIDI", "NOTE", "MIDI CC"};
    constexpr float MX0 = 420, MX1 = 1572; // the grid, across

    // ── the panel ────────────────────────────────────────────────────────────────────────────────────
    constexpr float W = 1600, H = 900;
    constexpr float HOST_CROP = 242; // in a host, MAIN: the PLAY column (and the name above it) left out (MOD MATRIX keeps its ROUTE)
    const char *kPart[] = {"STD", "WT", "FM", "GR", "SUPER"}; // the ENGINE switch's places (parts.hpp)
    const char *kAlgo[] = {"2OP", "2OP+FB", "4OP SER", "4OP PAR", "4OP MIX"};

    // where a control's value lives (normalized 0..1)
    // where a control's value lives (normalized 0..1)
    struct Bind
    {
        enum Space { OscSel, OscN, Glb, LfoSel, Cell, RouteAmt, None } space = None;
        int index = 0; // oscillator, route, or a cell's source (param: its destination)
        int param = 0;
    };

    struct State
    {
        float osc[4][OSC_PARAM_COUNT]{};
        float glb[GLB_PARAM_COUNT]{};
        int selOsc = 0, selLfo = 0, page = 0;
        int mxPage = 0, selRoute = 0; // the matrix's page; the route the left column shows
        std::array<bool, 8> grpOpen{true, false, false, false, false, false, false, false}; // source groups shown (and those in use)
        std::array<Route, kSlots> routes{};
        // vector synthesis: on, the path looping, the path (t s, x, y)
        bool vecOn = false, vecLoop = false;
        int vecCount = 0;
        std::array<std::array<float, 3>, 16> vecPath{};
        std::array<std::string, 4> grainFile; // each granular oscillator's recording (samples-folder name or path)
    };

    // the panel's own: the state, what it shows besides (the tables, the recordings), what the pointer is doing
    struct Ui
    {
        State st;
        std::array<Table, 4> tables;
        std::vector<Choice> srcs = modSources();
        SDL_FPoint hover{-1, -1}; // the pointer, in panel units
        std::array<std::string, 4> grainName;           // each granular oscillator's recording, as loaded, and its
        std::array<std::vector<float>, 4> grainPeaks;   // shape (a column's peak each)
        std::vector<std::string> samples;               // the samples folder
        int modDrag = -1;         // a source being dragged onto a knob (MAIN), or none
        bool vecRec = false, recording = false; // REC armed; a path being drawn on the pad
        uint64_t recStart = 0, recLast = 0;
        std::vector<std::array<float, 3>> recPts;
        int flashDst = -1;        // a destination the matrix shows lit, a moment (from a knob's halo)
        uint64_t flashUntil = 0;
        uint64_t madeAt = 0;      // a route a cell made just now: the next click there is not a second one
        // the presets (the list shows them), the one shown, what a preset sets beyond the controls (kept for saving)
        std::vector<PresetEntry> presets;
        int presetAt = -1;
        PresetData extras{};
        State loaded; // as the preset gave it, to tell a change
        bool picking = false; // the presets' list open
        ccov::Overlay *ov = nullptr;   // the SDK's (controllers.hpp's maps)
        synth::Sound *sound = nullptr; // for the overlay's targets the panel has no knob for (mod sources, chaos)
    };

    int slotAt(const State &s, int src, int dst)
    {
        for (size_t i = 0; i < s.routes.size(); ++i)
            if (s.routes[i].dst == dst && s.routes[i].src == src)
                return static_cast<int>(i);
        return -1;
    }

    // the matrix, down: a head per source group, then its sources when open (a group with routes is always open);
    // the rows share what height there is
    struct MxRow
    {
        int src = -1, group = 0; // src -1: the group's head
        float y = 0, h = 0;
        bool open = true;
    };

    std::vector<MxRow> mxRows(const State &s, int nsrc)
    {
        bool used[8]{};
        for (const Route &rt : s.routes)
            if (rt.dst >= 0)
                used[srcGroup(rt.src)] = true;
        int rows = 0;
        for (int i = 0; i < nsrc; ++i)
            rows += s.grpOpen[static_cast<size_t>(srcGroup(i))] || used[srcGroup(i)];
        const float top = 168, bottom = 858, hh = 17;
        const float rh = rows > 0 ? std::min(26.0f, std::floor((bottom - top - 8 * hh) / static_cast<float>(rows))) : 26;
        std::vector<MxRow> v;
        float y = top;
        int g = -1;
        for (int i = 0; i < nsrc; ++i)
        {
            const int gi = srcGroup(i);
            const bool open = s.grpOpen[static_cast<size_t>(gi)] || used[gi];
            if (gi != g)
            {
                g = gi;
                v.push_back({-1, gi, y, hh, open});
                y += hh;
            }
            if (open)
            {
                v.push_back({i, gi, y, rh, true});
                y += rh;
            }
        }
        return v;
    }

    int pageOf(int dst) { return dst >= 200 ? 4 : dst >= 68 ? dst - 68 : dst / 17; }

    // where a source lives on MAIN: a click on its name in the matrix goes there
    void showSource(State &s, int src)
    {
        if (src < 4)
            s.selLfo = src;
        else if (src < 12)
            s.selOsc = (src - 4) % 4;
        else if (src != 15 && src != 16)
            return;
        s.page = 0;
    }

    int partOf(const State &s, int o) { return static_cast<int>(std::lround(toValue(OSC_RANGES[OSC_PART_TYPE], s.osc[o][OSC_PART_TYPE]))); }

    float &value(State &s, const Bind &b)
    {
        static float none = 0;
        switch (b.space)
        {
        case Bind::OscSel: return s.osc[s.selOsc][b.param];
        case Bind::OscN: return s.osc[b.index][b.param];
        case Bind::Glb: return s.glb[b.param];
        case Bind::LfoSel: return s.glb[b.param + s.selLfo];
        case Bind::RouteAmt: return s.routes[static_cast<size_t>(b.index)].amount;
        case Bind::Cell:
        {
            const int k = slotAt(s, b.index, b.param);
            return k >= 0 ? s.routes[static_cast<size_t>(k)].amount : none;
        }
        default: return none;
        }
    }

    const Range &rangeOf(const State &s, const Bind &b)
    {
        static const Range unit{0, 1, 0.5f, false, 0};
        switch (b.space)
        {
        case Bind::OscSel:
        case Bind::OscN: return OSC_RANGES[b.param];
        case Bind::Glb: return GLB_RANGES[b.param];
        case Bind::LfoSel: return GLB_RANGES[b.param + s.selLfo];
        default: return unit;
        }
    }

    std::string fmt(const char *f, double v)
    {
        char b[32];
        std::snprintf(b, sizeof b, f, v);
        return b;
    }

    std::string hz(float v) { return v >= 1000 ? fmt("%.1f kHz", static_cast<double>(v) / 1000) : fmt("%.0f Hz", static_cast<double>(v)); }
    std::string sec(float v) { return v >= 1 ? fmt("%.2f s", static_cast<double>(v)) : fmt("%.0f ms", static_cast<double>(v) * 1000); }
    std::string pct(float v) { return fmt("%.0f%%", static_cast<double>(v) * 100); }
    std::string two(float v) { return fmt("%.2f", static_cast<double>(v)); }
    std::string semis(float v) { return std::fabs(v) < 0.5f ? std::string("0") : fmt("%+.0f", static_cast<double>(v)); }
    // SUPER's shape from the wave knob: SAW, SQR, TRI or SIN (PULSE plays as SQR, NOISE as SAW: super_engine.hpp)
    int superShape(int wave) { return wave == 0 ? 3 : wave == 2 || wave == 4 ? 1 : wave == 3 ? 2 : 0; }
    std::string superWave(float v)
    {
        static const char *n[] = {"SAW", "SQR", "TRI", "SIN"};
        return n[superShape(static_cast<int>(std::lround(v)))];
    }
    std::string panName(float v) { return std::fabs(v) < 0.01f ? std::string("C") : fmt(v < 0 ? "L%.0f" : "R%.0f", std::fabs(static_cast<double>(v)) * 100); }

    // ── the CC overlay: a control's names, and what a name moves ─────────────────────────────────────
    // ── the CC overlay: a control's names, and what a name moves ─────────────────────────────────────
    std::string ccId(const Bind &b)
    {
        switch (b.space)
        {
        case Bind::OscSel: return std::string("osc.sel.") + kOscIds[b.param];
        case Bind::OscN: return "osc." + std::to_string(b.index + 1) + "." + kOscIds[b.param];
        case Bind::Glb: return std::string("glb.") + kGlbIds[b.param];
        case Bind::LfoSel: return b.param == GLB_LFO1_RATE ? "lfo.sel.RATE" : b.param == GLB_LFO1_WAVE ? "lfo.sel.WAVE" : "";
        default: return "";
        }
    }

    // its own name first (what learn binds), then the others that are it now (the chosen oscillator, LFO)
    std::vector<std::string> ccAliases(const State &s, const std::string &id)
    {
        std::vector<std::string> v{id};
        if (id.rfind("osc.", 0) == 0)
        {
            const auto dot = id.find('.', 4);
            if (dot == std::string::npos)
                return v;
            const std::string which = id.substr(4, dot - 4), p = id.substr(dot + 1);
            if (which == "all")
                return v;
            if (which == "sel")
                v.push_back("osc." + std::to_string(s.selOsc + 1) + "." + p);
            else if (std::atoi(which.c_str()) - 1 == s.selOsc)
                v.push_back("osc.sel." + p);
            v.push_back("osc.all." + p);
        }
        else if (id == "lfo.sel.RATE" || id == "lfo.sel.WAVE")
            v.push_back(std::string("glb.") + kGlbIds[(id == "lfo.sel.RATE" ? GLB_LFO1_RATE : GLB_LFO1_WAVE) + s.selLfo]);
        else if (id.rfind("glb.LFO", 0) == 0 && id.size() == 13 && id[7] - '1' == s.selLfo) // glb.LFO2_RATE
            v.push_back(id.substr(9) == "RATE" ? "lfo.sel.RATE" : "lfo.sel.WAVE");
        return v;
    }

    std::string spaced(std::string id)
    {
        for (auto &ch : id)
            if (ch == '_')
                ch = ' ';
        return id;
    }

    ccov::Target ccResolve(Ui &u, const std::string &id)
    {
        State &s = u.st;
        ccov::Target t; // (@preset+ @preset- @overlay: the SDK's)
        if (id == "@osc+" || id == "@osc-")
            t.press = [&u, up = id == "@osc+"] { u.st.selOsc = (u.st.selOsc + (up ? 1 : 3)) % 4; if (u.ov) u.ov->retarget(); };
        else if (id == "@lfo+" || id == "@lfo-")
            t.press = [&u, up = id == "@lfo+"] { u.st.selLfo = (u.st.selLfo + (up ? 1 : 3)) % 4; if (u.ov) u.ov->retarget(); };
        else if (id == "@page")
            t.press = [&s] { s.page = 1 - s.page; };
        else if (id.rfind("mod.", 0) == 0 && u.sound) // the matrix's sources: the wheel, CC A..D
        {
            const std::string m = id.substr(4);
            const int cc = m == "WHEEL" ? 1 : m == "EXPRESSION" ? 11 : m.size() == 4 && m.rfind("CC_", 0) == 0 && m[3] >= 'A' && m[3] <= 'D' ? 16 + (m[3] - 'A') : -1;
            if (cc >= 0)
            {
                t.set = [&u, cc](float v) { u.sound->control(cc, v); };
                t.label = "MOD " + spaced(m);
            }
            return t;
        }
        else if (id == "extra.TUBE_CHAOS" && u.sound) // a preset's, no knob on the panel
        {
            t.v = &u.extras.tubeChaos;
            t.set = [&u](float v) { u.extras.tubeChaos = v; u.sound->setExtra(13, v); };
            t.label = "TUBE CHAOS";
            return t;
        }
        Bind b;
        const auto find = [](const char *const *names, int n, const std::string &p) { for (int k = 0; k < n; ++k) if (p == names[k]) return k; return -1; };
        if (id.rfind("osc.", 0) == 0)
        {
            const auto dot = id.find('.', 4);
            if (dot == std::string::npos)
                return t;
            const std::string which = id.substr(4, dot - 4);
            const int p = find(kOscIds, OSC_PARAM_COUNT, id.substr(dot + 1));
            if (which == "all" && p >= 0) // the four at once (v1's cutoff, resonance, attack, release)
            {
                t.v = &s.osc[0][p];
                t.set = [&s, p](float v) { for (auto &o : s.osc) o[p] = v; };
                t.label = "ALL OSC " + spaced(kOscIds[p]);
                return t;
            }
            const int o = which == "sel" ? s.selOsc : std::atoi(which.c_str()) - 1;
            if (p < 0 || o < 0 || o > 3)
                return t;
            b = {Bind::OscN, o, p};
            t.label = "OSC " + std::to_string(o + 1) + " " + spaced(kOscIds[p]);
        }
        else if (id.rfind("glb.", 0) == 0)
        {
            const int p = find(kGlbIds, GLB_PARAM_COUNT, id.substr(4));
            if (p < 0)
                return t;
            b = {Bind::Glb, 0, p};
            t.label = spaced(kGlbIds[p]);
        }
        else if (id == "lfo.sel.RATE" || id == "lfo.sel.WAVE")
        {
            b = {Bind::Glb, 0, (id == "lfo.sel.RATE" ? GLB_LFO1_RATE : GLB_LFO1_WAVE) + s.selLfo};
            t.label = spaced(kGlbIds[b.param]);
        }
        else
            return t;
        t.v = &value(s, b); // the state is normalized: 0..1
        return t;
    }

    // a matrix cell pressed: the right button removes its route; the left adds it (or chooses it; a second click in
    // a row mutes it), then drags its amount (false: the SDK drags)
    bool cellPress(Ui &u, int src, int dst, int button, bool twice)
    {
        State &s = u.st;
        int slot = slotAt(s, src, dst);
        if (button == SDL_BUTTON_RIGHT)
        {
            if (slot >= 0)
                s.routes[static_cast<size_t>(slot)] = Route{};
            return true;
        }
        if (button != SDL_BUTTON_LEFT)
            return true;
        const uint64_t now = SDL_GetTicks();
        if (slot < 0)
        {
            for (size_t i = 0; i < s.routes.size() && slot < 0; ++i)
                if (s.routes[i].dst < 0)
                {
                    s.routes[i] = {src, dst, 0.625f, true};
                    slot = static_cast<int>(i);
                }
            if (slot < 0) // all the engine's slots in use
                return true;
            u.madeAt = now; // a new route: the next click is not a double one
        }
        else if (twice && now - u.madeAt >= 350)
            s.routes[static_cast<size_t>(slot)].on = !s.routes[static_cast<size_t>(slot)].on;
        s.selRoute = slot;
        return false;
    }

    // the controls of the page, from the state (an oscillator's controls follow its engine); binds: what each valued
    // one is made from (its Control::tag)
    std::vector<Control> layoutOf(Ui &u, std::vector<Bind> &binds)
    {
        State &s = u.st;
        std::vector<Control> c;
        binds.clear();
        auto osc = [](int o, int p) { return Bind{Bind::OscN, o, p}; };
        auto sel = [](int p) { return Bind{Bind::OscSel, 0, p}; };
        auto glb = [](int p) { return Bind{Bind::Glb, 0, p}; };
        auto show = [&u](const Bind &b, std::function<std::string(float)> f) -> std::function<std::string(float)>
        {
            if (!f)
                return {};
            return [&u, b, f](float n) { return f(toValue(rangeOf(u.st, b), n)); };
        };
        // a control on a value of the state (0..1), its steps, its default, its name for the overlay
        auto valued = [&](Kind k, float x, float y, float a, float b, const Bind &bd, const std::string &label) -> Control &
        {
            Control ct;
            ct.kind = k, ct.x = x, ct.y = y, ct.a = a, ct.b = b, ct.label = label;
            ct.v = &value(s, bd);
            const Range &rg = rangeOf(s, bd);
            ct.steps = rg.steps;
            ct.def = bd.space == Bind::RouteAmt ? 0.5f : toNorm(rg, rg.def);
            ct.id = ccId(bd);
            ct.tag = static_cast<int>(binds.size());
            binds.push_back(bd);
            c.push_back(std::move(ct));
            return c.back();
        };
        auto knob = [&](float x, float y, float r, Bind b, const char *label, std::function<std::string(float)> f)
        { valued(Kind::Knob, x, y, r, 0, b, label).say = show(b, f); };
        auto fader = [&](float x, float y, float h, Bind b, const std::string &label, std::function<std::string(float)> f)
        {
            Control &ct = valued(Kind::Fader, x, y, h, 0, b, label);
            ct.say = show(b, f);
            ct.travel = h, ct.jump = true;     // a fader: its own length, and it jumps to where it is taken
            ct.badgeX = -4, ct.badgeY = -10;
        };
        auto sw = [&](float x, float y, float w, Bind b, const char *label, std::vector<std::string> options)
        { valued(Kind::Switch, x, y, w, 18, b, label).options = std::move(options); };
        auto toggle = [&](float x, float y, Bind b, const char *label) { valued(Kind::Toggle, x, y, 0, 0, b, label); };
        // a spot that does something (drawn by the page), and a numbered button (which oscillator, which LFO)
        auto hot = [&](float x, float y, float w, float h, std::function<void()> f)
        {
            Control ct;
            ct.kind = Kind::Hot, ct.x = x, ct.y = y, ct.a = w, ct.b = h;
            ct.click = [f](int) { f(); };
            c.push_back(std::move(ct));
        };
        auto pick = [&](float x, float y, float w, float h, int n, bool on, std::function<void()> f)
        {
            Control ct;
            ct.kind = Kind::Button, ct.x = x, ct.y = y, ct.a = w, ct.b = h, ct.label = std::to_string(n + 1), ct.on = on;
            ct.click = [f](int) { f(); };
            c.push_back(std::move(ct));
        };

        // the top bar: the pages, the master volume
        hot(296, 14, 68, 30, [&s] { s.page = 0; });
        hot(388, 14, 144, 30, [&s] { s.page = 1; });
        knob(1470, 29, 15, glb(GLB_VOLUME), "", pct);

        if (s.page == 1)
        {
            for (int p = 0; p < 5; ++p)
                hot(MX0 + static_cast<float>(p) * 150, 84, 140, 28, [&s, p] { s.mxPage = p; });
            const auto cols = mxCols(s.mxPage, s.mxPage < 4 ? partOf(s, s.mxPage) : 0);
            const float cw = (MX1 - MX0) / static_cast<float>(cols.size());
            for (const MxRow &row : mxRows(s, static_cast<int>(u.srcs.size())))
            {
                if (row.src < 0)
                {
                    const int g = row.group;
                    hot(272, row.y, 140, row.h, [&s, g] { s.grpOpen[static_cast<size_t>(g)] = !s.grpOpen[static_cast<size_t>(g)]; });
                    continue;
                }
                const int src = row.src;
                hot(272, row.y, 96, row.h, [&s, src] { showSource(s, src); });
                for (size_t k = 0; k < cols.size(); ++k)
                {
                    const int dst = cols[k].dst;
                    Control ct;
                    ct.kind = Kind::Cell, ct.x = MX0 + static_cast<float>(k) * cw, ct.y = row.y, ct.a = cw, ct.b = row.h;
                    ct.get = [&s, src, dst] {
                        const int r = slotAt(s, src, dst);
                        return r >= 0 ? s.routes[static_cast<size_t>(r)].amount : 0.0f;
                    };
                    ct.set = [&s, src, dst](float n) {
                        if (const int r = slotAt(s, src, dst); r >= 0)
                            s.routes[static_cast<size_t>(r)].amount = n, s.selRoute = r;
                    };
                    ct.press = [&u, src, dst](int button, bool twice) { return cellPress(u, src, dst, button, twice); };
                    ct.tag = static_cast<int>(binds.size());
                    binds.push_back({Bind::Cell, src, dst});
                    c.push_back(std::move(ct));
                }
            }
            // the left column: the chosen route, then every route (a click chooses it and shows its page)
            const int sr = s.selRoute;
            if (sr >= 0 && s.routes[static_cast<size_t>(sr)].dst >= 0)
            {
                valued(Kind::Knob, 129, 238, 30, 0, {Bind::RouteAmt, sr, 0}, "").say = [](float n) { return fmt("%+.0f%%", (static_cast<double>(n) - 0.5) * 400); };
                hot(28, 330, 96, 28, [&s, sr] { s.routes[static_cast<size_t>(sr)].on = !s.routes[static_cast<size_t>(sr)].on; });
                hot(134, 330, 96, 28, [&s, sr] { s.routes[static_cast<size_t>(sr)] = Route{}; });
            }
            float ly = 404;
            for (size_t i = 0; i < s.routes.size(); ++i)
                if (s.routes[i].dst >= 0 && ly < 860)
                {
                    const int k = static_cast<int>(i);
                    hot(22, ly, 214, 22, [&s, k] { s.selRoute = k; s.mxPage = pageOf(s.routes[static_cast<size_t>(k)].dst); });
                    ly += 23;
                }
            return c;
        }

        // four oscillators
        for (int o = 0; o < 4; ++o)
        {
            const float x = 256 + static_cast<float>(o) * 335, y = 72;
            const int part = partOf(s, o);
            sw(x + 16, y + 52, 44, osc(o, OSC_PART_TYPE), "ENGINE", {"STD", "WT", "FM", "GR", "SUP"});
            if (part == 0)
            {
                knob(x + 72, y + 150, 24, osc(o, OSC_WAVE), "WAVE", nullptr);
                knob(x + 186, y + 120, 30, osc(o, OSC_TUNE), "TUNE", semis);
                knob(x + 266, y + 112, 18, osc(o, OSC_FINE_TUNE), "FINE", two);
                knob(x + 186, y + 214, 16, osc(o, OSC_LEVEL), "LEVEL", pct);
                knob(x + 266, y + 214, 16, osc(o, OSC_PAN), "PAN", panName);
            }
            else if (part == 1)
            {
                knob(x + 56, y + 210, 21, osc(o, OSC_WT_MORPH), "POSITION", two);
                knob(x + 132, y + 212, 15, osc(o, OSC_TUNE), "TUNE", semis);
                knob(x + 186, y + 212, 15, osc(o, OSC_FINE_TUNE), "FINE", two);
                knob(x + 240, y + 212, 15, osc(o, OSC_LEVEL), "LEVEL", pct);
                knob(x + 292, y + 212, 15, osc(o, OSC_PAN), "PAN", panName);
            }
            else if (part == 3)
            {
                knob(x + 50, y + 210, 21, osc(o, OSC_WT_MORPH), "POSITION", two);
                knob(x + 116, y + 212, 13, osc(o, OSC_GR_SIZE), "SIZE", sec);
                knob(x + 162, y + 212, 13, osc(o, OSC_GR_DENSITY), "DENS", [](float v) { return fmt("%.0f/s", static_cast<double>(v)); });
                knob(x + 208, y + 212, 13, osc(o, OSC_GR_SPRAY), "SPRAY", pct);
                knob(x + 254, y + 212, 13, osc(o, OSC_TUNE), "TUNE", semis);
                knob(x + 298, y + 212, 13, osc(o, OSC_LEVEL), "LVL", pct);
                // the recording: the display's arrows, through the samples folder
                auto step = [&u, &s, o](int dir)
                {
                    if (u.samples.empty())
                        return;
                    const auto &v = u.samples;
                    std::string &f = s.grainFile[static_cast<size_t>(o)];
                    const std::string cur = f.substr(f.rfind('/') + 1);
                    auto it = std::find(v.begin(), v.end(), cur);
                    const auto n = static_cast<std::ptrdiff_t>(v.size());
                    std::ptrdiff_t i = it == v.end() ? 0 : (it - v.begin()) + dir;
                    i = (i % n + n) % n;
                    f = v[static_cast<size_t>(i)];
                };
                hot(x + 18, y + 84, 14, 18, [step] { step(-1); });
                hot(x + 34, y + 84, 14, 18, [step] { step(1); });
            }
            else if (part == 4)
            {
                // SUPER: seven voices of the wave; DETUNE and SIDE on the granular knobs (the same matrix destination)
                knob(x + 50, y + 210, 21, osc(o, OSC_WT_MORPH), "DETUNE", pct);
                knob(x + 116, y + 212, 13, osc(o, OSC_GR_SPRAY), "SIDE", pct);
                knob(x + 162, y + 212, 13, osc(o, OSC_WAVE), "WAVE", superWave);
                knob(x + 208, y + 212, 13, osc(o, OSC_TUNE), "TUNE", semis);
                knob(x + 254, y + 212, 13, osc(o, OSC_LEVEL), "LVL", pct);
                knob(x + 298, y + 212, 13, osc(o, OSC_PAN), "PAN", panName);
            }
            else
            {
                knob(x + 56, y + 210, 21, osc(o, OSC_FM_MOD_INDEX), "INDEX", [](float v) { return fmt("%.1f", static_cast<double>(v)); });
                knob(x + 132, y + 212, 15, osc(o, OSC_FM_MOD_RATIO), "M ×", two);
                knob(x + 186, y + 212, 15, osc(o, OSC_FM_FEEDBACK), "FDBK", two);
                knob(x + 240, y + 212, 15, osc(o, OSC_FM_OP2_RATIO), "2 ×", two);
                knob(x + 292, y + 212, 15, osc(o, OSC_FM_OP2_LEVEL), "2 LVL", pct);
                // the algorithm: the display's arrows
                hot(x + 18, y + 84, 22, 18, [&s, o] { float &a = s.osc[o][OSC_FM_ALGORITHM]; a = std::max(0.0f, a - 0.25f); });
                hot(x + 84, y + 84, 22, 18, [&s, o] { float &a = s.osc[o][OSC_FM_ALGORITHM]; a = std::min(1.0f, a + 0.25f); });
            }
        }

        // the chosen oscillator's filter and envelopes; which one: the buttons in the header
        const float ry = 350;
        for (int o = 0; o < 4; ++o)
            pick(560 + static_cast<float>(o) * 38, ry + 8, 32, 20, o, s.selOsc == o, [&s, o] { s.selOsc = o; });
        sw(272, ry + 52, 46, sel(OSC_FILT_TYPE), "TYPE", {"LP", "HP", "BP", "NOTCH"});
        knob(360, ry + 150, 44, sel(OSC_CUTOFF), "CUTOFF", hz);
        knob(486, ry + 140, 28, sel(OSC_RESO), "RESO", two);
        knob(590, ry + 96, 17, sel(OSC_HPF), "HPF", hz);
        knob(668, ry + 96, 17, sel(OSC_KEY_TRACK), "KEYTRK", pct);
        knob(629, ry + 180, 17, sel(OSC_FILT_ENV_AMT), "ENV AMT", [](float v) { return fmt("%+.0f%%", static_cast<double>(v) * 100); });
        const int fenv[4] = {OSC_FILT_ATK, OSC_FILT_DEC, OSC_FILT_SUS, OSC_FILT_REL}, aenv[4] = {OSC_AMP_ATK, OSC_AMP_DEC, OSC_AMP_SUS, OSC_AMP_REL};
        const char *adsr[4] = {"A", "D", "S", "R"};
        for (int i = 0; i < 4; ++i)
        {
            std::function<std::string(float)> f = i == 2 ? std::function<std::string(float)>(pct) : std::function<std::string(float)>(sec);
            fader(782 + static_cast<float>(i) * 66, ry + 56, 136, sel(fenv[i]), adsr[i], f);
            fader(1092 + static_cast<float>(i) * 66, ry + 56, 136, sel(aenv[i]), adsr[i], f);
        }
        // LFO: which one, its shape and rate (where it goes: the mod matrix)
        for (int l = 0; l < 4; ++l)
            pick(1440 + static_cast<float>(l) * 34, ry + 8, 28, 20, l, s.selLfo == l, [&s, l] { s.selLfo = l; });
        sw(1370, ry + 52, 38, {Bind::LfoSel, 0, GLB_LFO1_WAVE}, "SHAPE", {"SIN", "TRI", "SQ", "SAW", "RND"});
        knob(1470, ry + 146, 30, {Bind::LfoSel, 0, GLB_LFO1_RATE}, "RATE", [](float v) { return fmt("%.2f Hz", static_cast<double>(v)); });
        hot(1380, ry + 206, 180, 22, [&s] { s.page = 1; });

        // mixer, tube, delay, master filter, xy
        const float by = 606;
        for (int o = 0; o < 4; ++o)
            fader(296 + static_cast<float>(o) * 64, by + 66, 150, osc(o, OSC_LEVEL), std::to_string(o + 1), pct);
        fader(552, by + 66, 150, glb(GLB_VOLUME), "MST", pct);
        knob(660, by + 140, 28, glb(GLB_TUBE_DRV), "DRIVE", [](float v) { return fmt("%.1f", static_cast<double>(v)); });
        knob(752, by + 140, 20, glb(GLB_TUBE_ASYM), "ASYM", pct);
        knob(705, by + 226, 17, glb(GLB_TUBE_MIX), "MIX", pct);
        knob(870, by + 98, 22, glb(GLB_DLY_L), "LEFT", sec);
        knob(960, by + 98, 22, glb(GLB_DLY_R), "RIGHT", sec);
        knob(1050, by + 98, 22, glb(GLB_DLY_FB), "FDBK", pct);
        knob(870, by + 206, 18, glb(GLB_DLY_MIX), "MIX", pct);
        knob(960, by + 206, 18, glb(GLB_DLY_DMP), "DAMP", pct);
        toggle(1050, by + 216, glb(GLB_DLY_PING), "PING-PONG");
        sw(1130, by + 52, 41, glb(GLB_MF_TYPE), "", {"LP", "HP", "BP", "NOTCH"});
        knob(1180, by + 150, 30, glb(GLB_MF_CUTOFF), "CUTOFF", hz);
        knob(1270, by + 150, 20, glb(GLB_MF_RESO), "RESO", two);
        toggle(1270, by + 240, glb(GLB_MF_BYPASS), "BYPASS");
        {
            Control xy; // the XY pad: a press there is the path REC records (group: pressed)
            xy.kind = Kind::XY, xy.x = 1350, xy.y = by + 42, xy.a = 222, xy.b = 120, xy.v = &s.glb[GLB_XY_X], xy.vy = &s.glb[GLB_XY_Y], xy.group = 0;
            c.push_back(std::move(xy));
        }
        // the vector: on, record a path (the next drag on the pad), loop it
        hot(1440, by + 8, 40, 20, [&s] { s.vecOn = !s.vecOn; });
        hot(1484, by + 8, 40, 20, [&u] { u.vecRec = !u.vecRec; });
        hot(1528, by + 8, 44, 20, [&s] { s.vecLoop = !s.vecLoop; });
        return c;
    }

    // ── what does not move, as one SVG (under the SDK's bar and display) ────────────────────────────
    void svgPanel(std::ostringstream &s, float x, float y, float w, float h)
    {
        panelkit::svgPanel(s, x, y, w, h);
        panelkit::svgRule(s, x + 12, y + 31, w - 24);
    }

    // the page a background key names: its page, its oscillators' engines, the route knob there or not (baseKeyOf)
    State stateOf(const std::string &key)
    {
        State s;
        s.page = key.empty() ? 0 : key[0] - '0';
        for (int o = 0; o < 4 && o + 1 < static_cast<int>(key.size()); ++o)
            s.osc[o][OSC_PART_TYPE] = static_cast<float>(key[static_cast<size_t>(o + 1)] - '0') / static_cast<float>(kPartCount - 1);
        s.selRoute = -1;
        if (s.page == 1 && key.size() > 5 && key[5] == 'r')
            s.routes[0] = {0, 0, 0.5f, true}, s.selRoute = 0;
        return s;
    }

    std::string bodyOf(const panelkit::Theme &t, const State &st, const std::vector<Control> &ctls)
    {
        std::ostringstream s;
        svgPanel(s, 14, 72, 230, 814);
        if (st.page == 1)
        {
            svgPanel(s, 256, 72, 1330, 814);
            s << "<rect x='" << MX0 << "' y='120' width='" << MX1 - MX0 << "' height='42' rx='3' fill='" << t.display << "' fill-opacity='.6'/>";
        }
        else
        {
            for (int o = 0; o < 4; ++o)
                svgPanel(s, 256 + static_cast<float>(o) * 335, 72, 325, 268);
            svgPanel(s, 256, 350, 470, 244);
            svgPanel(s, 736, 350, 300, 244);
            svgPanel(s, 1046, 350, 300, 244);
            svgPanel(s, 1356, 350, 230, 244);
            svgPanel(s, 256, 606, 330, 280);
            svgPanel(s, 596, 606, 210, 280);
            svgPanel(s, 816, 606, 290, 280);
            svgPanel(s, 1116, 606, 210, 280);
            svgPanel(s, 1336, 606, 250, 280);
            for (int o = 0; o < 4; ++o)
                if (partOf(st, o) != 0)
                    s << "<rect x='" << 270 + static_cast<float>(o) * 335 << "' y='152' width='297' height='96' rx='4' fill='" << t.display << "' stroke='#000'/>";
            s << "<rect x='1350' y='648' width='222' height='120' rx='4' fill='" << t.display << "' stroke='#000'/>";
            s << "<rect x='1350' y='778' width='222' height='92' rx='4' fill='" << t.display << "' stroke='#000'/>";
            for (int i = 1; i < 4; ++i)
                s << "<line x1='" << 1350 + i * 55.5 << "' y1='648' x2='" << 1350 + i * 55.5 << "' y2='768' stroke='" << t.grid << "'/>"
                  << "<line x1='1350' y1='" << 648 + i * 30 << "' x2='1572' y2='" << 648 + i * 30 << "' stroke='" << t.grid << "'/>";
            s << "<line x1='530' y1='662' x2='530' y2='862' stroke='#000' stroke-opacity='.5'/>";
        }
        // the knobs' bodies and the faders' slots: the 4D's own (finer knurls, a tick every tenth)
        for (const Control &c : ctls)
        {
            if (c.kind == Kind::Knob)
            {
                const double cx = static_cast<double>(c.x), cy = static_cast<double>(c.y), r = static_cast<double>(c.a);
                for (int i = 0; i <= 10; ++i)
                {
                    const double a = (-135 + 27.0 * i - 90) * M_PI / 180.0, r1 = r + 4, r2 = r + ((i % 5 == 0) ? 8 : 6);
                    s << "<line x1='" << cx + r1 * std::cos(a) << "' y1='" << cy + r1 * std::sin(a) << "' x2='" << cx + r2 * std::cos(a) << "' y2='"
                      << cy + r2 * std::sin(a) << "' stroke='" << t.dim << "' stroke-width='1.2'/>";
                }
                s << "<circle cx='" << cx << "' cy='" << cy + 3 << "' r='" << r + 5 << "' fill='url(#sh)'/>";
                s << "<circle cx='" << cx << "' cy='" << cy << "' r='" << r << "' fill='url(#kb)' stroke='#050505'/>";
                for (int i = 0; i < 36; ++i)
                {
                    const double a = i * 10 * M_PI / 180.0;
                    s << "<line x1='" << cx + (r - 1) * std::cos(a) << "' y1='" << cy + (r - 1) * std::sin(a) << "' x2='" << cx + r * 0.8 * std::cos(a)
                      << "' y2='" << cy + r * 0.8 * std::sin(a) << "' stroke='#2a2a2c' stroke-width='.8'/>";
                }
            }
            else if (c.kind == Kind::Fader)
            {
                s << "<rect x='" << c.x - 3 << "' y='" << c.y << "' width='6' height='" << c.a << "' rx='3' fill='#0a0a0b' stroke='#000'/>";
                for (int i = 0; i <= 10; ++i)
                {
                    const float yy = c.y + c.a * static_cast<float>(i) / 10;
                    s << "<line x1='" << c.x - 12 << "' y1='" << yy << "' x2='" << c.x - 7 << "' y2='" << yy << "' stroke='" << t.dim << "' stroke-width='.8'/>";
                }
            }
        }
        return s.str();
    }

    std::string capSvg(float r)
    {
        const double rr = static_cast<double>(r), size = rr * 2 + 2, c = size / 2;
        std::ostringstream s;
        s << panelkit::svgOpen(size, size)
          << "<defs><radialGradient id='c' cx='.35' cy='.3' r='.9'><stop offset='0' stop-color='#f2f2ef'/><stop offset='.45' stop-color='#a9a9a6'/>"
             "<stop offset='1' stop-color='#55554f'/></radialGradient></defs>"
          << "<circle cx='" << c << "' cy='" << c << "' r='" << rr * 0.62 << "' fill='url(#c)' stroke='#111'/>"
          << "<line x1='" << c << "' y1='" << c - rr * 0.15 << "' x2='" << c << "' y2='" << c - rr * 0.9 << "' stroke='#f4f2ec' stroke-width='"
          << std::max(1.6, rr / 11) << "' stroke-linecap='round'/></svg>";
        return s.str();
    }

    // the 4D's knob cap and fader cap (a size up from panel_kit's), kept with the canvas's pieces
    SDL_Texture *pieceOf(Canvas &cv, const std::string &key, const std::string &svg, float w, float h)
    {
        auto it = cv.pieces.find(key);
        if (it == cv.pieces.end())
            it = cv.pieces.emplace(key, panelkit::rasterize(cv.r, svg, static_cast<int>(w * cv.scale), static_cast<int>(h * cv.scale))).first;
        return it->second;
    }

    // the shapes around the WAVE knob
    void waveIcon(Canvas &cv, float x, float y, int kind, bool on)
    {
        std::vector<SDL_FPoint> p;
        const float pi = static_cast<float>(M_PI);
        switch (kind)
        {
        case 0:
            for (int i = 0; i < 15; ++i)
                p.push_back({x + static_cast<float>(i), y - 6 * std::sin(static_cast<float>(i) / 14 * 2 * pi)});
            break;
        case 1: p = {{x, y + 6}, {x + 14, y - 6}, {x + 14, y + 6}}; break;
        case 2: p = {{x, y + 6}, {x, y - 6}, {x + 7, y - 6}, {x + 7, y + 6}, {x + 14, y + 6}, {x + 14, y - 6}}; break;
        case 3: p = {{x, y + 6}, {x + 7, y - 6}, {x + 14, y + 6}}; break;
        case 4: p = {{x, y + 6}, {x, y - 6}, {x + 3, y - 6}, {x + 3, y + 6}, {x + 14, y + 6}}; break;
        default:
            for (int i = 0; i < 15; ++i)
                p.push_back({x + static_cast<float>(i), y + ((i * 7) % 5 > 2 ? 5.0f : -5.0f) * (i % 2 ? 1.0f : 0.5f)});
            break;
        }
        cv.lines(p, on ? cv.theme.accent : cv.theme.dim, 1, 1.4f);
    }

    // the FM algorithms as the engine computes them (oscillator.cpp): C carrier, M modulator, 2..4 operators
    struct Op
    {
        const char *n;
        float col, row;
    };
    struct Algo
    {
        std::vector<Op> ops;
        std::vector<std::pair<const char *, const char *>> links;
        bool fb;
    };
    const Algo kAlgos[5] = {
        {{{"C", 0, 0}, {"M", 0, 1}}, {{"M", "C"}}, false},
        {{{"C", 0, 0}, {"M", 0, 1}}, {{"M", "C"}}, true},
        {{{"C", 0, 0}, {"M", 0, 1}, {"2", 0, 2}, {"3", 0, 3}, {"4", 0, 4}}, {{"4", "3"}, {"3", "2"}, {"2", "M"}, {"M", "C"}}, false},
        {{{"C", 0, 0}, {"M", -.6f, 1}, {"2", .6f, 1}, {"3", -.6f, 2}, {"4", .6f, 2}}, {{"3", "M"}, {"4", "2"}, {"M", "C"}, {"2", "C"}}, false},
        {{{"C", 0, 0}, {"M", -.6f, 1}, {"3", -.6f, 2}, {"4", -.6f, 3}, {"2", .6f, 1}}, {{"4", "3"}, {"3", "M"}, {"M", "C"}, {"2", "C"}}, false},
    };

    void algoDiagram(Canvas &cv, float cx, float bottom, int ai, float bx, float step)
    {
        const Algo &a = kAlgos[ai];
        std::map<std::string, SDL_FPoint> pos;
        for (const Op &o : a.ops)
            pos[o.n] = {cx + o.col * bx * 1.9f, bottom - o.row * step};
        for (auto &[from, to] : a.links)
            cv.lines({{pos[from].x, pos[from].y + bx / 2}, {pos[to].x, pos[to].y - bx / 2}}, cv.theme.accent, 0.8f, 1);
        if (a.fb)
        {
            const SDL_FPoint p = pos["C"];
            cv.lines({{p.x + bx / 2, p.y}, {p.x + bx * 0.9f, p.y}, {p.x + bx * 0.9f, p.y - bx}, {p.x - bx * 0.4f, p.y - bx}}, cv.theme.accent, 0.8f, 1);
        }
        const SDL_FPoint c = pos["C"];
        cv.lines({{c.x, c.y + bx / 2}, {c.x, c.y + bx / 2 + 8}}, cv.theme.accent, 1, 1);
        for (auto &[n, p] : pos)
        {
            const bool carrier = n == "C";
            cv.rect(p.x - bx / 2, p.y - bx / 2, bx, bx, cv.theme.display);
            cv.rect(p.x - bx / 2, p.y - bx / 2, bx, bx, carrier ? cv.theme.accent : cv.theme.trace, 1, false);
            cv.say(n, p.x, p.y + bx * 0.3f, bx * 0.62f, carrier ? cv.theme.accent : cv.theme.traceHi, 1, true, true, 0, Centre);
        }
    }

    // an arc, a band thick wide at radius rad, from a0 to a1 degrees (0 up, clockwise)
    void arc(Canvas &cv, float cx, float cy, float rad, float thick, float a0, float a1, const std::string &hex, float alpha)
    {
        if (a1 < a0)
            std::swap(a0, a1);
        // four rings: the band, and a pixel either side fading out (the renderer draws geometry without smoothing)
        const SDL_FColor c = panelkit::color(hex, alpha), clear = panelkit::color(hex, 0);
        const float f = 1.0f / cv.scale;
        const float rings[4] = {rad - thick / 2 - f, rad - thick / 2 + f * 0.5f, rad + thick / 2 - f * 0.5f, rad + thick / 2 + f};
        const int n = std::max(2, static_cast<int>((a1 - a0) / 2) + 2);
        std::vector<SDL_Vertex> v;
        std::vector<int> idx;
        for (int i = 0; i < n; ++i)
        {
            const float a = (a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(n - 1) - 90) * static_cast<float>(M_PI) / 180;
            for (int k = 0; k < 4; ++k)
                v.push_back({{(cx + rings[k] * std::cos(a)) * cv.scale, (cy + rings[k] * std::sin(a)) * cv.scale}, k == 0 || k == 3 ? clear : c, {0, 0}});
            if (i > 0)
            {
                const int b = (i - 1) * 4;
                for (int k = 0; k < 3; ++k)
                    idx.insert(idx.end(), {b + k, b + k + 1, b + k + 4, b + k + 1, b + k + 5, b + k + 4});
            }
        }
        SDL_RenderGeometry(cv.r, nullptr, v.data(), static_cast<int>(v.size()), idx.data(), static_cast<int>(idx.size()));
    }

    // a cached round piece: a disc (filled: a positive amount; a ring: a negative one) or the glow of a live route
    void piece(Canvas &cv, float cx, float cy, float rad, int kind, const std::string &hex, float alpha)
    {
        const int q = static_cast<int>(std::lround(rad * 2));
        const float size = kind == 2 ? 32.0f : 24.0f;
        char k[64];
        std::snprintf(k, sizeof k, "rnd%d.%d.%s", kind, q, hex.c_str());
        auto it = cv.pieces.find(k);
        if (it == cv.pieces.end())
        {
            std::ostringstream s;
            const double rr = q / 2.0, c = static_cast<double>(size) / 2;
            s << panelkit::svgOpen(static_cast<double>(size), static_cast<double>(size));
            if (kind == 0)
                s << "<circle cx='" << c << "' cy='" << c << "' r='" << rr << "' fill='" << hex << "' stroke='#000' stroke-opacity='.6'/>";
            else if (kind == 1)
                s << "<circle cx='" << c << "' cy='" << c << "' r='" << rr - 1 << "' fill='none' stroke='" << hex << "' stroke-width='2'/>";
            else
                s << "<defs><radialGradient id='g' cx='.5' cy='.5' r='.5'><stop offset='0' stop-color='" << hex << "' stop-opacity='.9'/><stop offset='1' stop-color='"
                  << hex << "' stop-opacity='0'/></radialGradient></defs><circle cx='16' cy='16' r='16' fill='url(#g)'/>";
            s << "</svg>";
            it = cv.pieces.emplace(k, panelkit::rasterize(cv.r, s.str(), static_cast<int>(size * cv.scale), static_cast<int>(size * cv.scale))).first;
        }
        SDL_SetTextureAlphaModFloat(it->second, alpha);
        const SDL_FRect d{std::round((cx - size / 2) * cv.scale), std::round((cy - size / 2) * cv.scale), size * cv.scale, size * cv.scale};
        SDL_RenderTexture(cv.r, it->second, nullptr, &d);
    }

    // what modulates a destination: the routes' amounts (engine units, -2..2 each, summed) and where they are now
    struct Mod
    {
        int routes = 0;
        float span = 0, live = 0;
    };

    Mod modOf(const State &s, const synth::Sound &sound, int dst)
    {
        Mod m;
        if (dst < 0)
            return m;
        for (const Route &rt : s.routes)
            if (rt.dst == dst && rt.on)
            {
                const float a = (rt.amount - 0.5f) * 4; // engine units
                ++m.routes;
                m.span += a;
                m.live += sound.source(rt.src) * a;
            }
        return m;
    }

    // SUPER's display: the seven voices of its shape, the centre over the six spread by DETUNE (as the matrix has it
    // now), dimmer as SIDE lowers them; the count and the shape's name, DETUNE and SIDE
    void superView(Canvas &cv, float dx, float dy, float dw, float dh, const float *o, const Mod &m)
    {
        const panelkit::Theme &t = cv.theme;
        const int shape = superShape(static_cast<int>(std::lround(toValue(OSC_RANGES[OSC_WAVE], o[OSC_WAVE]))));
        const float det = o[OSC_WT_MORPH], live = m.routes > 0 ? std::clamp(det + m.live, 0.0f, 1.0f) : det;
        const float side = o[OSC_GR_SPRAY];
        static const char *names[] = {"7 SAWS", "7 SQUARES", "7 TRIANGLES", "7 SINES"};
        cv.say(names[shape], dx + 8, dy + 14, 9, t.accent, 0.7f, false, true, 2, Left);
        cv.say("DET " + fmt("%.0f%%", static_cast<double>(live) * 100) + "  SIDE " + fmt("%.0f%%", static_cast<double>(side) * 100),
            dx + dw - 8, dy + 14, 8, t.accent, 0.8f, false, true, 0.5f, Right, false);
        auto wave = [shape](float ph, float pw)
        {
            ph -= std::floor(ph);
            switch (shape)
            {
            case 1: return ph < pw ? 1.0f : -1.0f;
            case 2: return ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph;
            case 3: return std::sin(ph * 2 * static_cast<float>(M_PI));
            default: return 2 * ph - 1;
            }
        };
        static const float off[6] = {-1.0f, -0.57f, -0.18f, 0.18f, 0.56f, 0.98f};
        const float x0 = dx + 10, w = dw - 20, mid = dy + dh * 0.6f, amp = (dh - 40) * 0.5f, cycles = 2.5f;
        for (int k = -1; k < 6; ++k)
        {
            const bool centre = k < 0;
            // each side a little faster or slower (the spread drawn larger than it sounds, to be seen) and its own start
            const float ratio = centre ? 1.0f : 1.0f + off[k] * live * 0.18f, start = centre ? 0.0f : 0.13f * static_cast<float>(k + 1);
            const float pw = centre ? 0.5f : 0.5f + off[k] * live * 0.3f;
            std::vector<SDL_FPoint> pts;
            for (int i = 0; i <= 160; ++i)
            {
                const float u = static_cast<float>(i) / 160;
                pts.push_back({x0 + u * w, mid - amp * wave(start + u * cycles * ratio, pw)});
            }
            if (centre)
                cv.lines(pts, t.accent, 0.5f + 0.5f * (1 - 0.4f * side), 2.0f);
            else
                cv.lines(pts, t.trace, 0.12f + 0.5f * side, 1.0f);
        }
    }

    // the destination a main-page control sets, or -1
    int destOf(const State &s, const Bind &b)
    {
        int o = -1, p = b.param;
        if (b.space == Bind::OscN)
            o = b.index;
        else if (b.space == Bind::OscSel)
            o = s.selOsc;
        if (o >= 0)
        {
            switch (p)
            {
            case OSC_TUNE: return o * 17 + 0;
            case OSC_FINE_TUNE: return o * 17 + 1;
            case OSC_WAVE: return o * 17 + 2;
            case OSC_LEVEL: return o * 17 + 3;
            case OSC_PAN: return o * 17 + 4;
            case OSC_FM_MOD_INDEX: return o * 17 + 6;
            case OSC_CUTOFF: return o * 17 + 7;
            case OSC_RESO: return o * 17 + 8;
            case OSC_FILT_ENV_AMT: return o * 17 + 9;
            case OSC_AMP_ATK: return o * 17 + 10;
            case OSC_AMP_DEC: return o * 17 + 11;
            case OSC_AMP_SUS: return o * 17 + 12;
            case OSC_AMP_REL: return o * 17 + 13;
            case OSC_FX_P1: return o * 17 + 15;
            case OSC_FX_P2: return o * 17 + 16;
            case OSC_WT_MORPH: return 68 + o;
            default: return -1;
            }
        }
        if (b.space == Bind::LfoSel)
            p += s.selLfo;
        else if (b.space != Bind::Glb)
            return -1;
        switch (p)
        {
        case GLB_VOLUME: return 224;
        case GLB_TUBE_DRV: return 226;
        case GLB_TUBE_ASYM: return 227;
        case GLB_MFX_P1: return 221;
        case GLB_MFX_P2: return 222;
        case GLB_MFX_P3: return 223;
        default: return p >= GLB_LFO1_RATE && p <= GLB_LFO4_RATE ? 230 + (p - GLB_LFO1_RATE) : -1;
        }
    }

    // the same, only when the engine applies it
    int reachOf(const State &s, const Bind &b)
    {
        const int d = destOf(s, b);
        return d >= 0 && wired(d) ? d : -1;
    }

    // a control's value (0..1) with the matrix's mod on it
    float haloNorm(const State &s, const Bind &b, float n, int dst, float mod)
    {
        const Range &rg = rangeOf(s, b);
        const float v = std::clamp(modded(dst, toValue(rg, n), mod), std::min(rg.lo, rg.hi), std::max(rg.lo, rg.hi));
        return std::clamp(toNorm(rg, v), 0.0f, 1.0f);
    }

    // the sources' handles on MAIN: drag one onto a knob and the matrix routes it there
    struct Handle
    {
        float x, y;
        int src;
        std::string label;
    };

    std::vector<Handle> handles(const State &s)
    {
        return {{1425, 372, s.selLfo, "LFO " + std::to_string(s.selLfo + 1)},
                {872, 372, 4 + s.selOsc, "F.ENV " + std::to_string(s.selOsc + 1)},
                {1162, 372, 8 + s.selOsc, "AMP ENV " + std::to_string(s.selOsc + 1)},
                {1396, 628, 15, "XY X"},
                {1422, 628, 16, "XY Y"}};
    }

    void drawHandle(Canvas &cv, const Handle &h, bool live)
    {
        const panelkit::Theme &t = cv.theme;
        piece(cv, h.x, h.y, 8, 1, t.accent, live ? 1.0f : 0.7f);
        piece(cv, h.x, h.y, 3, 0, t.accent, live ? 1.0f : 0.7f);
        if (h.src == 15 || h.src == 16)
            cv.say(h.src == 15 ? "X" : "Y", h.x - 14, h.y + 4, 9, t.dim, 1, true, false, 0, Centre);
    }

    // the matrix under the cells: the pages, the columns' heads, the sources with what they do now, the crosshair
    void matrixUnder(Canvas &cv, Ui &u, const synth::Sound &sound)
    {
        State &s = u.st;
        const panelkit::Theme &t = cv.theme;
        cv.say("MOD MATRIX", 270, 104, 12, t.ink, 1, true, false, 2.4f, Left);
        for (int p = 0; p < 5; ++p)
        {
            const float x = MX0 + static_cast<float>(p) * 150;
            int n = 0;
            for (const Route &rt : s.routes)
                n += rt.dst >= 0 && pageOf(rt.dst) == p;
            cv.box(x, 84, 140, 28, s.mxPage == p);
            std::string name = kMxPages[p];
            if (p < 4)
                name += std::string(" · ") + kPart[partOf(s, p)];
            cv.say(name, x + 12, 103, 10, s.mxPage == p ? t.accent : t.ink, 1, true, false, 1.4f, Left);
            if (n > 0)
                cv.say(std::to_string(n), x + 128, 103, 10, t.accent, 0.9f, false, true, 0, Right);
        }
        cv.say("click adds  ·  drag, wheel: amount  ·  double click mutes  ·  right click removes  ·  a source's name: its controls", MX1, 874, 9, t.ink, 0.5f, false,
            false, 0.5f, Right);

        const auto rows = mxRows(s, static_cast<int>(u.srcs.size()));
        const auto cols = mxCols(s.mxPage, s.mxPage < 4 ? partOf(s, s.mxPage) : 0);
        const float cw = (MX1 - MX0) / static_cast<float>(cols.size());
        const float bottom = rows.back().y + rows.back().h;
        // the rows: a band every other source, a line over each group
        int hr = -1, hc = -1, n = 0;
        if (u.hover.x >= MX0 && u.hover.x < MX1)
            hc = static_cast<int>((u.hover.x - MX0) / cw);
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const MxRow &row = rows[i];
            if (row.src < 0)
            {
                cv.rect(272, std::round(row.y), MX1 - 272, 1, "#000000", 0.5f);
                n = 0;
                continue;
            }
            if (n++ % 2 == 0)
                cv.rect(272, row.y, MX1 - 272, row.h, t.row, 0.28f);
            if (u.hover.y >= row.y && u.hover.y < row.y + row.h && u.hover.x >= 272 && u.hover.x < MX1)
                hr = static_cast<int>(i);
        }
        // a destination a halo sent here: its column lit, fading
        const uint64_t now = SDL_GetTicks();
        if (u.flashDst >= 0 && now < u.flashUntil)
            for (size_t k = 0; k < cols.size(); ++k)
                if (cols[k].dst == u.flashDst)
                    cv.rect(MX0 + static_cast<float>(k) * cw, 120, cw, bottom - 120, t.accent, 0.25f * static_cast<float>(u.flashUntil - now) / 1500.0f);
        if (hr >= 0 && hc >= 0)
        {
            cv.rect(272, rows[static_cast<size_t>(hr)].y, MX1 - 272, rows[static_cast<size_t>(hr)].h, t.accent, 0.07f);
            cv.rect(MX0 + static_cast<float>(hc) * cw, 120, cw, bottom - 120, t.accent, 0.07f);
        }
        // the heads: groups, names, and what lands on each destination now
        std::string group;
        for (size_t k = 0; k < cols.size(); ++k)
        {
            const float x = MX0 + static_cast<float>(k) * cw;
            if (cols[k].group != group)
            {
                group = cols[k].group;
                if (k > 0)
                    cv.rect(std::round(x), 124, 1, bottom - 124, "#000000", 0.55f);
                cv.say(group, x + 6, 134, 8, t.dim, 1, false, false, 1.6f, Left);
            }
            const bool hot = static_cast<int>(k) == hc;
            cv.say(cols[k].label, x + cw / 2, 151, 9, hot ? t.accent : t.ink, hot ? 1.0f : 0.85f, true, false, 0.6f, Centre);
            const Mod m = modOf(s, sound, cols[k].dst);
            const float mw = std::min(cw - 14, 40.0f), mx = std::round(x + cw / 2);
            cv.rect(mx - mw / 2, 156, mw, 3, t.display);
            if (m.routes > 0)
            {
                const float w = std::clamp(m.live, -1.0f, 1.0f) * mw / 2;
                cv.rect(w >= 0 ? mx : mx + w, 156, std::fabs(w), 3, t.traceHi);
            }
        }
        // the group heads and the sources: a name and a meter of each now
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const MxRow &row = rows[i];
            if (row.src < 0)
            {
                int routes = 0;
                for (const Route &rt : s.routes)
                    routes += rt.dst >= 0 && srcGroup(rt.src) == row.group;
                const bool locked = routes > 0; // in use: stays open
                // the arrow drawn: the fonts lack it
                const float ax = 278, ay = row.y + row.h / 2;
                const SDL_FColor ac = panelkit::color(locked ? t.accent : t.dim);
                const SDL_FPoint p0 = row.open ? SDL_FPoint{ax - 3.5f, ay - 2} : SDL_FPoint{ax - 2, ay - 3.5f};
                const SDL_FPoint p1 = row.open ? SDL_FPoint{ax + 3.5f, ay - 2} : SDL_FPoint{ax - 2, ay + 3.5f};
                const SDL_FPoint p2 = row.open ? SDL_FPoint{ax, ay + 2.5f} : SDL_FPoint{ax + 2.5f, ay};
                const SDL_Vertex tri[3] = {{{p0.x * cv.scale, p0.y * cv.scale}, ac, {0, 0}}, {{p1.x * cv.scale, p1.y * cv.scale}, ac, {0, 0}},
                                           {{p2.x * cv.scale, p2.y * cv.scale}, ac, {0, 0}}};
                SDL_RenderGeometry(cv.r, nullptr, tri, 3, nullptr, 0);
                std::string head = kSrcGroups[row.group];
                if (!row.open)
                {
                    int n = 0;
                    for (const Choice &c : u.srcs)
                        n += srcGroup(c.id) == row.group;
                    head += "  ·  " + std::to_string(n);
                }
                cv.say(head, 288, row.y + 12.5f, 8, locked ? t.accent : t.dim, locked ? 0.8f : 1.0f, false, false, 1.6f, Left);
                continue;
            }
            const int id = row.src;
            const bool hot = static_cast<int>(i) == hr;
            cv.say(u.srcs[static_cast<size_t>(id)].name, 284, row.y + row.h / 2 + 4.5f, 9.5f, hot ? t.accent : t.ink, hot ? 1.0f : 0.8f, hot, false, 0.8f, Left);
            const float v = std::clamp(sound.source(id), -1.0f, 1.0f), bx = 372, bw = 38, by = std::round(row.y + row.h / 2 - 2);
            cv.rect(bx, by, bw, 4, t.display);
            if (srcBipolar(id))
            {
                const float w = v * bw / 2;
                cv.rect(w >= 0 ? bx + bw / 2 : bx + bw / 2 + w, by, std::fabs(w), 4, t.trace);
                cv.rect(bx + bw / 2, by - 1, 1, 6, t.dim);
            }
            else
                cv.rect(bx, by, std::max(0.0f, v) * bw, 4, t.trace);
        }
    }

    // the left column on the matrix page: the chosen route, then every route
    void matrixInspector(Canvas &cv, Ui &u, const synth::Sound &sound)
    {
        State &s = u.st;
        const panelkit::Theme &t = cv.theme;
        cv.say("ROUTE", 28, 94, 12, t.ink, 1, true, false, 2.4f, Left);
        const int sr = s.selRoute;
        if (sr >= 0 && s.routes[static_cast<size_t>(sr)].dst >= 0)
        {
            const Route &rt = s.routes[static_cast<size_t>(sr)];
            cv.say("SLOT " + std::to_string(sr + 1), 230, 94, 9, t.dim, 1, false, true, 1, Right);
            cv.say(u.srcs[static_cast<size_t>(rt.src)].name, 129, 128, 13, rt.on ? t.accent : t.dim, 1, true, false, 1.2f, Centre);
            cv.say("↓", 129, 150, 12, t.dim, 1, false, false, 0, Centre);
            cv.say(destName(rt.dst), 129, 174, 13, t.ink, 1, true, false, 1.2f, Centre);
            const float v = std::clamp(sound.source(rt.src) * (rt.amount - 0.5f) * 2, -1.0f, 1.0f);
            cv.rect(34, 304, 190, 5, t.display);
            cv.rect(128.5f, 300, 1, 13, t.dim);
            if (rt.on)
            {
                const float w = 95 * v;
                cv.rect(w >= 0 ? 129 : 129 + w, 304, std::fabs(w), 5, t.traceHi);
            }
            cv.box(28, 330, 96, 28, rt.on);
            cv.say(rt.on ? "ON" : "MUTED", 76, 349, 10, rt.on ? t.accent : t.dim, 1, true, false, 1.5f, Centre);
            cv.box(134, 330, 96, 28, false);
            cv.say("REMOVE", 182, 349, 10, t.ink, 1, true, false, 1.5f, Centre);
        }
        else
        {
            cv.say("click a cell", 129, 200, 11, t.ink, 0.7f, false, false, 0.5f, Centre);
            cv.say("or drag a source's", 129, 220, 11, t.ink, 0.7f, false, false, 0.5f, Centre);
            cv.say("MOD onto a knob", 129, 240, 11, t.ink, 0.7f, false, false, 0.5f, Centre);
        }
        int used = 0;
        for (const Route &rt : s.routes)
            used += rt.dst >= 0;
        cv.rect(22, 378, 214, 1, "#000000", 0.5f);
        cv.say("ALL ROUTES", 28, 396, 9, t.ink, 1, true, false, 1.8f, Left);
        cv.say(std::to_string(used) + " / " + std::to_string(kSlots), 230, 396, 9, t.dim, 1, false, true, 0.5f, Right);
        float y = 404;
        for (size_t i = 0; i < s.routes.size(); ++i)
        {
            const Route &rt = s.routes[i];
            if (rt.dst < 0 || y >= 860)
                continue;
            const bool sel = static_cast<int>(i) == sr;
            if (sel)
                cv.rect(22, y, 214, 22, t.accent, 0.12f);
            const float amt = (rt.amount - 0.5f) * 2;
            piece(cv, 34, y + 11, std::max(3.0f, 6 * std::sqrt(std::fabs(amt))), amt >= 0 ? 0 : 1, rt.on ? t.accent : t.dim, 1);
            std::string dn = destName(rt.dst);
            cv.say(u.srcs[static_cast<size_t>(rt.src)].name + "  ›  " + dn, 46, y + 15, 9.5f, sel ? t.accent : t.ink, rt.on ? 0.9f : 0.45f, sel, false, 0.3f, Left);
            y += 23;
        }
    }

    // the background of the page as the state has it: kept per page and oscillator engines, so a page drawn
    // once comes back at once
    // the background of the page as the state has it: kept per page and oscillator engines, so a page drawn
    // once comes back at once
    std::string baseKeyOf(const State &s)
    {
        std::string key = std::to_string(s.page);
        for (int o = 0; o < 4; ++o)
            key += std::to_string(partOf(s, o));
        if (s.page == 1) // the route knob, there or not
            key += s.selRoute >= 0 && s.routes[static_cast<size_t>(s.selRoute)].dst >= 0 ? "r" : "-";
        return key;
    }

    // the state differs from the preset's (the volume and the XY pad are not the preset's)
    bool changed(const Ui &u)
    {
        const State &a = u.st, &b = u.loaded;
        for (int o = 0; o < 4; ++o)
            for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                if (std::fabs(a.osc[o][p] - b.osc[o][p]) > 1e-5f)
                    return true;
        for (int p = 0; p < GLB_PARAM_COUNT; ++p)
            if (p != GLB_VOLUME && p != GLB_XY_X && p != GLB_XY_Y && std::fabs(a.glb[p] - b.glb[p]) > 1e-5f)
                return true;
        if (a.vecOn != b.vecOn || a.vecLoop != b.vecLoop || a.vecCount != b.vecCount || a.vecPath != b.vecPath || a.grainFile != b.grainFile)
            return true;
        for (size_t i = 0; i < a.routes.size(); ++i)
        {
            const Route &x = a.routes[i], &y = b.routes[i];
            if (x.dst != y.dst || (x.dst >= 0 && (x.src != y.src || x.on != y.on || std::fabs(x.amount - y.amount) > 1e-5f)))
                return true;
        }
        return false;
    }

    std::string upper(std::string s)
    {
        for (auto &ch : s)
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        return s;
    }

    // the list: the factory's in two columns, the person's after them
    struct PresetBox
    {
        float x, y, w, h;
        int index;
    };

    std::vector<PresetBox> presetBoxes(const Ui &u)
    {
        std::vector<PresetBox> v;
        int factory = 0, user = 0;
        for (size_t i = 0; i < u.presets.size(); ++i)
        {
            const bool bank = u.presets[i].bank >= 0;
            const int k = bank ? factory++ : user++;
            const int col = (bank ? 0 : 2) + k / 18, row = k % 18;
            if (col > 4)
                continue;
            v.push_back({310 + static_cast<float>(col) * 254, 170 + static_cast<float>(row) * 36, 242, 30, static_cast<int>(i)});
        }
        return v;
    }

    void presetList(Canvas &cv, Ui &u)
    {
        const panelkit::Theme &t = cv.theme;
        cv.rect(0, 0, W, H, "#000000", 0.55f);
        cv.rect(290, 100, 1296, 770, t.panelBottom);
        cv.rect(290, 100, 1296, 770, t.accent, 0.35f, false);
        cv.say("PRESETS", 310, 134, 12, t.ink, 1, true, false, 2.4f, Left);
        cv.say("click loads  ·  esc or a click outside: as it was", 1566, 134, 9, t.ink, 0.55f, false, false, 1, Right);
        cv.say("FACTORY", 310, 160, 9, t.dim, 1, false, false, 1.6f, Left);
        cv.say("YOURS  ·  " + userPresetDir(), 818, 160, 9, t.dim, 1, false, false, 0.8f, Left);
        bool anyUser = false;
        for (const PresetBox &b : presetBoxes(u))
        {
            const bool on = b.index == u.presetAt;
            anyUser |= u.presets[static_cast<size_t>(b.index)].bank < 0;
            cv.box(b.x, b.y, b.w, b.h, on);
            cv.say(upper(u.presets[static_cast<size_t>(b.index)].name), b.x + 12, b.y + 20, 10, on ? t.accent : t.ink, 1, on, false, 0.8f, Left);
        }
        if (!anyUser)
            cv.say("none yet: SAVE puts the sound here", 818, 192, 10, t.ink, 0.5f, false, false, 0.5f, Left);
    }

    // the XY pad's vector: the buttons, the corners (which oscillator, which engine), the path as it plays from
    // the pad's place, the newest voice on it, a path being drawn
    void vectorView(Canvas &cv, Ui &u, const synth::Sound &sound)
    {
        const State &s = u.st;
        const panelkit::Theme &t = cv.theme;
        const float bx = 1350, by = 648, bw = 222, bh = 120;
        struct B { float x, w; const char *txt; bool on; };
        const bool blink = (SDL_GetTicks() / 400) % 2 == 0;
        const B buttons[] = {{1440, 40, "VEC", s.vecOn}, {1484, 40, "REC", u.vecRec && (blink || u.recording)}, {1528, 44, "LOOP", s.vecLoop}};
        for (const B &b : buttons)
        {
            cv.box(b.x, 614, b.w, 20, b.on);
            cv.say(b.txt, b.x + b.w / 2, 628, 8, b.on ? t.accent : t.dim, 1, true, false, 1.2f, Centre);
        }
        if (!s.vecOn && !u.vecRec)
        {
            cv.say("X CUTOFF", 1356, 762, 9, t.dim, 1, false, false, 1.5f, Left);
            cv.say("Y RESO", 1566, 662, 9, t.dim, 1, false, false, 1.5f, Right);
            return;
        }
        auto at = [&](float x, float y) { return SDL_FPoint{bx + x * bw, by + (1 - y) * bh}; };
        const char *corner[4] = {"1 ", "2 ", "3 ", "4 "};
        const float cx[4] = {bx + 5, bx + bw - 5, bx + 5, bx + bw - 5}, cy[4] = {by + 13, by + 13, by + bh - 5, by + bh - 5};
        for (int o = 0; o < 4; ++o)
            cv.say(corner[o] + std::string(kPart[partOf(s, o)]), cx[o], cy[o], 8, t.dim, 1, true, false, 1, o % 2 ? Right : Left);
        // the path, from where the pad puts it
        const float ox = s.glb[GLB_XY_X] - 0.5f, oy = s.glb[GLB_XY_Y] - 0.5f;
        if (s.vecCount > 1)
        {
            std::vector<SDL_FPoint> pts;
            for (int i = 0; i < s.vecCount; ++i)
                pts.push_back(at(std::clamp(s.vecPath[static_cast<size_t>(i)][1] + ox, 0.0f, 1.0f), std::clamp(s.vecPath[static_cast<size_t>(i)][2] + oy, 0.0f, 1.0f)));
            cv.lines(pts, t.accent, 0.6f, 1.2f);
            for (const SDL_FPoint &pt : pts)
                cv.rect(pt.x - 1.5f, pt.y - 1.5f, 3, 3, t.accent, 0.9f);
        }
        if (u.recording && u.recPts.size() > 1)
        {
            std::vector<SDL_FPoint> pts;
            for (const auto &pt : u.recPts)
                pts.push_back(at(pt[1], pt[2]));
            cv.lines(pts, t.traceHi, 0.9f, 1.4f);
        }
        if (s.vecOn)
        {
            float x, y;
            sound.vectorNow(x, y);
            const SDL_FPoint p = at(x, y);
            piece(cv, p.x, p.y, 16, 2, t.ink, 0.5f); // the newest voice: in the ink, apart from the path's colour
            piece(cv, p.x, p.y, 6, 1, t.ink, 1.0f);
            cv.rect(p.x - 2, p.y - 2, 4, 4, t.ink);
        }
        if (u.vecRec && !u.recording)
            cv.say("drag on the pad: a path", bx + bw / 2, by + bh / 2 + 4, 9, t.ink, 0.8f, false, false, 0.5f, Centre);
    }

    std::string findBank(const char *argv0, const char *name)
    {
        // the working folder, by the program (a release: pcm/ beside it), the source tree from build/src/app (and
        // from build\src\app\Release on Windows)
        std::vector<std::filesystem::path> bases{std::filesystem::current_path(), std::filesystem::path(argv0).parent_path() / "../../.."};
        if (const char *exe = SDL_GetBasePath())
            for (const char *up : {".", "../../..", "../../../.."})
                bases.push_back(std::filesystem::path(exe) / up);
        for (const auto &base : bases)
        {
            auto f = base / "pcm" / name;
            if (std::filesystem::exists(f))
                return f.string();
        }
        return {};
    }

    // ── the sound: sound.cpp's engine, or the plugin through its link ──
    class Engine4D : public synth::panel::Engine
    {
    public:
        synth::Sound sound;
        std::function<void(const synth::io::Settings &)> begin; // the product's: every value to the engine, then its audio
        bool start(const std::vector<float> &, const synth::io::Settings &settings) override
        {
            begin(settings);
            return true;
        }
        bool connect(const char *link) override
        {
            const bool ok = sound.connect(link);
            begin(synth::io::loadSettings("obssynth4d")); // linked: no audio, no MIDI here (the banks: the tables' counts)
            return ok;
        }
        bool remote() const override { return sound.remote(); }
        bool remoteAlive() const override { return sound.remoteAlive(); }
        bool pollRemote(int &, float &) override { return false; } // the host's values: Synth4D::tick
        void setParam(int, float) override {}                     // the values: Synth4D::tick sends them
        void noteOn(int note, float velocity) override { sound.noteOn(note, velocity); }
        void noteOff(int note) override { sound.noteOff(note); }
        bool pollCc(int &channel, int &cc, float &value) override { return sound.pollCc(channel, cc, value); }
        synth::io::MidiIn &midiIn() override { return sound.midiIn(); }
        const synth::io::AudioOut &audioOut() const override { return sound.audioOut(); }
        bool reopenAudio(const synth::io::Settings &settings) override { return sound.reopenAudio(settings); }
    };

    class Synth4D : public synth::panel::Product
    {
    public:
        Synth4D(int argc, char **argv)
        {
            State &s = u_.st;
            for (int i = 1; i + 1 < argc; ++i)
                if (std::strcmp(argv[i], "--page") == 0 && std::strcmp(argv[i + 1], "matrix") == 0)
                    s.page = 1;
            for (int o = 0; o < 4; ++o)
                for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                    s.osc[o][p] = toNorm(OSC_RANGES[p], OSC_RANGES[p].def);
            for (int p = 0; p < GLB_PARAM_COUNT; ++p)
                s.glb[p] = toNorm(GLB_RANGES[p], GLB_RANGES[p].def);
            // a voice to start with: OSC 1 a saw, OSC 2 the morph table, OSC 3 FM and OSC 4 silent
            s.osc[0][OSC_LEVEL] = 0.8f;
            s.osc[1][OSC_PART_TYPE] = toNorm(OSC_RANGES[OSC_PART_TYPE], 1); // WT
            s.osc[1][OSC_LEVEL] = 0.7f;
            s.osc[1][OSC_WT_MORPH] = 0.38f;
            s.osc[2][OSC_PART_TYPE] = toNorm(OSC_RANGES[OSC_PART_TYPE], 2); // FM
            s.osc[2][OSC_LEVEL] = 0.0f;
            s.osc[2][OSC_FM_ALGORITHM] = 1.0f;
            s.osc[3][OSC_LEVEL] = 0.0f;
            // a start: LFO 1 slowly through OSC 2's tables; the mod wheel opens OSC 1 (nothing until it moves); LFO 2 on
            // OSC 2's pan and velocity on OSC 1's level, muted, waiting
            s.routes[0] = {0, 69, 0.53f, true}; // +12%: a few tables either way
            s.routes[1] = {19, 7, 0.6f, true};
            s.routes[2] = {1, 21, 0.4f, false};
            s.routes[3] = {17, 3, 0.7f, false};

            const char *bankNames[4] = {"bank_morph_32waves_1024samples_32float.wav", "bank_morph_32waves_1024samples_32float.wav",
                                        "bank_harmonic_16waves_1024samples_32float.wav", "bank_all_waves_32waves_1024samples_32float.wav"};
            for (size_t o = 0; o < 4; ++o)
            {
                banks_[o] = findBank(argv[0], bankNames[o]);
                u_.tables[o] = loadTable(banks_[o]);
            }
            // the grains' recording: --grain FILE, else the samples folder's, else the ALSA voice
            std::string grain;
            for (int i = 1; i + 1 < argc; ++i)
                if (std::strcmp(argv[i], "--grain") == 0)
                    grain = argv[i + 1];
            if (const char *base = SDL_GetBasePath()) // the shipped recordings (pcm/)
                synth::addSampleDirsBeside(base);
            if (grain.empty()) // the ALSA voice where it is (the samples folder, ALSA's), else the organ shipped in pcm/
            {
                grain = std::filesystem::exists(samplePath("alsa-rear-center.wav")) ? std::string("alsa-rear-center.wav")
                        : std::filesystem::exists("/usr/share/sounds/alsa/Rear_Center.wav") ? "/usr/share/sounds/alsa/Rear_Center.wav"
                                                                                             : "vcsl-pipe-organ-quiet-C4.wav";
            }
            u_.samples = listSamples();
            for (auto &f : s.grainFile)
                f = grain;
            defaultGrain_ = grain;
            if (auto src = synth::GrainSource::loadWav(samplePath(defaultGrain_)))
                for (int o = 0; o < 4; ++o)
                    overview(o, *src);
            themeName_ = panelkit::loadTheme(synth::io::loadSettings("obssynth4d").palette).name;
            u_.loaded = s;
            u_.sound = &sound_;
            engine_.begin = [this](const synth::io::Settings &settings) { begin(settings); };
            if (const char *flip = SDL_getenv("OBS_PANEL_FLIP"))
                flipEvery_ = static_cast<uint64_t>(std::atoi(flip));
            debug_ = SDL_getenv("OBS_PANEL_DEBUG") != nullptr;
        }

        const char *id() const override { return "obssynth4d"; }
        const char *title() const override { return "ObsSynth4D"; }
        const char *barName() const override { return crop() > 0 ? "" : "OBSSYNTH"; } // in a host: the host's window says it
        const char *barNumber() const override { return crop() > 0 ? "" : "4D"; }
        const char *startName() const override { return "START"; }
        float crop() const override { return sound_.remote() && u_.st.page == 0 ? HOST_CROP : 0.0f; }

        int params() const override { return 0; } // the state's own values: the controls point into it
        std::string paramName(int) const override { return {}; }
        float paramDefault(int) const override { return 0.5f; }
        std::string paramText(int, float) const override { return {}; }

        // the presets: the engine's bank, then the person's files (presets.hpp)
        std::vector<synth::panel::PresetEntry> presets() override
        {
            u_.presets = listPresets();
            std::vector<synth::panel::PresetEntry> v;
            for (const PresetEntry &e : u_.presets)
                v.push_back({e.name, e.path, e.bank});
            return v;
        }
        // loading one sets the state (the engine follows through send(), as for a knob), plus what the panel has no
        // control for, through the same queue
        bool loadPreset(const synth::panel::PresetEntry &e, std::vector<float> &, std::string &name) override
        {
            const PresetEntry pe{e.name, e.factory, e.path};
            if (!apply(pe))
                return false;
            if (!started_) // the tables' counts come with the engine: once more then
                pending_ = pe, pendingSet_ = true;
            name = u_.extras.name;
            return true;
        }
        bool savePreset(const std::string &name, const std::vector<float> &, std::string &path) override
        {
            const State &s = u_.st;
            PresetData d = u_.extras;
            d.name = name;
            for (int o = 0; o < 4; ++o)
                for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                    d.osc[o][p] = toValue(OSC_RANGES[p], s.osc[o][p]);
            for (int p = 0; p < GLB_PARAM_COUNT; ++p)
                d.glb[p] = toValue(GLB_RANGES[p], s.glb[p]);
            d.routes.clear();
            for (const Route &rt : s.routes)
                if (rt.dst >= 0 && rt.on) // a muted route: the format has no "muted", it stays out
                    d.routes.push_back({rt.src, rt.dst, (rt.amount - 0.5f) * 4});
            for (int o = 0; o < 4; ++o) // a name in the samples folder when it is there
            {
                const std::string &f = s.grainFile[static_cast<size_t>(o)];
                const std::string dir = samplesDir() + "/";
                d.grainFile[static_cast<size_t>(o)] = f.rfind(dir, 0) == 0 ? f.substr(dir.size()) : f;
            }
            d.vecOn = s.vecOn;
            d.vecLoop = s.vecLoop;
            d.vecPath.assign(s.vecPath.begin(), s.vecPath.begin() + s.vecCount);
            if (!obs::savePreset(d, tables_, path))
                return false;
            u_.extras = d;
            u_.loaded = s;
            return true;
        }
        bool modified() override { return changed(u_); }

        std::vector<ccov::Map> controllers() const override { return factoryControllers(); }
        ccov::Target target(const std::string &id) override
        {
            u_.ov = host ? host->overlay : nullptr;
            return ccResolve(u_, id);
        }
        std::vector<std::string> aliases(const std::string &id) override { return ccAliases(u_.st, id); }
        synth::panel::Engine &engine() override { return engine_; }

        // the backgrounds: one per page and oscillator engines; the other page's drawn ahead
        std::string layoutKey(const std::vector<float> &) const override { return baseKeyOf(u_.st); }
        std::vector<std::string> layoutKeys() const override
        {
            State other = u_.st;
            other.page = 1 - other.page;
            return {baseKeyOf(other)};
        }
        std::vector<Control> layout(std::vector<float> &) override
        {
            u_.ov = host ? host->overlay : nullptr;
            if (host)
                u_.hover = host->pointer;
            ctls_ = layoutOf(u_, binds_);
            return ctls_;
        }
        std::string bodySvg(const panelkit::Theme &t, const std::string &key) override
        {
            themeName_ = t.name;
            Ui page;
            page.st = stateOf(key);
            std::vector<Bind> binds;
            return bodyOf(t, page.st, layoutOf(page, binds));
        }

        // the status line: the palette, the oscillator MAIN shows
        std::string status() override { return upper(themeName_) + "  ·  OSC " + std::to_string(u_.st.selOsc + 1); }

        void drawBody(Canvas &cv, const std::vector<float> &, const synth::panel::Clip &) override
        {
            Ui &u = u_;
            const synth::Sound &sound = sound_;
            State &s = u.st;
            const panelkit::Theme &t = cv.theme;
            // the top bar and the play column
            cv.say("MAIN", 330, 35, 12, s.page == 0 ? t.ink : t.dim, 1, true, false, 2.5f, Centre);
            cv.say("MOD MATRIX", 460, 35, 12, s.page == 1 ? t.ink : t.dim, 1, true, false, 2.5f, Centre);
            cv.rect(s.page == 0 ? 310.0f : 440.0f, 44, 40, 2, t.accent);
            cv.say("MASTER", 1430, 34, 10, t.ink, 1, true, false, 1.6f, Right);
            if (s.page == 0)
                cv.say("PLAY", 28, 94, 12, t.ink, 1, true, false, 2.4f, Left);
            struct Line { const char *txt; bool strong; };
            const Line help[] = {{"Z S X D C V G B H N J M", true}, {"an octave from C3", false}, {"Q 2 W 3 E R 5 T 6 Y 7 U", true}, {"the next", false}, {"", false},
                                 {"MIDI IN", true}, {"ObsSynth4D:input", false}, {"gridseq joins it", false}, {"", false},
                                 {"DRAG", true}, {"turns (SHIFT: fine)", false}, {"WHEEL", true}, {"turns", false}, {"DOUBLE CLICK", true}, {"default", false},
                                 {"TAB", true}, {"the other page", false}, {"", false},
                                 {"THE RINGS BY LFO, ENV, XY", true}, {"drag one onto a knob", false}, {"CTRL + WHEEL", true}, {"a knob's modulation", false},
                                 {"RIGHT CLICK, HALO", true}, {"its routes in the matrix", false}};
            for (size_t i = 0; i < std::size(help) && s.page == 0; ++i)
                cv.say(help[i].txt, 30, 126 + static_cast<float>(i) * 22, 11, t.ink, help[i].strong ? 1.0f : 0.55f, help[i].strong, false, 0.5f, Left);

            if (s.page == 0)
            {
                struct Title { const char *txt; float x, y; };
                const Title titles[] = {{"FILTER", 256, 350}, {"FILTER ENV", 736, 350}, {"AMP ENV", 1046, 350}, {"LFO", 1356, 350}, {"MIXER", 256, 606},
                                        {"TUBE", 596, 606}, {"DELAY", 816, 606}, {"MASTER FILTER", 1116, 606}, {"XY", 1336, 606}};
                for (const Title &tt : titles)
                    cv.say(tt.txt, tt.x + 14, tt.y + 22, 12, t.ink, 1, true, false, 2.4f, Left);
                cv.say("OSC", 552, 372, 10, t.dim, 1, false, false, 1.5f, Right);
                cv.say("OSC " + std::to_string(s.selOsc + 1), 1022, 372, 10, t.dim, 1, false, false, 1.5f, Right);
                cv.say("OSC " + std::to_string(s.selOsc + 1), 1332, 372, 10, t.dim, 1, false, false, 1.5f, Right);
                cv.say("ROUTES: MOD MATRIX ›", 1470, 572, 9, t.dim, 1, false, false, 1.2f, Centre);
                for (int o = 0; o < 4; ++o)
                {
                    const float x = 256 + static_cast<float>(o) * 335, y = 72;
                    const int part = partOf(s, o);
                    cv.say("OSC " + std::to_string(o + 1), x + 14, y + 22, 12, t.ink, 1, true, false, 2.4f, Left);
                    cv.say(kPart[part], x + 311, y + 22, 10, t.dim, 1, false, false, 1.5f, Right);
                    cv.led(x + 255, y + 17, s.osc[o][OSC_LEVEL] > 0.001f);
                    if (part == 0)
                    {
                        const int wave = static_cast<int>(std::lround(toValue(OSC_RANGES[OSC_WAVE], s.osc[o][OSC_WAVE])));
                        for (int i = 0; i < 6; ++i)
                        {
                            const float a = static_cast<float>(-115 + i * 46) * static_cast<float>(M_PI) / 180;
                            waveIcon(cv, x + 72 + 46 * std::sin(a) - 7, y + 146 - 46 * std::cos(a), i, i == wave);
                        }
                    }
                    else if (part == 1)
                    {
                        const Table &tb = u.tables[static_cast<size_t>(o)];
                        const float dx = x + 14, dy = y + 80, dw = 297, dh = 96;
                        cv.say("◀ " + tb.name + " ▶", dx + 8, dy + 14, 9, t.accent, 0.7f, false, true, 2, Left);
                        if (!tb.frames.empty())
                        {
                            const auto &fr = tb.frames;
                            const int shown = std::min<int>(16, static_cast<int>(fr.size()));
                            const int cur = static_cast<int>(std::lround(s.osc[o][OSC_WT_MORPH] * static_cast<float>(fr.size() - 1)));
                            const float fw = dw * 0.62f, amp = 15;
                            auto plot = [&](const std::vector<float> &f, float bx, float by, const std::string &hex, float a, float thick)
                            {
                                std::vector<SDL_FPoint> pts(f.size());
                                for (size_t j = 0; j < f.size(); ++j)
                                    pts[j] = {bx + static_cast<float>(j) * fw / static_cast<float>(f.size() - 1), by - amp * f[j]};
                                cv.lines(pts, hex, a, thick);
                            };
                            const float span = static_cast<float>(std::max(1, shown - 1));
                            for (int k = shown - 1; k >= 0; --k)
                            {
                                const size_t fi = static_cast<size_t>(std::lround(k * static_cast<double>(fr.size() - 1) / static_cast<double>(span)));
                                const float kk = static_cast<float>(k);
                                plot(fr[fi], dx + 12 + kk * (dw - fw - 24) / span, dy + dh - 24 - kk * (dh - 52) / span, t.trace, 0.25f + 0.45f * (1 - kk / static_cast<float>(shown)), 1);
                            }
                            // two frames: the knob's, and the one playing (the knob plus the matrix, as the grains show it)
                            const float last = static_cast<float>(fr.size() - 1);
                            const float pos = s.osc[o][OSC_WT_MORPH];
                            const Mod m = modOf(s, sound, 68 + o);
                            const float livePos = m.routes > 0 ? std::clamp(pos + m.live, 0.0f, 1.0f) : pos;
                            const int live = static_cast<int>(std::lround(livePos * last));
                            auto at = [&](int f, const std::string &hex, float a, float thick)
                            {
                                const float kk = static_cast<float>(f) * span / std::max(1.0f, last);
                                plot(fr[static_cast<size_t>(f)], dx + 12 + kk * (dw - fw - 24) / span, dy + dh - 24 - kk * (dh - 52) / span, hex, a, thick);
                            };
                            if (m.routes > 0 && live != cur)
                                at(cur, t.ink, 0.9f, 1.8f); // where the knob is
                            at(live, t.accent, 1, 2.2f);    // what sounds
                            // the strip: the whole bank, the knob's mark and the playing one
                            const float sx = dx + 12, sw = dw - 24, sy = dy + dh - 7;
                            cv.rect(sx, sy, sw, 2, t.trace, 0.35f);
                            cv.rect(std::round(sx + pos * sw) - 1, sy - 3, 2, 8, t.ink, 0.8f);
                            if (m.routes > 0)
                                cv.rect(std::round(sx + livePos * sw) - 2, sy - 2, 4, 6, t.accent);
                            cv.say(m.routes > 0 && live != cur ? fmt("%02.0f", cur + 1.0) + " → " + fmt("%02.0f", live + 1.0) + " / " + std::to_string(fr.size())
                                                                  : fmt("%02.0f", live + 1.0) + " / " + std::to_string(fr.size()),
                                dx + dw - 8, dy + 14, 9, t.accent, 0.8f, false, true, 2, Right, false);
                        }
                    }
                    else if (part == 3)
                    {
                        // the recording: its shape, the spray's band, the position (where the matrix has it now: the halo's mark)
                        const float dx = x + 14, dy = y + 80, dw = 297, dh = 96, mid = dy + dh * 0.58f;
                        cv.say("◀ ▶ " + upper(u.grainName[static_cast<size_t>(o)]), dx + 8, dy + 14, 8, t.accent, 0.7f, false, true, 1, Left);
                        const float size = toValue(OSC_RANGES[OSC_GR_SIZE], s.osc[o][OSC_GR_SIZE]), dens = toValue(OSC_RANGES[OSC_GR_DENSITY], s.osc[o][OSC_GR_DENSITY]);
                        cv.say(fmt("%.0f ms", static_cast<double>(size) * 1000) + " " + fmt("%.0f/s", static_cast<double>(dens)), dx + dw - 8, dy + 14, 8, t.accent, 0.8f, false, true, 0.5f, Right, false);
                        const auto &pk = u.grainPeaks[static_cast<size_t>(o)];
                        for (size_t k = 0; k < pk.size(); ++k)
                        {
                            const float h = std::max(1.0f, pk[k] * (dh - 34) * 0.5f);
                            cv.rect(dx + 4 + static_cast<float>(k), mid - h, 1, 2 * h, t.trace, 0.55f);
                        }
                        const float pos = s.osc[o][OSC_WT_MORPH], spray = s.osc[o][OSC_GR_SPRAY];
                        const float px = dx + 4 + pos * (dw - 8), sw = spray * (dw - 8);
                        cv.rect(std::max(dx + 4, px - sw / 2), dy + 22, std::min(sw, dw - 8), dh - 28, t.accent, 0.13f);
                        cv.rect(std::round(px), dy + 22, 1.5f, dh - 28, t.accent);
                        if (const Mod m = modOf(s, sound, 68 + o); m.routes > 0)
                        {
                            const float lx = dx + 4 + std::clamp(pos + m.live, 0.0f, 1.0f) * (dw - 8);
                            cv.rect(std::round(lx) - 1, dy + 22, 3, 6, t.traceHi);
                        }
                    }
                    else if (part == 4)
                        superView(cv, x + 14, y + 80, 297, 96, s.osc[o], modOf(s, sound, 68 + o));
                    else
                    {
                        const int ai = static_cast<int>(std::lround(toValue(OSC_RANGES[OSC_FM_ALGORITHM], s.osc[o][OSC_FM_ALGORITHM])));
                        const float dx = x + 14, dy = y + 80, dw = 297, dh = 96;
                        cv.say("◀ ALGO " + std::to_string(ai + 1) + " ▶", dx + 8, dy + 14, 9, t.accent, 0.7f, false, true, 2, Left);
                        cv.say(kAlgo[ai], dx + 8, dy + 28, 9, t.accent, 0.9f, false, true, 2, Left);
                        for (int k = 0; k < 5; ++k)
                            cv.rect(dx + 10 + static_cast<float>(k) * 13, dy + dh - 16, 9, 6, k == ai ? t.accent : t.btnOff);
                        const int rows = ai == 2 ? 5 : ai == 4 ? 4 : ai == 3 ? 3 : 2;
                        const float step = std::min(16.0f, (dh - 26) / static_cast<float>(rows));
                        algoDiagram(cv, dx + dw * 0.52f, dy + dh - 20, ai, std::min(13.0f, step * 0.8f), step);
                        cv.say("M ×" + two(toValue(OSC_RANGES[OSC_FM_MOD_RATIO], s.osc[o][OSC_FM_MOD_RATIO])), dx + dw - 8, dy + 40, 9, t.accent, 0.8f, false, true, 1, Right, false);
                        cv.say("2 ×" + two(toValue(OSC_RANGES[OSC_FM_OP2_RATIO], s.osc[o][OSC_FM_OP2_RATIO])), dx + dw - 8, dy + 54, 9, t.accent, 0.8f, false, true, 1, Right, false);
                        cv.say("FB " + two(s.osc[o][OSC_FM_FEEDBACK]), dx + dw - 8, dy + 68, 9, t.accent, 0.8f, false, true, 1, Right, false);
                    }
                }
                // the scope: the last samples played, from a rising zero crossing so the trace stands still
                float sc[512];
                sound.scope(sc, 512);
                int start = 0;
                for (int i = 1; i < 256; ++i)
                    if (sc[i - 1] < 0 && sc[i] >= 0)
                    {
                        start = i;
                        break;
                    }
                std::vector<SDL_FPoint> pts;
                for (int i = 0; i < 256; ++i)
                    pts.push_back({1352 + static_cast<float>(i) * 218 / 256, 824 - std::clamp(sc[start + i], -1.0f, 1.0f) * 40});
                cv.lines(pts, t.traceHi, 0.9f, 1.4f);
                vectorView(cv, u, sound);
            }
            else
            {
                matrixUnder(cv, u, sound);
                matrixInspector(cv, u, sound);
            }
        }

        bool drawControl(Canvas &cv, const Control &c, float n) override
        {
            Ui &u = u_;
            const State &s = u.st;
            const synth::Sound &sound = sound_;
            const panelkit::Theme &t = cv.theme;
            const Bind none{};
            const Bind &bind = c.tag >= 0 && static_cast<size_t>(c.tag) < binds_.size() ? binds_[static_cast<size_t>(c.tag)] : none;
            switch (c.kind)
            {
            case Kind::Knob:
            {
                char key[32];
                std::snprintf(key, sizeof key, "cap4d%ld", std::lround(c.a * 10));
                SDL_Texture *cap = pieceOf(cv, key, capSvg(c.a), c.a * 2 + 2, c.a * 2 + 2);
                const float size = (c.a * 2 + 2) * cv.scale;
                const SDL_FRect d{c.x * cv.scale - size / 2, c.y * cv.scale - size / 2, size, size};
                SDL_RenderTextureRotated(cv.r, cap, nullptr, &d, -135.0 + 270.0 * static_cast<double>(n), nullptr, SDL_FLIP_NONE);
                // the halo: what the matrix does to this knob, how far (the band) and where it is now (the bright mark)
                if (const int dst = reachOf(s, bind); dst >= 0)
                    if (const Mod m = modOf(s, sound, dst); m.routes > 0)
                    {
                        const float hr = c.a + 12, from = -135 + 270 * n;
                        const float to = -135 + 270 * haloNorm(s, bind, n, dst, m.span);
                        const float at = -135 + 270 * haloNorm(s, bind, n, dst, m.live);
                        arc(cv, c.x, c.y, hr, 3, -135, 135, t.display, 0.9f);
                        arc(cv, c.x, c.y, hr, 3, from, to, t.accent, 0.55f);
                        arc(cv, c.x, c.y, hr, 5, at - 5, at + 5, t.traceHi, 1);
                    }
                cv.say(c.label, c.x, c.y + c.a + 22, 10, t.ink, 1, true, false, 1.6f, Centre);
                if (c.say) // held: the value bright
                    cv.say(c.say(n), c.x, c.y + c.a + (c.label.empty() ? 22 : 35), 10, c.held ? t.ink : t.accent, 1, c.held, true, 0.5f, Centre, false);
                return true;
            }
            case Kind::Fader:
            {
                const float ky = c.y + c.a * (1 - n);
                SDL_Texture *cap = pieceOf(cv, "fadercap4d",
                                           panelkit::svgOpen(22, 14) +
                                               "<defs><linearGradient id='c' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='#e9e9e6'/><stop offset='1' stop-color='#6b6b68'/></linearGradient></defs>"
                                               "<rect x='.5' y='.5' width='21' height='13' rx='2' fill='url(#c)' stroke='#111'/><line x1='2' y1='7' x2='20' y2='7' stroke='#f8f8f4' stroke-width='1.5'/></svg>",
                                           22, 14);
                // the halo of a fader: a band beside it
                if (const int dst = reachOf(s, bind); dst >= 0)
                    if (const Mod m = modOf(s, sound, dst); m.routes > 0)
                    {
                        const float to = c.y + c.a * (1 - haloNorm(s, bind, n, dst, m.span));
                        const float at = c.y + c.a * (1 - haloNorm(s, bind, n, dst, m.live));
                        cv.rect(c.x + 9, std::min(ky, to), 3, std::fabs(to - ky), t.accent, 0.55f);
                        cv.rect(c.x + 8, at - 2, 5, 4, t.traceHi);
                    }
                const SDL_FRect d{(c.x - 11) * cv.scale, (ky - 7) * cv.scale, 22 * cv.scale, 14 * cv.scale};
                SDL_RenderTexture(cv.r, cap, nullptr, &d);
                cv.say(c.label, c.x, c.y + c.a + 20, 10, t.ink, 1, true, false, 1.6f, Centre);
                if (c.say && c.held)
                    cv.say(c.say(n), c.x + 16, ky + 4, 10, t.ink, 1, true, true, 0.5f, Left, false);
                return true;
            }
            case Kind::Cell:
            {
                const float cx = c.x + c.a / 2, cy = c.y + c.b / 2;
                const int k = slotAt(s, bind.index, bind.param);
                if (k < 0)
                {
                    cv.rect(std::round(cx) - 1, std::round(cy) - 1, 2, 2, t.dim, 0.4f);
                    return true;
                }
                const Route &rt = s.routes[static_cast<size_t>(k)];
                const float amt = (rt.amount - 0.5f) * 2;
                const float live = rt.on ? std::min(1.0f, std::fabs(sound.source(rt.src) * amt) * 1.5f) : 0.0f;
                const float most = std::min(9.5f, c.b / 2 - 1.5f);
                if (live > 0.02f)
                    piece(cv, cx, cy, 16, 2, t.traceHi, live * 0.8f);
                piece(cv, cx, cy, most, 1, rt.on ? t.accent : t.dim, 0.22f); // the most it could be
                piece(cv, cx, cy, std::max(4.5f, most * std::sqrt(std::fabs(amt))), amt >= 0 ? 0 : 1, rt.on ? t.accent : t.dim, 1);
                if (k == s.selRoute)
                    cv.rect(c.x + 3, c.y + 1, c.a - 6, c.b - 2, t.accent, 0.9f, false);
                return true;
            }
            case Kind::Button: // which oscillator, which LFO
                cv.box(c.x, c.y, c.a, c.b, c.on);
                cv.say(c.label, c.x + c.a / 2, c.y + 14, 10, c.on ? t.accent : t.dim, 1, true, false, 0, Centre);
                return true;
            default: return false; // switches, LEDs, the XY pad: the SDK's (as the 4D had them)
            }
        }

        // over the controls: the sources' handles, a source being dragged, the presets' list
        void drawOver(Canvas &cv, const synth::panel::Clip &) override
        {
            Ui &u = u_;
            const State &s = u.st;
            const panelkit::Theme &t = cv.theme;
            const std::vector<Control> &ctls = ctls_;
            // MAIN: the sources' handles; while one is dragged, where it can land and the line to the pointer
            if (s.page == 0)
            {
                for (const Handle &h : handles(s))
                {
                    const bool hot = std::hypot(u.hover.x - h.x, u.hover.y - h.y) < 10;
                    drawHandle(cv, h, hot || u.modDrag == h.src);
                    if (hot && u.modDrag < 0)
                        cv.say("drag " + h.label + " onto a knob", h.x, h.y - 14, 9, t.ink, 0.9f, false, false, 0.5f, Centre);
                }
                if (u.modDrag >= 0)
                {
                    for (const Control &c : ctls)
                        if (c.tag < 0 || reachOf(s, binds_[static_cast<size_t>(c.tag)]) < 0)
                            continue;
                        else if (c.kind == Kind::Knob)
                        {
                            const bool over = std::hypot(u.hover.x - c.x, u.hover.y - c.y) <= c.a + 16;
                            arc(cv, c.x, c.y, c.a + 12, over ? 4.0f : 2.0f, -135, 135, t.accent, over ? 0.9f : 0.3f);
                        }
                        else if (c.kind == Kind::Fader)
                            cv.rect(c.x + 9, c.y, 3, c.a, t.accent, 0.3f);
                    for (const Handle &h : handles(s))
                        if (h.src == u.modDrag)
                            cv.lines({{h.x, h.y}, u.hover}, t.accent, 0.8f, 1.5f);
                    cv.say(u.srcs[static_cast<size_t>(u.modDrag)].name, u.hover.x + 12, u.hover.y - 8, 10, t.accent, 1, true, false, 1, Left);
                }
            }
            if (u.picking)
            {
                u.presetAt = host ? host->presetAt() : -1;
                presetList(cv, u);
            }
        }

        bool clickTitle() override
        {
            u_.picking = !u_.picking;
            return true;
        }
        bool overOpen() const override { return u_.picking; }
        void closeOver() override { u_.picking = false; }

        // a press, before the controls: the list takes it (a preset, or outside it closes); MAIN: a source's handle
        // starts a drag; a knob's halo, or a right click on a knob the matrix can reach, goes to its column there
        bool press(float x, float y, int button) override
        {
            Ui &u = u_;
            State &s = u.st;
            if (u.picking)
            {
                for (const PresetBox &b : presetBoxes(u))
                    if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h && host)
                        host->loadPreset(b.index);
                u.picking = false;
                return true;
            }
            if (s.page != 0)
                return false;
            if (button == SDL_BUTTON_LEFT)
                for (const Handle &h : handles(s))
                    if (std::hypot(x - h.x, y - h.y) < 11)
                    {
                        u.modDrag = h.src;
                        return true;
                    }
            int to = -1;
            for (const Control &c : ctls_)
            {
                if (c.kind != Kind::Knob || c.tag < 0)
                    continue;
                const float d = std::hypot(x - c.x, y - c.y);
                const int dst = reachOf(s, binds_[static_cast<size_t>(c.tag)]);
                if (dst < 0)
                    continue;
                if ((button == SDL_BUTTON_RIGHT && d <= c.a + 16) || (button == SDL_BUTTON_LEFT && d > c.a + 7 && d <= c.a + 17 && modOf(s, sound_, dst).routes > 0))
                    to = dst;
            }
            if (to < 0)
                return false;
            s.page = 1;
            s.mxPage = pageOf(to);
            u.flashDst = to;
            u.flashUntil = SDL_GetTicks() + 1500;
            for (size_t i = 0; i < s.routes.size(); ++i)
                if (s.routes[i].dst == to)
                {
                    s.selRoute = static_cast<int>(i);
                    break;
                }
            return true;
        }

        // a release: a source dropped on a knob or a fader the matrix reaches (that route, a new one at +50% if there
        // was none); a path drawn on the pad (sixteen points of it, evenly in time; the pad back to the middle)
        void release(float x, float y) override
        {
            Ui &u = u_;
            State &s = u.st;
            if (u.modDrag >= 0)
            {
                int dst = -1;
                for (const Control &c : ctls_)
                    if (c.tag >= 0 && ((c.kind == Kind::Knob && std::hypot(x - c.x, y - c.y) <= c.a + 16) ||
                                       (c.kind == Kind::Fader && std::fabs(x - c.x) <= 14 && y >= c.y - 8 && y <= c.y + c.a + 8)))
                        dst = reachOf(s, binds_[static_cast<size_t>(c.tag)]);
                if (dst >= 0)
                {
                    int slot = slotAt(s, u.modDrag, dst);
                    for (size_t i = 0; i < s.routes.size() && slot < 0; ++i)
                        if (s.routes[i].dst < 0)
                        {
                            s.routes[i] = {u.modDrag, dst, 0.625f, true};
                            slot = static_cast<int>(i);
                        }
                    if (slot >= 0)
                        s.selRoute = slot;
                }
                u.modDrag = -1;
            }
            if (u.recording)
            {
                const size_t n = u.recPts.size();
                const size_t m = std::min<size_t>(n, 16);
                for (size_t i = 0; i < m; ++i)
                    s.vecPath[i] = u.recPts[m > 1 ? i * (n - 1) / (m - 1) : 0];
                s.vecCount = static_cast<int>(m);
                s.vecOn = m > 0;
                s.glb[GLB_XY_X] = s.glb[GLB_XY_Y] = 0.5f;
                u.recording = u.vecRec = false;
            }
        }

        // Ctrl + wheel on a knob (MAIN): its modulation, the chosen route if it reaches the knob, else the first that does
        bool wheel(float x, float y, float dy) override
        {
            State &s = u_.st;
            if (s.page != 0 || !(SDL_GetModState() & SDL_KMOD_CTRL))
                return false;
            const bool fine = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            for (size_t i = ctls_.size(); i-- > 0;)
            {
                const Control &c = ctls_[i];
                const bool on = c.kind == Kind::Knob ? std::hypot(x - c.x, y - c.y) <= c.a + 6
                                : c.kind == Kind::Fader ? std::fabs(x - c.x) <= 14 && y >= c.y - 8 && y <= c.y + c.a + 8
                                                        : false;
                if (!on || c.tag < 0)
                    continue;
                const int reach = reachOf(s, binds_[static_cast<size_t>(c.tag)]);
                if (reach < 0)
                    return false;
                int k = s.selRoute >= 0 && s.routes[static_cast<size_t>(s.selRoute)].dst == reach ? s.selRoute : -1;
                for (size_t r = 0; r < s.routes.size() && k < 0; ++r)
                    if (s.routes[r].dst == reach)
                        k = static_cast<int>(r);
                if (k >= 0)
                {
                    float &a = s.routes[static_cast<size_t>(k)].amount;
                    a = std::clamp(a + dy * (fine ? 0.001f : 0.01f), 0.0f, 1.0f);
                    s.selRoute = k;
                }
                return true;
            }
            return false;
        }

        // the XY pad pressed with REC armed: this drag is the path
        void pressed(const Control &c) override
        {
            if (c.kind != Kind::XY || !u_.vecRec)
                return;
            u_.recording = true;
            u_.recStart = u_.recLast = SDL_GetTicks();
            u_.recPts.clear();
        }

        // Tab: the other page
        bool key(SDL_Scancode sc, bool down) override
        {
            if (sc != SDL_SCANCODE_TAB)
                return false;
            if (down)
                u_.st.page = 1 - u_.st.page;
            return true;
        }

        void tick() override
        {
            Ui &u = u_;
            State &s = u.st;
            u.ov = host ? host->overlay : nullptr;
            const uint64_t now = SDL_GetTicks();
            if (sound_.remote()) // what the host changed: shown, not sent back
            {
                int id;
                float v;
                while (sound_.pollRemote(id, v))
                    if (id < 4 * OSC_PARAM_COUNT)
                        s.osc[id / OSC_PARAM_COUNT][id % OSC_PARAM_COUNT] = sent_.osc[id / OSC_PARAM_COUNT][id % OSC_PARAM_COUNT] = v;
                    else if (id < 4 * OSC_PARAM_COUNT + GLB_PARAM_COUNT)
                        s.glb[id - 4 * OSC_PARAM_COUNT] = sent_.glb[id - 4 * OSC_PARAM_COUNT] = v;
                std::string whole;
                if (sound_.pollWhole(whole)) // the plugin's state (on opening, a program, a session): as a preset loads
                    takeWhole(whole);
            }
            if (u.recording) // the path drawn on the pad: a point every 30 ms it moves
            {
                const std::array<float, 3> pt{static_cast<float>(now - u.recStart) / 1000.0f, s.glb[GLB_XY_X], s.glb[GLB_XY_Y]};
                if (u.recPts.empty())
                    u.recPts.push_back({0.0f, pt[1], pt[2]});
                else if (now - u.recLast >= 30 && u.recPts.size() < 1000 && (pt[1] != u.recPts.back()[1] || pt[2] != u.recPts.back()[2]))
                {
                    u.recLast = now;
                    u.recPts.push_back(pt);
                }
            }
            send();
            // OBS_PANEL_FLIP: the page flipped where a click would flip it; the frames after it timed (tick to tick)
            if (flipEvery_)
            {
                const uint64_t ns = SDL_GetTicksNS();
                if (flipFrames_ < 3 && lastTickNs_)
                {
                    ++flipFrames_;
                    std::fprintf(stderr, "[flip] page %d frame %d %.1f ms\n", s.page, flipFrames_, static_cast<double>(ns - lastTickNs_) / 1e6);
                }
                lastTickNs_ = ns;
                if (now - lastFlip_ > flipEvery_)
                {
                    std::fprintf(stderr, "[flip] page %d, %s\n", s.page, sound_.debugLine().c_str());
                    s.page = 1 - s.page;
                    lastFlip_ = now;
                    flipFrames_ = 0;
                }
            }
            if (debug_ && now - lastDebug_ > 1000)
            {
                lastDebug_ = now;
                std::fprintf(stderr, "[obssynth4d] %s\n", sound_.debugLine().c_str());
            }
        }

        // what the panel draws again by itself: what the state shows beyond the controls, what moves (the mod
        // sources: the halos, the matrix; the scope; the vector), what the pointer touches (the matrix's crosshair,
        // a handle's hint, a source dragged)
        std::vector<SDL_FRect> live() override
        {
            Ui &u = u_;
            const State &s = u.st, &d = drawn_;
            std::vector<SDL_FRect> r;
            const SDL_FRect all{0, 0, W, H}, grid{256, 72, 1330, 814}, pad{1350, 648, 222, 120};
            bool routes = false;
            for (size_t i = 0; i < s.routes.size(); ++i)
            {
                const Route &a = s.routes[i], &b = d.routes[i];
                routes |= a.src != b.src || a.dst != b.dst || a.amount != b.amount || a.on != b.on;
            }
            if (routes || s.page != d.page || s.selOsc != d.selOsc || s.selLfo != d.selLfo || s.mxPage != d.mxPage || s.selRoute != d.selRoute ||
                s.grpOpen != d.grpOpen || s.vecOn != d.vecOn || s.vecLoop != d.vecLoop || s.vecCount != d.vecCount || s.vecPath != d.vecPath ||
                u.grainName != drawnGrain_ || u.picking != drawnPicking_ || u.vecRec != drawnRec_ || u.modDrag >= 0 || drawnDrag_ >= 0)
                r.push_back(all);
            else if (s.page == 0)
            {
                for (int o = 0; o < 4; ++o)
                    if (std::memcmp(s.osc[o], d.osc[o], sizeof s.osc[o]) != 0)
                    {
                        r.push_back({256 + static_cast<float>(o) * 335, 72, 325, 268});
                        if (o == s.selOsc)
                            r.push_back({256, 350, 1330, 244});
                    }
                if (std::memcmp(s.glb, d.glb, sizeof s.glb) != 0)
                    r.push_back({256, 350, 1330, 536});
            }
            // what moves by itself
            std::vector<float> moving(64 + 2);
            for (int i = 0; i < 64; ++i)
                moving[static_cast<size_t>(i)] = std::round(sound_.source(i) * 200.0f);
            sound_.vectorNow(moving[64], moving[65]);
            std::vector<float> scope(512);
            sound_.scope(scope.data(), 512);
            for (float &v : scope)
                v = std::round(v * 100.0f);
            if (s.page == 0 && scope != drawnScope_)
                r.push_back({1350, 778, 222, 92});
            if (moving != drawnMoving_)
            {
                if (s.page == 1) // the sources' meters, the destinations', the routes' cells, the chosen route's meter
                {
                    r.push_back({370, 168, 44, 690});
                    r.push_back({MX0, 153, MX1 - MX0, 9});
                    r.push_back({22, 296, 214, 20});
                    for (const Control &c : ctls_)
                        if (c.kind == Kind::Cell && c.tag >= 0 && slotAt(s, binds_[static_cast<size_t>(c.tag)].index, binds_[static_cast<size_t>(c.tag)].param) >= 0)
                            r.push_back({c.x, c.y, c.a, c.b});
                }
                else if (s.vecOn)
                    r.push_back(pad);
            }
            // MAIN: a halo or a display whose modulation moved
            std::vector<float> halos;
            if (s.page == 0)
            {
                for (const Control &c : ctls_)
                {
                    if (c.tag < 0 || (c.kind != Kind::Knob && c.kind != Kind::Fader))
                        continue;
                    const int dst = reachOf(s, binds_[static_cast<size_t>(c.tag)]);
                    const Mod m = dst >= 0 ? modOf(s, sound_, dst) : Mod{};
                    halos.push_back(m.routes > 0 ? std::round(m.live * 400.0f) : -1e9f);
                    const size_t k = halos.size() - 1;
                    if (k >= drawnHalos_.size() || drawnHalos_[k] != halos[k])
                        r.push_back(c.kind == Kind::Knob ? SDL_FRect{c.x - c.a - 16, c.y - c.a - 16, 2 * c.a + 32, 2 * c.a + 32} : SDL_FRect{c.x + 7, c.y - 3, 7, c.a + 6});
                }
                for (int o = 0; o < 4; ++o)
                {
                    const int part = partOf(s, o);
                    const Mod m = part == 1 || part >= 3 ? modOf(s, sound_, 68 + o) : Mod{};
                    halos.push_back(m.routes > 0 ? std::round(m.live * 400.0f) : -1e9f);
                    const size_t k = halos.size() - 1;
                    if (k >= drawnHalos_.size() || drawnHalos_[k] != halos[k])
                        r.push_back({270 + static_cast<float>(o) * 335, 152, 297, 96});
                }
            }
            if (s.page == 0 && (u.vecRec || u.recording)) // REC blinks; the path being drawn
                r.push_back({1350, 610, 222, 158});
            // the pointer: the matrix's crosshair, a handle's hint
            std::vector<int> pointed;
            if (s.page == 1)
            {
                const auto cols = mxCols(s.mxPage, s.mxPage < 4 ? partOf(s, s.mxPage) : 0);
                const float cw = (MX1 - MX0) / static_cast<float>(cols.size());
                pointed.push_back(u.hover.x >= MX0 && u.hover.x < MX1 ? static_cast<int>((u.hover.x - MX0) / cw) : -1);
                int row = -1, i = 0;
                for (const MxRow &mr : mxRows(s, static_cast<int>(u.srcs.size())))
                {
                    if (u.hover.y >= mr.y && u.hover.y < mr.y + mr.h && u.hover.x >= 272 && u.hover.x < MX1)
                        row = i;
                    ++i;
                }
                pointed.push_back(row);
            }
            else
                for (const Handle &h : handles(s))
                    pointed.push_back(std::hypot(u.hover.x - h.x, u.hover.y - h.y) < 10);
            if (pointed != drawnPointed_)
            {
                if (s.page == 1)
                    r.push_back(grid);
                else
                    for (const Handle &h : handles(s))
                        r.push_back({h.x - 120, h.y - 30, 240, 44});
            }
            if (s.page == 1 && (u.flashDst >= 0 || drawnFlash_)) // a column lit, fading
                r.push_back(grid);
            drawnFlash_ = u.flashDst >= 0;
            if (u.flashDst >= 0 && SDL_GetTicks() >= u.flashUntil)
                u.flashDst = -1;
            drawn_ = s;
            drawnGrain_ = u.grainName;
            drawnPicking_ = u.picking, drawnRec_ = u.vecRec, drawnDrag_ = u.modDrag;
            drawnMoving_ = std::move(moving), drawnScope_ = std::move(scope), drawnPointed_ = std::move(pointed), drawnHalos_ = std::move(halos);
            return r;
        }

    private:
        // the engine: every value set before its audio thread starts; from then on, through the queues
        void begin(const synth::io::Settings &settings)
        {
            State &s = u_.st;
            float oscV[4][48]{}, glbV[48]{};
            for (int o = 0; o < 4; ++o)
                for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                    oscV[o][p] = toValue(OSC_RANGES[p], s.osc[o][p]);
            for (int p = 0; p < GLB_PARAM_COUNT; ++p)
                glbV[p] = toValue(GLB_RANGES[p], s.glb[p]);
            sound_.start(banks_, samplePath(defaultGrain_), oscV, glbV, settings);
            for (int o = 0; o < 4; ++o)
                tables_[o] = sound_.tables(o);
            started_ = true;
            for (size_t i = 0; i < s.routes.size(); ++i)
            {
                const Route &rt = s.routes[i];
                sound_.setRoute(static_cast<int>(i), rt.src, std::max(0, rt.dst), (rt.amount - 0.5f) * 4, rt.on && rt.dst >= 0);
            }
            sent_ = s; // what the engine has: these, and the default recording (a preset's others follow)
            for (auto &f : sent_.grainFile)
                f = defaultGrain_;
            PresetData init{};
            if (!u_.presets.empty() && obs::loadPreset(u_.presets[0], tables_, init)) // the extras' start: Init's
                u_.extras = init;
            if (pendingSet_) // a preset chosen before the engine knew its tables: again with them (send() follows)
                apply(pending_);
        }

        // a preset into the state (and its extras to the engine)
        bool apply(const PresetEntry &e)
        {
            State &s = u_.st;
            PresetData d;
            if (!obs::loadPreset(e, tables_, d))
                return false;
            for (int o = 0; o < 4; ++o)
                for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                {
                    const Range &rg = OSC_RANGES[p];
                    const float v = std::isnan(d.osc[o][p]) ? rg.def : d.osc[o][p];
                    s.osc[o][p] = std::clamp(toNorm(rg, std::clamp(v, rg.lo, rg.hi)), 0.0f, 1.0f);
                }
            for (int p = 0; p < GLB_PARAM_COUNT; ++p)
            {
                if (std::isnan(d.glb[p]) && (p == GLB_VOLUME || p == GLB_XY_X || p == GLB_XY_Y))
                    continue; // not the preset's: as they are
                const Range &rg = GLB_RANGES[p];
                const float v = std::isnan(d.glb[p]) ? rg.def : d.glb[p];
                s.glb[p] = std::clamp(toNorm(rg, std::clamp(v, rg.lo, rg.hi)), 0.0f, 1.0f);
            }
            s.routes = {};
            for (size_t i = 0; i < d.routes.size() && i < s.routes.size(); ++i)
                s.routes[i] = {d.routes[i].src, d.routes[i].dst, std::clamp(0.5f + d.routes[i].amount / 4, 0.0f, 1.0f), true};
            s.selRoute = d.routes.empty() ? -1 : 0;
            for (int o = 0; o < 4; ++o) // the preset's recording, or the default
                s.grainFile[static_cast<size_t>(o)] = d.grainFile[static_cast<size_t>(o)].empty() ? defaultGrain_ : d.grainFile[static_cast<size_t>(o)];
            s.vecOn = d.vecOn;
            s.vecLoop = d.vecLoop;
            s.vecCount = static_cast<int>(std::min<size_t>(d.vecPath.size(), 16));
            s.vecPath = {};
            for (int i = 0; i < s.vecCount; ++i)
                s.vecPath[static_cast<size_t>(i)] = d.vecPath[static_cast<size_t>(i)];
            for (int m = 0; m < 3; ++m)
                for (int q = 0; q < 4; ++q)
                    sound_.setExtra(m * 4 + q, d.modEnv[m][q]);
            sound_.setExtra(12, static_cast<float>(d.tubeModel));
            sound_.setExtra(13, d.tubeChaos);
            u_.extras = d;
            u_.loaded = s;
            return true;
        }

        // the plugin's whole state (its JSON): as a preset loads
        void takeWhole(const std::string &whole)
        {
            Ui &u = u_;
            State &s = u.st;
            try
            {
                const auto j = nlohmann::json::parse(whole);
                const auto &vals = j.at("values");
                for (int o = 0; o < 4; ++o)
                    for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                        if (auto it = vals.find("OSC" + std::to_string(o + 1) + "_" + kOscIds[p]); it != vals.end())
                            s.osc[o][p] = it->get<float>();
                for (int p = 0; p < GLB_PARAM_COUNT; ++p)
                    if (auto it = vals.find(kGlbIds[p]); it != vals.end())
                        s.glb[p] = it->get<float>();
                s.routes = {};
                size_t k = 0;
                for (const auto &r : j.at("routes"))
                    if (k < s.routes.size())
                        s.routes[k++] = {r[0].get<int>(), r[1].get<int>(), std::clamp(0.5f + r[2].get<float>() / 4, 0.0f, 1.0f), r[3].get<bool>()};
                s.selRoute = k ? 0 : -1;
                for (size_t m = 0; m < 3 && m < j.at("modEnv").size(); ++m)
                    for (size_t q = 0; q < 4; ++q)
                        u.extras.modEnv[m][q] = j["modEnv"][m][q].get<float>();
                u.extras.tubeModel = j.at("tube")[0].get<int>(), u.extras.tubeChaos = j["tube"][1].get<float>();
                const auto &vec = j.at("vector");
                s.vecOn = vec.value("on", false), s.vecLoop = vec.value("loop", false);
                s.vecPath = {};
                s.vecCount = 0;
                for (const auto &q : vec.at("path"))
                    if (s.vecCount < 16)
                        s.vecPath[static_cast<size_t>(s.vecCount++)] = {q[0].get<float>(), q[1].get<float>(), q[2].get<float>()};
                if (j.contains("grains")) // the recordings: shown when send() reads them (the plugin has them)
                    for (size_t o = 0; o < 4 && o < j["grains"].size(); ++o)
                        if (const std::string g = j["grains"][o].get<std::string>(); !g.empty())
                            s.grainFile[o] = g;
                const std::string name = j.value("name", std::string());
                const int program = j.value("program", -1);
                if (host) // its name in the bar; its place in the list when it is the list's
                    host->shownPreset(program >= 0 && program < static_cast<int>(u.presets.size()) && u.presets[static_cast<size_t>(program)].name == name ? program
                                                                                                                                                   : host->presetAt(),
                                      name.empty() ? "START" : name);
                u.loaded = s;
                const State was = sent_;
                sent_ = s;
                for (int o = 0; o < 4; ++o) // the recordings stay the panel's
                    sent_.grainFile[static_cast<size_t>(o)] = was.grainFile[static_cast<size_t>(o)];
                if (u.ov)
                    u.ov->retarget();
            }
            catch (...)
            {
                synth::editorlog::say("ObsSynth4D", "panel", "the plugin's state could not be read");
            }
        }

        // a granular oscillator's recording: its shape for the display
        void overview(int o, const synth::GrainSource &src)
        {
            u_.grainName[static_cast<size_t>(o)] = src.name.substr(0, src.name.rfind('.'));
            const size_t cols = 289, n = src.samples.size();
            auto &pk = u_.grainPeaks[static_cast<size_t>(o)];
            pk.assign(cols, 0.0f);
            for (size_t k = 0; k < n; ++k)
            {
                float &p = pk[std::min(cols - 1, k * cols / n)];
                p = std::max(p, std::fabs(src.samples[k]));
            }
        }
        // read here (this thread), shown, handed to the engine (false: it has not taken the last one yet)
        bool loadGrain(int o, const std::string &file)
        {
            auto src = synth::GrainSource::loadWav(samplePath(file));
            if (!src)
            {
                if (host)
                    host->notice("CANNOT READ " + upper(file));
                return true;
            }
            overview(o, *src);
            sound_.setGrainFile(o, file); // in a host: the plugin reads it too
            return sound_.setGrainSource(o, std::move(src));
        }

        // what changed since the last frame goes to the engine
        void send()
        {
            const State &s = u_.st;
            for (int o = 0; o < 4; ++o)
                for (int p = 0; p < OSC_PARAM_COUNT; ++p)
                    if (s.osc[o][p] != sent_.osc[o][p])
                        sound_.setOsc(o, p, toValue(OSC_RANGES[p], s.osc[o][p]));
            for (int p = 0; p < GLB_PARAM_COUNT; ++p)
                if (s.glb[p] != sent_.glb[p])
                    sound_.setGlobal(p, toValue(GLB_RANGES[p], s.glb[p]));
            for (size_t i = 0; i < s.routes.size(); ++i)
            {
                const Route &a = s.routes[i], &b = sent_.routes[i];
                if (a.src != b.src || a.dst != b.dst || a.amount != b.amount || a.on != b.on)
                    sound_.setRoute(static_cast<int>(i), a.src, std::max(0, a.dst), (a.amount - 0.5f) * 4, a.on && a.dst >= 0);
            }
            bool grainLater[4] = {};
            for (int o = 0; o < 4; ++o)
                if (s.grainFile[static_cast<size_t>(o)] != sent_.grainFile[static_cast<size_t>(o)])
                    grainLater[o] = !loadGrain(o, s.grainFile[static_cast<size_t>(o)]);
            if (s.vecOn != sent_.vecOn || s.vecLoop != sent_.vecLoop || s.vecCount != sent_.vecCount || s.vecPath != sent_.vecPath)
            {
                for (int i = 0; i < s.vecCount; ++i)
                    for (int k = 0; k < 3; ++k)
                        sound_.setExtra(17 + i * 3 + k, s.vecPath[static_cast<size_t>(i)][static_cast<size_t>(k)]);
                sound_.setExtra(16, static_cast<float>(s.vecCount));
                sound_.setExtra(15, s.vecLoop ? 1.0f : 0.0f);
                sound_.setExtra(14, s.vecOn ? 1.0f : 0.0f);
            }
            sent_ = s;
            for (int o = 0; o < 4; ++o)
                if (grainLater[o]) // try again next frame
                    sent_.grainFile[static_cast<size_t>(o)].clear();
        }

        Ui u_;
        State sent_; // what the engine has
        Engine4D engine_;
        synth::Sound &sound_ = engine_.sound;
        std::array<std::string, 4> banks_;
        int tables_[4]{}; // each oscillator's wavetable count (a preset keeps a table, the panel 0..1)
        std::string defaultGrain_, themeName_;
        bool started_ = false, pendingSet_ = false;
        PresetEntry pending_;
        std::vector<Control> ctls_; // the controls as last laid out, and what each valued one is made from
        std::vector<Bind> binds_;
        // as last drawn (live)
        State drawn_;
        std::array<std::string, 4> drawnGrain_;
        bool drawnPicking_ = false, drawnRec_ = false, drawnFlash_ = false;
        int drawnDrag_ = -1;
        std::vector<float> drawnMoving_, drawnScope_, drawnHalos_;
        std::vector<int> drawnPointed_;
        uint64_t flipEvery_ = 0, lastFlip_ = 0, lastTickNs_ = 0, lastDebug_ = 0;
        int flipFrames_ = 3;
        bool debug_ = false;

    };
} // namespace

int main(int argc, char **argv)
{
    // the engine's profile: --profile lite|full (else OBS_PROFILE, else by the CPU: engine_profile.hpp), before the
    // engine exists
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--profile") == 0)
#ifdef _WIN32
            _putenv_s("OBS_PROFILE", argv[i + 1]);
#else
            setenv("OBS_PROFILE", argv[i + 1], 1);
#endif
    Synth4D panel(argc, argv);
    return synth::panel::run(panel, argc, argv);
}
