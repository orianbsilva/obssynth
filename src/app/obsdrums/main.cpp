// obsdrums: the ObsDrums front panel, a column per pad (docs/mockups/obsdrums_mockup.py), a product of the panels'
// SDK (common/panel_app.hpp: the window, the top bar, SETUP, the CC overlay, the kits as presets, the drawing).
// Sixteen columns: the twelve instruments and four free pads; each its engine (CIRCUIT, FM, VA, SAMPLE, GRAIN), its
// circuit when CIRCUIT, what it played lately, TUNE, DECAY, the model's two knobs, FX and pan, BENT, the level with
// its meter, mute, solo, and the pad. On the right the master (delay, reverb, tube and glue, the output).
//   knob, fader: drag (Shift: fine), wheel, double click: the middle.  Engine, circuit: click (right: back).
//   SAMPLE and GRAIN: the display's ends step through the samples folder.  A pad: click (higher, harder).
//   Keys: Z X C V B N M , and A S D F G H J K play the sixteen pads.
//   MIDI in on ObsDrums:input (gridseq joins it by itself): the pads by their notes, CC1 (the wheel), CC16-17.
//   The CC overlay's names: "pad.3.TUNE", the chosen pad's "pad.sel.TUNE", the bank's "pad.bank2.LEVEL", "master.GLUE".
//   --remote FD: the editor of ObsDrums.clap (sound.cpp has the link; the kit goes whole when a value cannot say it).
// Threads: this one draws and reads the input; sound.cpp's queues carry every change to the audio thread.
#include "../common/editor_log.hpp"
#include "../common/panel_app.hpp"
#include "../common/sample_find.hpp"
#include "controllers.hpp"
#include "kits_io.hpp"
#include "sound.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

using namespace synth;
using namespace synth::drums;
using panelkit::Align;
using panelkit::Canvas;
using synth::panel::Control;
using Kind = synth::panel::Kind;

namespace
{
    constexpr float W = 1600;
    constexpr float CX0 = 10, CW = 92, CY0 = 68, CH = 822; // the columns
    constexpr float MX = CX0 + 16 * CW + 8, MW = W - MX - 10;  // the master
    const char *kKeys[16] = {"Z", "X", "C", "V", "B", "N", "M", ",", "A", "S", "D", "F", "G", "H", "J", "K"};
    const SDL_Scancode kScan[16] = {SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V, SDL_SCANCODE_B, SDL_SCANCODE_N,
                                    SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
                                    SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K};

    // the model's two knobs, by engine and circuit (as the models name them)
    std::pair<const char *, const char *> knobNames(const PadSettings &s)
    {
        switch (s.engine)
        {
        case DrumEngine::FM: return {"RATIO", "INDEX"};
        case DrumEngine::VA: return {"WAVE", "FILTER"};
        case DrumEngine::SAMPLE: return {"START", "FILTER"};
        case DrumEngine::GRAIN: return {"SIZE", "SPRAY"};
        default: break;
        }
        switch (s.circuit)
        {
        case DrumKind::KICK: return {"ATTACK", "DRIVE"};
        case DrumKind::SNARE: return {"TONE", "SNAPPY"};
        case DrumKind::CLAP: return {"SPREAD", "TONE"};
        case DrumKind::RIM: return {"TONE", "CLICK"};
        case DrumKind::TOM_LO:
        case DrumKind::TOM_MID:
        case DrumKind::TOM_HI: return {"SWEEP", "NOISE"};
        default: return {"TONE", "METAL"};
        }
    }

    // the drums' state: the kit (the controls point into it), what the engine has, the recordings, what played
    struct St
    {
        DrumKit kit, sent;
        bool routesDirty = true;
        std::vector<std::string> samples;
        std::array<std::array<float, 40>, NUM_DRUM_PADS> hist{};
        int histAt = 0;
        int selPad = 0, bank = 0; // the pad the controller's knobs follow, the bank its faders hold
        ccov::Overlay *ov = nullptr;
        void retarget()
        {
            if (ov)
                ov->retarget();
        }
    };

    // a control as the panel had it (kind, rectangle, its value and range, click, pad, lit, label, the overlay's name)
    Control mk(Kind k, float x, float y, float a, float b, float *v, float lo, float hi, std::function<void(int)> click, int pad, bool on, const std::string &label,
               const std::string &id)
    {
        Control c;
        c.kind = k, c.x = x, c.y = y, c.a = a, c.b = b, c.v = v, c.lo = lo, c.hi = hi, c.click = std::move(click), c.group = pad, c.on = on, c.label = label, c.id = id;
        if (k == Kind::Fader)
            c.def = 0.75f, c.jump = true, c.travel = a;
        return c;
    }

    // a control's names for the overlay: its own, and the chosen pad's or the bank's when it is that pad
    std::vector<std::string> aliases(const St &u, const std::string &id)
    {
        std::vector<std::string> v{id};
        if (id.rfind("pad.", 0) == 0)
        {
            const auto dot = id.find('.', 4);
            const int pad = std::atoi(id.c_str() + 4) - 1;
            const std::string p = id.substr(dot + 1);
            if (pad == u.selPad)
                v.push_back("pad.sel." + p);
            if (pad / 8 == u.bank)
                v.push_back("pad.bank" + std::to_string(pad % 8 + 1) + "." + p);
        }
        return v;
    }

    void stepSample(St &u, int pad, int dir)
    {
        if (u.samples.empty())
            return;
        std::string &f = u.kit.samples[static_cast<size_t>(pad)];
        auto it = std::find(u.samples.begin(), u.samples.end(), f);
        std::ptrdiff_t i = it == u.samples.end() ? (dir > 0 ? -1 : 0) : it - u.samples.begin();
        const auto n = static_cast<std::ptrdiff_t>(u.samples.size());
        f = u.samples[static_cast<size_t>(((i + dir) % n + n) % n)];
    }

    ccov::Target resolve(St &u, Sound &sound, const std::string &id)
    {
        ccov::Target t;
        if (id == "@bank+" || id == "@bank-")
            t.press = [&u] { u.bank ^= 1; u.retarget(); };
        else if (id == "@pad+" || id == "@pad-")
            t.press = [&u, up = id == "@pad+"] { u.selPad = (u.selPad + (up ? 1 : NUM_DRUM_PADS - 1)) % NUM_DRUM_PADS; u.retarget(); };
        else if (id.rfind("master.", 0) == 0)
        {
            DrumMaster &m = u.kit.master;
            const std::string p = id.substr(7);
            float *v[] = {&m.delayTime, &m.delayFeedback, &m.delayMix, &m.reverbSize, &m.reverbDamp, &m.reverbMix, &m.drive, &m.glue, &m.tubeMix, &m.volume};
            const char *say[] = {"DELAY TIME", "DELAY FDBK", "DELAY MIX", "REVERB SIZE", "REVERB DAMP", "REVERB MIX", "TUBE DRIVE", "GLUE", "TUBE MIX", "VOLUME"};
            for (int k = 0; k < 10; ++k)
                if (p == kMasterIds[k])
                {
                    t.v = v[k];
                    t.lo = k == 0 ? 0.02f : 0.0f;
                    t.hi = k == 1 ? 0.95f : 1.0f;
                    t.label = std::string("MASTER ") + say[k];
                }
        }
        else if (id.rfind("pad.", 0) == 0)
        {
            const auto dot = id.find('.', 4);
            if (dot == std::string::npos)
                return t;
            const std::string which = id.substr(4, dot - 4), p = id.substr(dot + 1);
            int pad = which == "sel" ? u.selPad : which.rfind("bank", 0) == 0 ? u.bank * 8 + std::atoi(which.c_str() + 4) - 1 : std::atoi(which.c_str()) - 1;
            if (pad < 0 || pad >= NUM_DRUM_PADS)
                return t;
            PadSettings &s = u.kit.pads[static_cast<size_t>(pad)];
            const auto names = knobNames(s);
            t.label = std::string(drumPadName(pad)) + " ";
            if (p == "TUNE") t.v = &s.p.tune, t.label += "TUNE";
            else if (p == "DECAY") t.v = &s.p.decay, t.label += "DECAY";
            else if (p == "A") t.v = &s.p.a, t.label += names.first;
            else if (p == "B") t.v = &s.p.b, t.label += names.second;
            else if (p == "FX") t.v = &s.fx, t.label += "FX";
            else if (p == "PAN") t.v = &s.pan, t.lo = -1, t.label += "PAN";
            else if (p == "LEVEL") t.v = &s.level, t.label += "LEVEL";
            else if (p == "MUTE") t.press = [&s] { s.mute = !s.mute; }, t.label += "MUTE";
            else if (p == "SOLO") t.press = [&s] { s.solo = !s.solo; }, t.label += "SOLO";
            else if (p == "BENT") t.press = [&s] { s.p.bent = !s.p.bent; }, t.label += "BENT";
            else if (p == "HIT") t.press = [&sound, pad] { sound.hit(pad, 1.0f); }, t.label.clear();
            else t.label.clear();
        }
        return t;
    }

    std::vector<Control> layoutOf(St &u, Sound &sound)
    {
        std::vector<Control> c;
        DrumKit &k = u.kit;
        auto knob = [&](float x, float y, float r, float *v, float lo, float hi, const std::string &label)
        { c.push_back(mk(Kind::Knob, x, y, r, 0, v, lo, hi, {}, -1, false, label, "")); };
        auto button = [&](float x, float y, float w, float h, const std::string &label, bool on, std::function<void(int)> f)
        { c.push_back(mk(Kind::Button, x, y, w, h, nullptr, 0, 1, std::move(f), -1, on, label, "")); };

        // the master volume (the bar's buttons: ui::TopBar)
        knob(1562, 29, 15, &k.master.volume, 0, 1, "");
        c.back().id = "master.VOLUME";

        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            PadSettings &s = k.pads[static_cast<size_t>(i)];
            const float x = CX0 + static_cast<float>(i) * CW, cx = x + (CW - 6) / 2;
            button(x + 6, CY0 + 28, CW - 18, 16, drumEngineName(s.engine), true, [&s](int b)
                   { s.engine = static_cast<DrumEngine>((static_cast<int>(s.engine) + (b == 3 ? NUM_DRUM_ENGINES - 1 : 1)) % NUM_DRUM_ENGINES); });
            if (s.engine == DrumEngine::CIRCUIT)
                button(x + 6, CY0 + 48, CW - 18, 14, drumKindName(s.circuit), false, [&s](int b)
                       { s.circuit = static_cast<DrumKind>((static_cast<int>(s.circuit) + (b == 3 ? NUM_DRUM_KINDS - 1 : 1)) % NUM_DRUM_KINDS); });
            if (s.engine == DrumEngine::SAMPLE || s.engine == DrumEngine::GRAIN) // the display's ends: the samples folder
            {
                button(x + 6, CY0 + 66, 16, 44, "", false, [&u, i](int) { stepSample(u, i, -1); });
                button(x + CW - 28, CY0 + 66, 16, 44, "", false, [&u, i](int) { stepSample(u, i, 1); });
            }
            const auto names = knobNames(s);
            const std::string pid = "pad." + std::to_string(i + 1) + ".";
            auto named = [&](const char *p) { c.back().id = pid + p; c.back().group = i; };
            knob(cx, CY0 + 150, 17, &s.p.tune, 0, 1, "TUNE");
            named("TUNE");
            knob(cx, CY0 + 220, 13, &s.p.decay, 0, 1, "DECAY");
            named("DECAY");
            knob(cx - 22, CY0 + 280, 10, &s.p.a, 0, 1, names.first);
            named("A");
            knob(cx + 22, CY0 + 280, 10, &s.p.b, 0, 1, names.second);
            named("B");
            knob(cx - 22, CY0 + 338, 9, &s.fx, 0, 1, "FX");
            named("FX");
            knob(cx + 22, CY0 + 338, 9, &s.pan, -1, 1, "PAN");
            named("PAN");
            button(x + 16, CY0 + 372, CW - 38, 14, "BENT", s.p.bent, [&s](int) { s.p.bent = !s.p.bent; });
            named("BENT");
            c.push_back(mk(Kind::Fader, cx - 6, CY0 + 404, 210, 0, &s.level, 0, 1, {}, i, false, "", ""));
            named("LEVEL");
            const float bw = (CW - 22) / 2;
            button(x + 6, CY0 + 636, bw, 16, "M", s.mute, [&s](int) { s.mute = !s.mute; });
            named("MUTE");
            button(x + 10 + bw, CY0 + 636, bw, 16, "S", s.solo, [&s](int) { s.solo = !s.solo; });
            named("SOLO");
            c.push_back(mk(Kind::Pad, x + 6, CY0 + 660, CW - 18, 84, nullptr, 0, 1, [&sound, i](int vel) { sound.hit(i, static_cast<float>(vel) / 127.0f); }, i, false, "", ""));
            named("HIT");
        }
        // the master: delay, reverb, tube and glue
        DrumMaster &m = k.master;
        // each section a triangle: its two knobs on top, the mix below between them
        const float a = MX + MW / 2 - 22, b = MX + MW / 2 + 22, d = MX + MW / 2;
        auto trio = [&](float y, float *one, float hiOne, const char *nOne, float *two, float hiTwo, const char *nTwo, float *mix, int id0)
        {
            knob(a, y, 13, one, one == &m.delayTime ? 0.02f : 0.0f, hiOne, nOne);
            c.back().id = std::string("master.") + kMasterIds[id0];
            knob(b, y, 13, two, 0, hiTwo, nTwo);
            c.back().id = std::string("master.") + kMasterIds[id0 + 1];
            knob(d, y + 50, 12, mix, 0, 1, "MIX");
            c.back().id = std::string("master.") + kMasterIds[id0 + 2];
        };
        trio(CY0 + 72, &m.delayTime, 1.0f, "TIME", &m.delayFeedback, 0.95f, "FDBK", &m.delayMix, 0);
        trio(CY0 + 202, &m.reverbSize, 1.0f, "SIZE", &m.reverbDamp, 1.0f, "DAMP", &m.reverbMix, 3);
        trio(CY0 + 332, &m.drive, 1.0f, "DRIVE", &m.glue, 1.0f, "GLUE", &m.tubeMix, 6);
        return c;
    }

    std::string bodyOf(const panelkit::Theme &t, const std::vector<Control> &ctls)
    {
        std::ostringstream s;
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            const float x = CX0 + static_cast<float>(i) * CW;
            panelkit::svgPanel(s, x, CY0, CW - 6, CH);
            panelkit::svgRule(s, x + 6, CY0 + 23, CW - 18);
            panelkit::svgDisplay(s, t, x + 6, CY0 + 66, CW - 18, 44);
        }
        panelkit::svgPanel(s, MX, CY0, MW, CH);
        panelkit::svgRule(s, MX + 8, CY0 + 23, MW - 16);
        panelkit::svgDisplay(s, t, MX + 10, CY0 + 450, MW - 20, 310);
        for (const Control &c : ctls)
            if (c.kind == Kind::Knob)
                panelkit::svgKnob(s, t, c.x, c.y, c.a);
            else if (c.kind == Kind::Fader)
                panelkit::svgFader(s, t, c.x, c.y, c.a);
        return s.str();
    }


    // ── the sound: sound.cpp's engine (or the plugin through its link) ──
    class DrumsEngine : public synth::panel::Engine
    {
    public:
        Sound sound;
        St *st = nullptr;
        bool start(const std::vector<float> &, const io::Settings &settings) override
        {
            return sound.start(st->kit, loadDrumKitSamples(st->kit, samplePathOf), settings);
        }
        bool connect(const char *link) override
        {
            const bool ok = sound.connect(link);
            start({}, io::loadSettings("obsdrums")); // linked: no audio, no MIDI here (the kit as it is)
            return ok;
        }
        bool remote() const override { return sound.remote(); }
        bool remoteAlive() const override { return sound.remoteAlive(); }
        bool pollRemote(int &, float &) override { return false; } // the pads' values: Drums::tick
        void setParam(int, float) override {}                     // the values: Drums::tick sends them
        void noteOn(int, float) override {}
        void noteOff(int) override {}
        // the CCs that came in; a note that hit a pad chooses it
        bool pollCc(int &channel, int &cc, float &value) override
        {
            Sound::Seen m;
            while (sound.pollMidi(m))
                if (m.cc)
                {
                    channel = m.channel, cc = m.number, value = m.value;
                    return true;
                }
                else if (m.pad >= 0 && m.pad != st->selPad)
                {
                    st->selPad = m.pad;
                    st->retarget();
                }
            return false;
        }
        io::MidiIn &midiIn() override { return sound.midiIn(); }
        const io::AudioOut &audioOut() const override { return sound.audioOut(); }
        bool reopenAudio(const io::Settings &settings) override { return sound.reopenAudio(settings); }
    };

    class Drums : public synth::panel::Product
    {
    public:
        Drums()
        {
            if (const char *base = SDL_GetBasePath()) // the shipped recordings (pcm/)
                addSampleDirsBeside(base);
            st_.samples = listSampleNames(samplesDir());
            std::sort(st_.samples.begin(), st_.samples.end());
            engine_.st = &st_;
        }

        const char *id() const override { return "obsdrums"; }
        const char *title() const override { return "ObsDrums"; }
        const char *barName() const override { return "OBSDRUMS"; }
        const char *barNumber() const override { return "16"; }
        float barNumberX() const override { return 213; }

        int params() const override { return 0; } // the kit's own values: the controls point into it
        std::string paramName(int) const override { return {}; }
        float paramDefault(int) const override { return 0.5f; }
        std::string paramText(int, float) const override { return {}; }

        std::vector<synth::panel::PresetEntry> presets() override
        {
            std::vector<synth::panel::PresetEntry> v;
            for (const KitEntry &e : listKits())
                v.push_back({e.name, e.path, e.factory});
            return v;
        }
        bool loadPreset(const synth::panel::PresetEntry &e, std::vector<float> &, std::string &name) override
        {
            DrumKit k;
            if (!loadKit({e.name, e.path, e.factory}, k))
                return false;
            st_.kit = k;
            st_.routesDirty = true;
            name = k.name;
            return true;
        }
        bool savePreset(const std::string &name, const std::vector<float> &, std::string &path) override
        {
            st_.kit.name = name;
            return saveKit(st_.kit, path);
        }

        std::vector<ccov::Map> controllers() const override { return factoryControllers(); }
        ccov::Target target(const std::string &id) override
        {
            st_.ov = host ? host->overlay : nullptr;
            return resolve(st_, sound_, id);
        }
        std::vector<std::string> aliases(const std::string &id) override { return ::aliases(st_, id); }
        synth::panel::Engine &engine() override { return engine_; }

        std::string layoutKey(const std::vector<float> &) const override { return "drums"; }
        std::vector<Control> layout(std::vector<float> &) override { return layoutOf(st_, sound_); }
        std::string bodySvg(const panelkit::Theme &t, const std::string &) override { return bodyOf(t, layoutOf(st_, sound_)); }

        void drawBody(Canvas &cv, const std::vector<float> &, const synth::panel::Clip &clip) override
        {
            St &u = st_;
            const panelkit::Theme &t = cv.theme;
            if (clip.touches(0, 0, W, 58))
                cv.say("MASTER", 1534, 34, 10, t.ink, 1, true, false, 1.6f, Align::Right);
        // the columns' words and live parts
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            const PadSettings &s = u.kit.pads[static_cast<size_t>(i)];
            const float x = CX0 + static_cast<float>(i) * CW, cx = x + (CW - 6) / 2;
            cv.say(drumPadName(i), x + 8, CY0 + 17, 9, i == u.selPad ? t.accent : t.ink, 1, true, false, 1.2f, Align::Left);
            if (i == u.selPad) // the pad the controller's knobs follow
                cv.rect(x + 6, CY0 + 22, CW - 18, 2, t.accent, 0.9f);
            if (i / 8 == u.bank) // the bank the faders hold
                cv.rect(x + 2, CY0 + CH - 6, CW - 10, 2, t.accent, 0.35f);
            // the display: what the pad played lately (a bar per frame), or its recording's name
            const float dx = x + 6, dy = CY0 + 66, dw = CW - 18, dh = 44;
            const auto &h = u.hist[static_cast<size_t>(i)];
            for (int k = 0; k < 40; ++k)
            {
                const float v = std::min(1.0f, h[static_cast<size_t>((u.histAt + k) % 40)] * 1.6f);
                const float bh = std::max(0.5f, v * (dh - 8));
                cv.rect(dx + 3 + static_cast<float>(k) * (dw - 6) / 40, dy + dh - 4 - bh, std::max(1.0f, (dw - 6) / 40 - 0.4f), bh, t.trace, 0.75f);
            }
            if (s.engine == DrumEngine::SAMPLE || s.engine == DrumEngine::GRAIN)
            {
                std::string f = u.kit.samples[static_cast<size_t>(i)];
                f = f.substr(0, f.rfind('.'));
                if (f.rfind("vcsl-", 0) == 0)
                    f = f.substr(5);
                if (f.size() > 11)
                    f = f.substr(0, 11);
                for (auto &ch : f)
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                cv.say(f.empty() ? "NO SAMPLE" : f, cx, dy + 12, 6.5f, t.accent, 0.9f, false, true, 0, Align::Centre, false);
                cv.say("‹", dx + 7, dy + 28, 10, t.dim, 1, false, false, 0, Align::Centre);
                cv.say("›", dx + dw - 7, dy + 28, 10, t.dim, 1, false, false, 0, Align::Centre);
            }
            if (s.choke >= 0)
                cv.say("CHOKE " + std::to_string(s.choke + 1), cx, CY0 + 798, 7, t.accent, 0.7f, false, false, 1, Align::Centre);
            cv.say(kKeys[i], cx, CY0 + 766, 8, t.dim, 1, true, false, 0, Align::Centre);
            cv.say("NOTE " + std::to_string(s.note), cx, CY0 + 780, 6.5f, t.dim, 0.8f, false, true, 0, Align::Centre, false);
        }


        // the master's words and the output
        cv.say("MASTER", MX + 10, CY0 + 17, 9, t.ink, 1, true, false, 1.6f, Align::Left);
        cv.say("DELAY", MX + 10, CY0 + 45, 7, t.dim, 1, false, false, 1.4f, Align::Left);
        cv.say("REVERB", MX + 10, CY0 + 175, 7, t.dim, 1, false, false, 1.4f, Align::Left);
        cv.say("TUBE · GLUE", MX + 10, CY0 + 305, 7, t.dim, 1, false, false, 1.4f, Align::Left);
        cv.say("OUT", MX + 10, CY0 + 442, 7, t.dim, 1, false, false, 1.4f, Align::Left);
        for (int side = 0; side < 2; ++side)
        {
            const float v = std::min(1.0f, sound_.outPeak(side)), bx = MX + MW / 2 - 25 + static_cast<float>(side) * 28, by = CY0 + 460, bh = 272;
            cv.rect(bx, by, 22, bh, t.bg, 0.6f);
            cv.rect(bx, by + bh * (1 - v), 22, bh * v, v > 0.89f ? t.accent : t.traceHi, 0.9f);
            cv.say(side ? "R" : "L", bx + 11, by + bh + 12, 8, t.ink, 1, true, false, 0, Align::Centre);
        }
        cv.say("-1 dBFS", MX + MW / 2, CY0 + 790, 7, t.dim, 1, false, true, 0.5f, Align::Centre, false);
        }

        bool drawControl(Canvas &cv, const Control &c, float val) override
        {
            const panelkit::Theme &t = cv.theme;
            switch (c.kind)
            {
            case Kind::Knob:
                cv.cap(c.x, c.y, c.a, val);
                if (!c.label.empty())
                    cv.say(c.label, c.x, c.y + c.a + 13, 7, t.ink, 1, true, false, 0.8f, Align::Centre);
                break;
            case Kind::Fader:
                cv.faderCap(c.x, c.y, c.a, val, sound_.padPeak(c.group) * 1.4f);
                break;
            case Kind::Button:
                if (c.label.empty())
                    break;
                cv.box(c.x, c.y, c.a, c.b, c.on);
                cv.say(c.label, c.x + c.a / 2, c.y + c.b / 2 + 3.5f, c.b > 15 ? 7.5f : 6.5f, c.on ? t.accent : t.dim, 1, true, false, 0.8f, Align::Centre);
                break;
            case Kind::Pad:
            {
                const float f = sound_.padFlash(c.group);
                cv.box(c.x, c.y, c.a, c.b, false);
                if (f > 0.02f)
                    cv.rect(c.x + 2, c.y + 2, c.a - 4, c.b - 4, t.accent, std::min(1.0f, f));
                break;
            }
            default: return false;
            }
            return true;
        }

        void pressed(const Control &c) override
        {
            if (c.group != st_.selPad) // a click in a column chooses its pad
            {
                st_.selPad = c.group;
                st_.retarget();
                if (host)
                    host->whole();
            }
        }
        bool key(SDL_Scancode sc, bool down) override
        {
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
                if (sc == kScan[i])
                {
                    if (down)
                        sound_.hit(i, (SDL_GetModState() & SDL_KMOD_SHIFT) ? 1.0f : 0.8f);
                    return true;
                }
            return false;
        }
        std::string status() override
        {
            return std::string("OBSDRUMS:INPUT  ·  BANK ") + (st_.bank ? "9-16" : "1-8") + "  ·  " + drumPadName(st_.selPad);
        }

        void tick() override
        {
            St &u = st_;
            u.ov = host ? host->overlay : nullptr;
            if (u.ov && (u.ov->active != noteMapOf_ || u.ov->client != noteMapClient_)) // the controller's notes to the pads
            {
                noteMapOf_ = u.ov->active;
                noteMapClient_ = u.ov->client;
                const ccov::Map *mp = u.ov->map();
                sound_.setNoteMap(u.ov->client, mp ? mp->notes : std::array<int, 128>{});
            }
        if (sound_.remote()) // what the host changed: shown, not sent back
        {
            int pad, param;
            float v;
            while (sound_.pollRemote(pad, param, v) )
                if (pad < 0)
                    for (DrumMaster *m : {&u.kit.master, &u.sent.master})
                    {
                        float *f[] = {&m->delayTime, &m->delayFeedback, &m->delayMix, &m->reverbSize, &m->reverbDamp, &m->reverbMix, &m->drive, &m->glue, &m->tubeMix, &m->volume};
                        if (param >= 0 && param < static_cast<int>(std::size(f)))
                            *f[param] = v;
                    }
                else if (pad < NUM_DRUM_PADS)
                    for (PadSettings *q : {&u.kit.pads[static_cast<size_t>(pad)], &u.sent.pads[static_cast<size_t>(pad)]})
                    {
                        const int n = static_cast<int>(std::lround(v));
                        switch (param)
                        {
                        case P_ENGINE: q->engine = static_cast<DrumEngine>(std::clamp(n, 0, NUM_DRUM_ENGINES - 1)); break;
                        case P_CIRCUIT: q->circuit = static_cast<DrumKind>(std::clamp(n, 0, NUM_DRUM_KINDS - 1)); break;
                        case P_TUNE: q->p.tune = v; break;
                        case P_DECAY: q->p.decay = v; break;
                        case P_A: q->p.a = v; break;
                        case P_B: q->p.b = v; break;
                        case P_BENT: q->p.bent = v >= 0.5f; break;
                        case P_LEVEL: q->level = v; break;
                        case P_PAN: q->pan = v; break;
                        case P_FX: q->fx = v; break;
                        case P_MUTE: q->mute = v >= 0.5f; break;
                        case P_SOLO: q->solo = v >= 0.5f; break;
                        case P_GRAINPOS: q->grainPos = v; break;
                        default: break;
                        }
                    }
            std::string whole;
            if (sound_.pollKit(whole) ) // the plugin's kit (on opening, a program, a session): as a kit loads
                try
                {
                    DrumKit k;
                    if (kitFromJson(json::parse(whole), k))
                    {
                        u.kit = u.sent = k;
                        u.routesDirty = false;
                        const auto kits = listKits();
                        for (size_t i = 0; i < kits.size(); ++i) // its name in the list: there, the arrows go on from it
                            if (kits[i].name == k.name && host)
                                host->shownPreset(static_cast<int>(i), k.name);
                        u.retarget();
                        if (host)
                            host->whole();
                    }
                }
                catch (...)
                {
                    editorlog::say("ObsDrums", "panel", "the plugin's kit could not be read");
                }
        }
            send();
            if (sound_.kitWanted()) // a change no parameter carries (a note, a choke, the routes, a recording): the whole kit
                sound_.sendKit(kitToJson(u.kit).dump());
            for (int i = 0; i < NUM_DRUM_PADS; ++i) // the displays' history
                u.hist[static_cast<size_t>(i)][static_cast<size_t>(u.histAt)] = sound_.padPeak(i);
            u.histAt = (u.histAt + 1) % 40;
        }

        // what moves by itself: the displays' history, the faders' meters, the pads' flashes, the output, a recording's name
        std::vector<SDL_FRect> live() override
        {
            std::vector<SDL_FRect> r;
            for (int i = 0; i < NUM_DRUM_PADS; ++i)
            {
                const float x = CX0 + static_cast<float>(i) * CW, cx = x + (CW - 6) / 2;
                const auto &h = st_.hist[static_cast<size_t>(i)];
                const bool quiet = std::all_of(h.begin(), h.end(), [](float v) { return v < 1e-4f; });
                const std::string &smp = st_.kit.samples[static_cast<size_t>(i)];
                if (!quiet || !wasQuiet_[static_cast<size_t>(i)] || smp != shownSample_[static_cast<size_t>(i)])
                    r.push_back({x + 6, CY0 + 66, CW - 18, 44});
                wasQuiet_[static_cast<size_t>(i)] = quiet;
                shownSample_[static_cast<size_t>(i)] = smp;
                const int m = static_cast<int>(sound_.padPeak(i) * 140.0f), f = static_cast<int>(sound_.padFlash(i) * 100.0f);
                if (m != meter_[static_cast<size_t>(i)])
                    r.push_back({cx - 6 - 28, CY0 + 404 - 14, 56, 210 + 28});
                if (f != flash_[static_cast<size_t>(i)])
                    r.push_back({x + 6, CY0 + 660, CW - 18, 84});
                meter_[static_cast<size_t>(i)] = m, flash_[static_cast<size_t>(i)] = f;
            }
            const int o[2] = {static_cast<int>(sound_.outPeak(0) * 300), static_cast<int>(sound_.outPeak(1) * 300)};
            if (o[0] != out_[0] || o[1] != out_[1])
                r.push_back({MX + MW / 2 - 30, CY0 + 455, 64, 290});
            out_[0] = o[0], out_[1] = o[1];
            return r;
        }

    private:
        // what changed since the last frame goes to the engine: the pads field by field, the master, the recordings
        void send()
        {
            St &u = st_;
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            const PadSettings &a = u.kit.pads[static_cast<size_t>(i)], &b = u.sent.pads[static_cast<size_t>(i)];
            auto put = [&](int p, float va, float vb) { if (va != vb) sound_.setPad(i, p, va); };
            put(P_ENGINE, static_cast<float>(a.engine), static_cast<float>(b.engine));
            put(P_CIRCUIT, static_cast<float>(a.circuit), static_cast<float>(b.circuit));
            put(P_TUNE, a.p.tune, b.p.tune);
            put(P_DECAY, a.p.decay, b.p.decay);
            put(P_A, a.p.a, b.p.a);
            put(P_B, a.p.b, b.p.b);
            put(P_BENT, a.p.bent ? 1.0f : 0.0f, b.p.bent ? 1.0f : 0.0f);
            put(P_LEVEL, a.level, b.level);
            put(P_PAN, a.pan, b.pan);
            put(P_FX, a.fx, b.fx);
            put(P_MUTE, a.mute ? 1.0f : 0.0f, b.mute ? 1.0f : 0.0f);
            put(P_SOLO, a.solo ? 1.0f : 0.0f, b.solo ? 1.0f : 0.0f);
            put(P_NOTE, static_cast<float>(a.note), static_cast<float>(b.note));
            put(P_CHOKE, static_cast<float>(a.choke), static_cast<float>(b.choke));
            put(P_GRAINPOS, a.grainPos, b.grainPos);
        }
        const DrumMaster &m = u.kit.master, &n = u.sent.master;
        auto putM = [&](int p, float va, float vb) { if (va != vb) sound_.setMaster(p, va); };
        putM(M_DELAY_TIME, m.delayTime, n.delayTime);
        putM(M_DELAY_FB, m.delayFeedback, n.delayFeedback);
        putM(M_DELAY_MIX, m.delayMix, n.delayMix);
        putM(M_REV_SIZE, m.reverbSize, n.reverbSize);
        putM(M_REV_DAMP, m.reverbDamp, n.reverbDamp);
        putM(M_REV_MIX, m.reverbMix, n.reverbMix);
        putM(M_DRIVE, m.drive, n.drive);
        putM(M_GLUE, m.glue, n.glue);
        putM(M_TUBE_MIX, m.tubeMix, n.tubeMix);
        putM(M_VOLUME, m.volume, n.volume);
        if (u.routesDirty)
        {
            sound_.clearRoutes();
            int slot = 0;
            for (const DrumRoute &r : u.kit.routes)
                sound_.setRoute(slot++, static_cast<int>(r.src), static_cast<int>(r.pad) * PARAMS_PER_PAD + r.param, r.amount, true);
            u.routesDirty = false;
        }
        std::array<bool, NUM_DRUM_PADS> later{};
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
        {
            const std::string &f = u.kit.samples[static_cast<size_t>(i)];
            if (f != u.sent.samples[static_cast<size_t>(i)] && !f.empty())
            {
                auto src = GrainSource::loadWav(samplePathOf(f)); // read here, this thread
                if (!src)
                {
                    if (host)
                        host->notice("CANNOT READ " + f);
                }
                else if (!sound_.setSource(i, std::move(src)))
                    later[static_cast<size_t>(i)] = true;
            }
        }
        const auto samplesWere = u.sent.samples;
        u.sent = u.kit;
        for (int i = 0; i < NUM_DRUM_PADS; ++i)
            if (later[static_cast<size_t>(i)]) // the engine had not taken the last one: again next frame
                u.sent.samples[static_cast<size_t>(i)] = samplesWere[static_cast<size_t>(i)];
            }

        St st_;
        DrumsEngine engine_;
        Sound &sound_ = engine_.sound;
        int noteMapOf_ = -2, noteMapClient_ = -2;
        std::array<bool, NUM_DRUM_PADS> wasQuiet_{};
        std::array<std::string, NUM_DRUM_PADS> shownSample_{};
        std::array<int, NUM_DRUM_PADS> meter_{}, flash_{};
        int out_[2] = {0, 0};
    };
} // namespace

int main(int argc, char **argv)
{
    Drums drums;
    return synth::panel::run(drums, argc, argv);
}
