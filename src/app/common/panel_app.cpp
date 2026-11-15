// The panels' SDK: the program around a product's panel (panel_app.hpp says what is here and what is the product's).
// Threads: this one draws and reads the input (and talks to the product); the backgrounds not drawn yet are
// rasterized by a thread of their own (SVG to pixels only: the textures are made here); the product's Engine has
// the audio and the MIDI threads.
#include "panel_app.hpp"

#include "editor_log.hpp"
#include "host_window.hpp"
#include "setup_page.hpp"
#include "top_bar.hpp"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

namespace synth::panel
{
    namespace
    {
        using panelkit::Align;

        ui::Painter painterOf(panelkit::Canvas &cv)
        {
            ui::Painter p;
            p.rect = [&cv](float x, float y, float w, float h, const std::string &hex, float a, bool fill) { cv.rect(x, y, w, h, hex, a, fill); };
            p.say = [&cv](const std::string &txt, float x, float y, float size, const std::string &hex, float a, bool bold, bool mono, float tracking, int align)
            { cv.say(txt, x, y, size, hex, a, bold, mono, tracking, align == 0 ? Align::Left : align == 1 ? Align::Centre : Align::Right, false); };
            p.box = [&cv](float x, float y, float w, float h, bool on) { cv.box(x, y, w, h, on); };
            const panelkit::Theme &t = cv.theme;
            p.bg = t.bg, p.ink = t.ink, p.dim = t.dim, p.accent = t.accent, p.display = t.display;
            return p;
        }

        // a control's area (panel units): what a change of its value draws again (a knob's ring around it, a held
        // fader's value beside it, a switch's label above it)
        SDL_FRect areaOf(const Control &c)
        {
            switch (c.kind)
            {
            case Kind::Knob:
            {
                const float half = std::max(c.a + 16, 48.0f * c.text);
                return {c.x - half, c.y - c.a - 16, 2 * half, 2 * c.a + 16 + 44 * c.text};
            }
            case Kind::Fader: return {c.x - 28, c.y - 14, 100, c.a + 14 + 30 * c.text};
            case Kind::Switch: return {c.x - 3, c.y - 22, static_cast<float>(c.options.size()) * (c.a + 3) + 3, c.b + 25};
            case Kind::Toggle: return {c.x - 50, c.y - 28, 100, 42};
            case Kind::XY: return {c.x - 12, c.y - 12, c.a + 24, c.b + 24};
            default: return {c.x - 3, c.y - 3, c.a + 6, c.b + 6};
            }
        }

        // a switch's choice under x, or -1
        int optionAt(const Control &c, float x)
        {
            const int k = static_cast<int>(std::floor((x - c.x) / (c.a + 3)));
            return k >= 0 && k < static_cast<int>(c.options.size()) && x - c.x - static_cast<float>(k) * (c.a + 3) <= c.a ? k : -1;
        }

        bool hits(const Control &c, float x, float y)
        {
            switch (c.kind)
            {
            case Kind::Knob: return std::hypot(x - c.x, y - c.y) <= c.a + 6;
            case Kind::Fader: return std::fabs(x - c.x) <= 14 && y >= c.y - 8 && y <= c.y + c.a + 8;
            case Kind::Toggle: return std::hypot(x - c.x, y - c.y) <= 12;
            case Kind::Switch: return y >= c.y && y <= c.y + c.b && optionAt(c, x) >= 0;
            default: return x >= c.x && x <= c.x + c.a && y >= c.y && y <= c.y + c.b;
            }
        }

        struct App
        {
            Product &p;
            Engine &e;
            SDL_Window *win = nullptr;
            SDL_Renderer *ren = nullptr;
            panelkit::Canvas cv;
            io::Settings settings;
            std::vector<float> v, sent, loaded, drawn; // the panel's values; what the engine has; the preset's; as drawn
            std::vector<PresetEntry> presets;
            int presetAt = 0, presetReq = -1;
            std::string presetName;
            bool naming = false;
            std::string nameEdit, notice;
            uint64_t noticeUntil = 0;
            ccov::Overlay ov;
            int drag = -1;
            float dragY = 0, dragV = 0;
            uint64_t lastClick = 0;
            int lastCtl = -1;

            // the backgrounds by layout; those a thread rasterized, waiting to be made textures here
            std::map<std::string, SDL_Texture *> bases;
            std::mutex readyLock;
            std::map<std::string, SDL_Surface *> ready;
            std::thread raster;
            std::atomic<bool> stopRaster{false};
            // the panel as last drawn, and what it showed
            SDL_Texture *canvas = nullptr;
            std::string drawnKey, drawnBar;
            bool whole = true;
            struct Seen { float v, vy; bool on, held; std::string label; };
            std::vector<Seen> seen; // each control as last drawn
            Host host;

            App(Product &prod) : p(prod), e(prod.engine()) {}

            // a control's value, 0..1, and setting it (a stepped one snaps); an XY pad's second value
            static bool own(const Control &c) { return c.v || c.get; }
            static bool valued(const Control &c) { return own(c) || c.param >= 0; }
            int stepsOf(const Control &c) const
            {
                if (c.steps > 1)
                    return c.steps;
                if (c.kind == Kind::Switch)
                    return static_cast<int>(c.options.size());
                return own(c) || c.param < 0 ? 0 : p.paramSteps(c.param);
            }
            float valueOf(const Control &c) const
            {
                if (c.get)
                    return c.get();
                if (c.v)
                    return c.hi != c.lo ? (*c.v - c.lo) / (c.hi - c.lo) : 0.0f;
                return c.param >= 0 ? v[static_cast<size_t>(c.param)] : 0.0f;
            }
            static float valueY(const Control &c) { return c.vy && c.hi != c.lo ? (*c.vy - c.lo) / (c.hi - c.lo) : 0.0f; }
            void setValue(const Control &c, float n)
            {
                n = std::clamp(n, 0.0f, 1.0f);
                if (const int st = stepsOf(c); st > 1)
                    n = std::round(n * static_cast<float>(st - 1)) / static_cast<float>(st - 1);
                if (c.set)
                    c.set(n);
                else if (c.v)
                    *c.v = c.lo + n * (c.hi - c.lo);
                else if (c.param >= 0)
                    v[static_cast<size_t>(c.param)] = n;
            }
            // an XY pad: both values from a point on it
            void setPoint(const Control &c, float x, float y)
            {
                setValue(c, (x - c.x) / c.a);
                if (c.vy)
                    *c.vy = c.lo + std::clamp(1 - (y - c.y) / c.b, 0.0f, 1.0f) * (c.hi - c.lo);
            }
            float defaultOf(const Control &c) const { return c.def >= 0 ? c.def : !own(c) && c.param >= 0 ? p.paramDefault(c.param) : 0.5f; }
            // the overlay's name for a control: its own, or its parameter's
            std::string idOf(const Control &c) const { return !c.id.empty() ? c.id : c.param >= 0 && !own(c) ? "p." + p.paramName(c.param) : std::string(); }

            int find(const std::string &name) const
            {
                for (int i = 0; i < p.params(); ++i)
                    if (p.paramName(i) == name)
                        return i;
                return -1;
            }

            void loadPresetAt(int k)
            {
                if (k < 0 || k >= static_cast<int>(presets.size()))
                    return;
                std::vector<float> nv = v;
                std::string name;
                if (!p.loadPreset(presets[static_cast<size_t>(k)], nv, name))
                {
                    notice = "CANNOT READ IT";
                    noticeUntil = SDL_GetTicks() + 2500;
                    return;
                }
                v = nv;
                loaded = v;
                presetAt = k;
                presetName = name;
                ov.retarget();
                whole = true;
            }

            ccov::Target resolve(const std::string &id)
            {
                ccov::Target t;
                if (id == "@preset+" || id == "@preset-" || id == "@kit+" || id == "@kit-")
                    t.press = [this, up = id == "@preset+" || id == "@kit+"] { presetReq = stepPreset(up ? 1 : -1); };
                else if (id == "@overlay")
                    t.press = [this] { ov.shown = !ov.shown; };
                else if (id.rfind("p.", 0) == 0)
                {
                    if (const int k = find(id.substr(2)); k >= 0)
                    {
                        t.v = &v[static_cast<size_t>(k)];
                        t.label = id.substr(2);
                        for (auto &c : t.label)
                            if (c == '_')
                                c = ' ';
                    }
                }
                else
                    t = p.target(id);
                return t;
            }

            // the preset after (dir 1) or before (-1) the one shown; from none (the start): the first, the last
            int stepPreset(int dir) const
            {
                const int n = std::max(1, static_cast<int>(presets.size()));
                if (presetAt < 0)
                    return dir > 0 ? 0 : n - 1;
                return ((presetAt + dir) % n + n) % n;
            }

            // ── the backgrounds ──
            std::string fullSvg(const std::string &key)
            {
                const panelkit::Theme &t = cv.theme;
                std::ostringstream s;
                s << panelkit::svgOpen(static_cast<double>(W), static_cast<double>(H));
                panelkit::svgDefs(s, t);
                s << "<rect width='" << W << "' height='" << H << "' fill='" << t.bg << "'/>";
                s << "<rect width='" << W << "' height='" << BAR << "' fill='url(#bar)'/><line x1='0' y1='" << BAR << "' x2='" << W << "' y2='" << BAR << "' stroke='#000'/>";
                panelkit::svgDisplay(s, t, ui::TopBar::DX, ui::TopBar::DY, ui::TopBar::DW, ui::TopBar::DH);
                s << p.bodySvg(t, key) << "</svg>";
                return s.str();
            }
            SDL_Texture *baseFor(const std::string &key)
            {
                if (auto it = bases.find(key); it != bases.end())
                    return it->second;
                SDL_Surface *surf = nullptr;
                {
                    std::lock_guard<std::mutex> g(readyLock);
                    if (auto it = ready.find(key); it != ready.end())
                        surf = it->second, ready.erase(it);
                }
                SDL_Texture *t = nullptr;
                if (surf)
                {
                    t = SDL_CreateTextureFromSurface(ren, surf);
                    SDL_DestroySurface(surf);
                    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
                }
                else // not rasterized yet: now
                    t = panelkit::rasterize(ren, fullSvg(key), static_cast<int>(W * cv.scale), static_cast<int>(H * cv.scale));
                return bases[key] = t;
            }
            void stopRasterizing()
            {
                stopRaster = true;
                if (raster.joinable())
                    raster.join();
                stopRaster = false;
                std::lock_guard<std::mutex> g(readyLock);
                for (auto &[k, s] : ready)
                    SDL_DestroySurface(s);
                ready.clear();
            }
            void dropBases()
            {
                stopRasterizing();
                for (auto &[k, t] : bases)
                    if (t)
                        SDL_DestroyTexture(t);
                bases.clear();
            }
            void rasterizeRest()
            {
                std::vector<std::pair<std::string, std::string>> todo;
                for (const std::string &k : p.layoutKeys())
                    if (!bases.count(k))
                        todo.emplace_back(k, fullSvg(k));
                if (todo.empty())
                    return;
                const int w = static_cast<int>(W * cv.scale), h = static_cast<int>(H * cv.scale);
                raster = std::thread([this, todo = std::move(todo), w, h] {
                    for (const auto &[key, svg] : todo)
                    {
                        if (stopRaster)
                            return;
                        SDL_IOStream *io = SDL_IOFromConstMem(svg.data(), svg.size());
                        SDL_Surface *surf = io ? IMG_LoadSizedSVG_IO(io, w, h) : nullptr;
                        if (io)
                            SDL_CloseIO(io);
                        if (surf)
                        {
                            std::lock_guard<std::mutex> g(readyLock);
                            ready[key] = surf;
                        }
                    }
                });
            }

            // ── the top bar ──
            ui::TopBar::Model bar(bool setupOpen)
            {
                std::string from = presetAt < 0 ? "NOT SAVED"
                                   : (presetAt < static_cast<int>(presets.size()) && presets[static_cast<size_t>(presetAt)].factory >= 0 ? "FACTORY " : "YOURS ") +
                                         std::to_string(presetAt + 1) + " / " + std::to_string(presets.size());
                from += "  ·  CC " + ov.name() + "  ·  " + p.status();
                if (naming)
                    from = "NAME IT  ·  ENTER SAVES  ·  ESC LEAVES";
                else if (!ov.arm.empty())
                    from = "LEARN  ·  MOVE A KNOB, A FADER OR A BUTTON  ·  ESC LEAVES";
                else if (SDL_GetTicks() < ov.movedAt + 1500 && !ov.moved.empty())
                    from = ov.moved;
                else if (SDL_GetTicks() < noticeUntil)
                    from = notice;
                else if (ov.shown)
                    from = "CC " + ov.name() + "  ·  CLICK: LEARN  ·  RIGHT: FORGET";
                ui::TopBar::Model m;
                m.name = p.barName();
                m.number = p.barNumber();
                m.numberX = p.barNumberX();
                m.status = from;
                m.title = presetName;
                m.changed = v.empty() ? p.modified() : v != loaded;
                m.naming = naming;
                m.nameEdit = nameEdit;
                m.cc = ov.shown;
                m.setup = setupOpen;
                return m;
            }
            static std::string barKey(const ui::TopBar::Model &m)
            {
                return m.status + "|" + m.title + "|" + std::to_string(m.changed) + std::to_string(m.naming) + m.nameEdit + std::to_string((SDL_GetTicks() / 500) % 2 && m.naming) +
                       std::to_string(m.cc) + std::to_string(m.setup);
            }

            // ── a frame: into the canvas, the part that changed ──
            void drawControls(const std::vector<Control> &ctls, const Clip &clip)
            {
                const panelkit::Theme &t = cv.theme;
                for (const Control &c : ctls)
                {
                    if (!clip.touches(areaOf(c)))
                        continue;
                    const float val = valueOf(c);
                    if (p.drawControl(cv, c, val))
                        continue;
                    switch (c.kind)
                    {
                    case Kind::Knob:
                        cv.cap(c.x, c.y, c.a, val);
                        if (!c.label.empty())
                        {
                            cv.say(c.label, c.x, c.y + c.a + 22 * c.text, 10 * c.text, t.ink, 1, true, false, 1.6f, Align::Centre);
                            if (c.param >= 0 && !c.v)
                                cv.say(p.paramText(c.param, val), c.x, c.y + c.a + 35 * c.text, 10 * c.text, t.accent, 1, false, true, 0, Align::Centre, false);
                        }
                        break;
                    case Kind::Fader:
                        cv.faderCap(c.x, c.y, c.a, val, -1);
                        cv.say(c.label, c.x, c.y + c.a + 20 * c.text, 10 * c.text, t.ink, 1, true, false, 1.6f, Align::Centre);
                        break;
                    case Kind::Button:
                    case Kind::Pad:
                        cv.box(c.x, c.y, c.a, c.b, c.on);
                        cv.say(c.label, c.x + c.a / 2, c.y + c.b / 2 + 4 * c.text, 9 * c.text, c.on ? t.accent : t.dim, 1, true, false, 1, Align::Centre);
                        break;
                    case Kind::Switch: // the label above, the choices side by side, the chosen lit
                    {
                        const int n = static_cast<int>(c.options.size());
                        const int chosen = n > 1 ? static_cast<int>(std::lround(val * static_cast<float>(n - 1))) : 0;
                        if (!c.label.empty())
                            cv.say(c.label, c.x, c.y - 6, 10 * c.text, t.ink, 1, true, false, 1.6f, Align::Left);
                        for (int k = 0; k < n; ++k)
                        {
                            const float bx = c.x + static_cast<float>(k) * (c.a + 3);
                            cv.box(bx, c.y, c.a, c.b, k == chosen);
                            cv.say(c.options[static_cast<size_t>(k)], bx + c.a / 2, c.y + c.b / 2 + 4 * c.text, 9 * c.text, k == chosen ? t.accent : t.dim, 1, true, false, 1,
                                   Align::Centre);
                        }
                        break;
                    }
                    case Kind::Toggle:
                        cv.led(c.x, c.y, val > 0.5f);
                        cv.say(c.label, c.x, c.y - 14, 10 * c.text, t.ink, 1, true, false, 1.6f, Align::Centre);
                        break;
                    case Kind::XY: cv.led(c.x + val * c.a, c.y + (1 - valueY(c)) * c.b, true); break;
                    default: break;
                    }
                }
                if (ov.shown && !p.overOpen()) // the overlay's badges
                    for (const Control &c : ctls)
                    {
                        const std::string id = idOf(c);
                        if (id.empty() || c.kind == Kind::Hot)
                            continue;
                        const bool armed = ov.arm == id;
                        const int cc = ov.ccOf(p.aliases(id));
                        if (cc < 0 && !armed)
                            continue;
                        float bx = c.x + c.a - 24, by = c.y + 2;
                        if (c.kind == Kind::Knob)
                            bx = c.x + c.a * 0.7f, by = c.y - c.a - 4;
                        else if (c.kind == Kind::Fader)
                            bx = c.x + 8;
                        else if (c.kind == Kind::Switch)
                            bx = c.x + 4, by = c.y - 8;
                        else if (c.kind == Kind::Toggle)
                            bx = c.x + 6, by = c.y - 16;
                        bx += c.badgeX, by += c.badgeY;
                        const bool lit = armed ? (SDL_GetTicks() / 250) % 2 == 0 : ov.caught(cc);
                        cv.rect(bx, by, 22, 11, armed || lit ? t.accent : t.bg, 0.95f);
                        cv.rect(bx, by, 22, 11, t.accent, 0.8f, false);
                        cv.say(armed ? "?" : std::to_string(cc), bx + 11, by + 8.5f, 6.5f, armed || lit ? t.bg : t.accent, 1, true, true, 0, Align::Centre, false);
                    }
            }
        };
    } // namespace

    int run(Product &product, int argc, char **argv)
    {
        const std::string pid = product.id();
        SDL_SetHint(SDL_HINT_APP_ID, pid.c_str());
        const char *remoteLink = nullptr;
        uint64_t hostParent = 0;
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (std::strcmp(argv[i], "--remote") == 0)
                remoteLink = argv[i + 1];
            if (std::strcmp(argv[i], "--parent") == 0)
                hostParent = std::strtoull(argv[i + 1], nullptr, 10);
        }
        const char *who = product.title();
        if (remoteLink)
            editorlog::say(who, "panel", "started: link %s, parent %llu", remoteLink, static_cast<unsigned long long>(hostParent));
        if (hostParent)
            hostwin::prepare();
        if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init())
        {
            if (remoteLink)
                editorlog::say(who, "panel", "SDL_Init failed: %s", SDL_GetError());
            std::fprintf(stderr, "%s: %s\n", pid.c_str(), SDL_GetError());
            return 1;
        }
        SDL_Window *win = hostParent ? SDL_CreateWindow(who, 1280, 720, SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY)
                                     : SDL_CreateWindow(who, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (remoteLink)
            editorlog::say(who, "panel", "window %s (video %s)", win ? "made" : SDL_GetError(), SDL_GetCurrentVideoDriver());
        if (win && hostParent && hostwin::embedInHost(win, hostParent))
            editorlog::say(who, "panel", "inside the host's window");
        else if (win && hostParent)
        {
            editorlog::say(who, "panel", "COULD NOT go inside the host's window: %s", hostwin::lastError().c_str());
            std::fprintf(stderr, "%s: cannot go into the host's window: a window of its own\n", pid.c_str());
            hostParent = 0;
            SDL_SetWindowBordered(win, true);
            SDL_SetWindowResizable(win, true);
            SDL_ShowWindow(win);
        }
        SDL_Renderer *ren = win ? SDL_CreateRenderer(win, nullptr) : nullptr;
        if (!ren)
        {
            std::fprintf(stderr, "%s: %s\n", pid.c_str(), SDL_GetError());
            return 1;
        }
        const char *vs = SDL_getenv("OBS_PANEL_VSYNC");
        SDL_SetRenderVSync(ren, vs && vs[0] == '0' ? 0 : 1);

        App a(product);
        a.win = win, a.ren = ren, a.cv.r = ren;
        a.host.overlay = &a.ov;
        a.host.whole = [&a] { a.whole = true; };
        a.host.shownPreset = [&a](int k, const std::string &name) {
            a.presetAt = std::clamp(k, -1, static_cast<int>(a.presets.size()) - 1); // -1: none (the start)
            a.presetName = name;
            a.loaded = a.v;
            a.whole = true;
        };
        a.host.notice = [&a](const std::string &n) {
            a.notice = n;
            a.noticeUntil = SDL_GetTicks() + 2500;
        };
        a.host.presetAt = [&a] { return a.presetAt; };
        a.host.loadPreset = [&a](int k) { a.presetReq = k; };
        product.host = &a.host;
        a.settings = io::loadSettings(pid); // what SETUP chose
        a.cv.theme = panelkit::loadTheme(a.settings.palette);
        const int np = product.params();
        a.v.resize(static_cast<size_t>(np));
        for (int i = 0; i < np; ++i)
            a.v[static_cast<size_t>(i)] = product.paramDefault(i);
        a.presets = product.presets();
        int first = product.startName() ? -1 : 0; // a state of the product's own, or its first preset
        for (int i = 1; i + 1 < argc; ++i)
            if (std::strcmp(argv[i], "--preset") == 0)
            {
                const int n = std::atoi(argv[i + 1]);
                if (n > 0)
                    first = std::min(n, static_cast<int>(a.presets.size())) - 1;
                for (size_t k = 0; k < a.presets.size(); ++k)
                    if (SDL_strcasecmp(a.presets[k].name.c_str(), argv[i + 1]) == 0)
                        first = static_cast<int>(k);
            }
        if (first >= 0)
            a.loadPresetAt(first);
        else
        {
            a.presetAt = -1;
            a.presetName = product.startName();
            a.loaded = a.v;
        }

        Engine &e = product.engine();
        if (remoteLink)
        {
            e.connect(remoteLink);
            SDL_SetWindowTitle(win, (std::string(who) + " · plugin").c_str());
        }
        else
            e.start(a.v, a.settings);
        if (!hostParent && e.remote() && product.crop() > 0) // the plugin's editor in a window of its own: as wide as what it shows
            SDL_SetWindowSize(win, static_cast<int>(std::lround((W - product.crop()) / H * 720)), 720);
        else if (!hostParent)
            SDL_SetWindowAspectRatio(win, W / H, W / H);
        a.sent = a.v;
        a.ov.load(pid, product.controllers());
        if (!a.settings.ccMap.empty())
            setup::applyCcMap(a.ov, a.settings.ccMap);
        setup::Page setupPage(pid, who, a.settings, e.midiIn(), a.ov, [&] { return e.reopenAudio(a.settings); },
                              [&]() -> const io::AudioOut & { return e.audioOut(); });
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--cc") == 0)
                a.ov.shown = true;
            if (std::strcmp(argv[i], "--setup") == 0)
                setupPage.show(true);
        }
        bool lookChanged = false;
        setupPage.inHost = e.remote();
        setupPage.onLook = [&lookChanged] { lookChanged = true; };

        // the computer keyboard: Z..M from C3, Q..U the octave up
        const std::map<SDL_Scancode, int> piano{{SDL_SCANCODE_Z, 48}, {SDL_SCANCODE_S, 49}, {SDL_SCANCODE_X, 50}, {SDL_SCANCODE_D, 51}, {SDL_SCANCODE_C, 52},
                                                {SDL_SCANCODE_V, 53}, {SDL_SCANCODE_G, 54}, {SDL_SCANCODE_B, 55}, {SDL_SCANCODE_H, 56}, {SDL_SCANCODE_N, 57},
                                                {SDL_SCANCODE_J, 58}, {SDL_SCANCODE_M, 59}, {SDL_SCANCODE_Q, 60}, {SDL_SCANCODE_2, 61}, {SDL_SCANCODE_W, 62},
                                                {SDL_SCANCODE_3, 63}, {SDL_SCANCODE_E, 64}, {SDL_SCANCODE_R, 65}, {SDL_SCANCODE_5, 66}, {SDL_SCANCODE_T, 67},
                                                {SDL_SCANCODE_6, 68}, {SDL_SCANCODE_Y, 69}, {SDL_SCANCODE_7, 70}, {SDL_SCANCODE_U, 71}};

        // the panel trying itself (panel units): OBS_PANEL_SHOT, OBS_PANEL_CLICKS, OBS_PANEL_DRAG, OBS_PANEL_TIMES
        std::string shotPath;
        uint64_t shotAt = 0;
        if (const char *shot = SDL_getenv("OBS_PANEL_SHOT"))
        {
            shotPath = shot;
            const auto colon = shotPath.rfind(':');
            shotAt = SDL_GetTicks() + static_cast<uint64_t>((colon == std::string::npos ? 3.0 : std::atof(shotPath.c_str() + colon + 1)) * 1000);
            if (colon != std::string::npos)
                shotPath.resize(colon);
        }
        struct Click { uint64_t at; float x, y; bool done; };
        std::vector<Click> clicks;
        if (const char *c = SDL_getenv("OBS_PANEL_CLICKS"))
        {
            const std::string all = c;
            for (size_t at = 0; at < all.size();)
            {
                const size_t end = std::min(all.find(';', at), all.size());
                float secs = 0, x = 0, y = 0;
                if (std::sscanf(all.substr(at, end - at).c_str(), "%f:%f,%f", &secs, &x, &y) == 3)
                    clicks.push_back({SDL_GetTicks() + static_cast<uint64_t>(secs * 1000), x, y, false});
                at = end + 1;
            }
        }
        struct Drag { uint64_t at; float x, y, dy; int step; };
        std::vector<Drag> drags;
        if (const char *d = SDL_getenv("OBS_PANEL_DRAG"))
        {
            float secs = 0, x = 0, y = 0, dy = -2;
            if (std::sscanf(d, "%f:%f,%f,%f", &secs, &x, &y, &dy) >= 3)
                drags.push_back({SDL_GetTicks() + static_cast<uint64_t>(secs * 1000), x, y, dy, -1});
        }
        const bool times = SDL_getenv("OBS_PANEL_TIMES") != nullptr;
        uint64_t timesAt = SDL_GetTicksNS(), drawNs = 0, worstNs = 0, presentNs = 0;
        int frames = 0, events = 0;

        uint64_t lastPalette = 0, lastDetect = 0;
        std::filesystem::file_time_type paletteTime{};
        bool wasSetup = false, wasShown = false;
        bool running = true;
        while (running)
        {
            int pw, ph;
            SDL_GetCurrentRenderOutputSize(ren, &pw, &ph);
            const float crop = e.remote() ? product.crop() : 0.0f; // in a host: a column on the left left out
            const float scale = std::min(static_cast<float>(pw) / (W - crop), static_cast<float>(ph) / H);
            if (std::fabs(scale - a.cv.scale) > 0.005f)
            {
                a.cv.scale = scale;
                a.cv.drop();
                a.dropBases();
                if (a.canvas)
                    SDL_DestroyTexture(a.canvas);
                a.canvas = nullptr;
            }
            const float ox = std::floor((static_cast<float>(pw) - (W - crop) * scale) / 2) - crop * scale, oy = std::floor((static_cast<float>(ph) - H * scale) / 2);
            const float density = SDL_GetWindowPixelDensity(win);
            auto toPanel = [&](float wx, float wy) { return SDL_FPoint{(wx * density - ox) / a.cv.scale, (wy * density - oy) / a.cv.scale}; };
            auto toWindow = [&](float x, float y) { return SDL_FPoint{(x * a.cv.scale + ox) / density, (y * a.cv.scale + oy) / density}; };
            {
                float mx, my;
                SDL_GetMouseState(&mx, &my);
                a.host.pointer = toPanel(mx, my);
            }
            std::vector<Control> ctls = product.layout(a.v);

            for (Click &k : clicks)
                if (!k.done && SDL_GetTicks() >= k.at)
                {
                    k.done = true;
                    const SDL_FPoint w = toWindow(k.x, k.y);
                    SDL_Event ev{};
                    ev.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN, ev.button.button = SDL_BUTTON_LEFT, ev.button.x = w.x, ev.button.y = w.y;
                    SDL_PushEvent(&ev);
                    ev.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
                    SDL_PushEvent(&ev);
                }
            for (Drag &d : drags)
                while (d.step < 125 && SDL_GetTicks() >= d.at + static_cast<uint64_t>(std::max(d.step, 0)) * 8)
                {
                    const SDL_FPoint w = toWindow(d.x, d.y + d.dy * static_cast<float>(std::max(d.step, 0)));
                    SDL_Event ev{};
                    ev.type = d.step < 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : d.step == 124 ? SDL_EVENT_MOUSE_BUTTON_UP : SDL_EVENT_MOUSE_MOTION;
                    if (ev.type == SDL_EVENT_MOUSE_MOTION)
                        ev.motion.x = w.x, ev.motion.y = w.y, ev.motion.yrel = 0;
                    else
                        ev.button.button = SDL_BUTTON_LEFT, ev.button.x = w.x, ev.button.y = w.y;
                    SDL_PushEvent(&ev);
                    ++d.step;
                }

            SDL_Event ev;
            while (SDL_PollEvent(&ev))
            {
                ++events;
                switch (ev.type)
                {
                case SDL_EVENT_QUIT: running = false; break;
                case SDL_EVENT_WINDOW_EXPOSED: a.whole = true; break;
                case SDL_EVENT_TEXT_INPUT:
                    if (a.naming && a.nameEdit.size() < 32)
                        a.nameEdit += ev.text.text;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (a.naming)
                    {
                        if (ev.key.key == SDLK_ESCAPE)
                            a.naming = false;
                        else if (ev.key.key == SDLK_BACKSPACE && !a.nameEdit.empty())
                        {
                            do // a whole UTF-8 character
                                a.nameEdit.pop_back();
                            while (!a.nameEdit.empty() && (static_cast<unsigned char>(a.nameEdit.back()) & 0xC0) == 0x80);
                        }
                        else if ((ev.key.key == SDLK_RETURN || ev.key.key == SDLK_KP_ENTER) && !a.nameEdit.empty())
                        {
                            std::string path;
                            a.naming = false;
                            if (product.savePreset(a.nameEdit, a.v, path))
                            {
                                a.presets = product.presets();
                                for (size_t k = 0; k < a.presets.size(); ++k)
                                    if (a.presets[k].path == path)
                                        a.presetAt = static_cast<int>(k);
                                a.presetName = a.nameEdit;
                                a.loaded = a.v;
                                a.notice = "SAVED";
                            }
                            else
                                a.notice = "COULD NOT SAVE IT";
                            a.noticeUntil = SDL_GetTicks() + 2500;
                        }
                        break;
                    }
                    if (ev.key.key == SDLK_ESCAPE && setupPage.open)
                        setupPage.show(false);
                    else if (ev.key.key == SDLK_ESCAPE && product.overOpen())
                    {
                        product.closeOver();
                        a.whole = true;
                    }
                    else if (ev.key.key == SDLK_ESCAPE && !a.ov.arm.empty())
                        a.ov.arm.clear();
                    else if (ev.key.key == SDLK_ESCAPE && a.ov.shown)
                        a.ov.shown = false;
                    else if (ev.key.key == SDLK_ESCAPE)
                        running = false;
                    else if (ev.key.key == SDLK_F2 && (SDL_GetModState() & SDL_KMOD_SHIFT)) // the next controller's map
                    {
                        a.ov.next();
                        a.ov.arm.clear();
                    }
                    else if (ev.key.key == SDLK_F2)
                    {
                        a.ov.shown = !a.ov.shown;
                        a.ov.arm.clear();
                    }
                    else if (!ev.key.repeat && !product.key(ev.key.scancode, true) && piano.count(ev.key.scancode))
                        e.noteOn(piano.at(ev.key.scancode), (SDL_GetModState() & SDL_KMOD_SHIFT) ? 1.0f : 0.8f);
                    break;
                case SDL_EVENT_KEY_UP:
                    if (!a.naming && !product.key(ev.key.scancode, false) && piano.count(ev.key.scancode))
                        e.noteOff(piano.at(ev.key.scancode));
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                {
                    const SDL_FPoint q = toPanel(ev.button.x, ev.button.y);
                    if (setupPage.click(q.x, q.y))
                        break;
                    if (product.press(q.x, q.y, ev.button.button)) // the product's layer first
                    {
                        a.whole = true;
                        break;
                    }
                    const ui::TopBar::Hit bh = ui::TopBar::hit(q.x, q.y);
                    if (bh != ui::TopBar::None)
                    {
                        switch (bh)
                        {
                        case ui::TopBar::Prev: a.presetReq = a.stepPreset(-1); break;
                        case ui::TopBar::Title:
                            if (product.clickTitle())
                            {
                                a.whole = true;
                                break;
                            }
                            a.presetReq = a.stepPreset(1);
                            break;
                        case ui::TopBar::Next: a.presetReq = a.stepPreset(1); break;
                        case ui::TopBar::Save:
                            a.naming = true;
                            a.nameEdit = a.presetName;
                            break;
                        case ui::TopBar::Cc:
                            if (ev.button.button == SDL_BUTTON_RIGHT)
                                a.ov.next();
                            else
                                a.ov.shown = !a.ov.shown;
                            a.ov.arm.clear();
                            break;
                        case ui::TopBar::Setup: setupPage.show(true); break; // in a host: LOOK only
                        default: break;
                        }
                        break;
                    }
                    int k = -1;
                    for (size_t i = ctls.size(); i-- > 0;)
                        if (hits(ctls[i], q.x, q.y))
                        {
                            k = static_cast<int>(i);
                            break;
                        }
                    if (k < 0)
                    {
                        if (q.y < BAR && product.clickBar(q.x, q.y, a.v))
                            a.whole = true;
                        break;
                    }
                    const Control &c = ctls[static_cast<size_t>(k)];
                    const uint64_t now = SDL_GetTicks();
                    const bool twice = k == a.lastCtl && now - a.lastClick < 350;
                    a.lastClick = now;
                    a.lastCtl = k;
                    if (const std::string id = a.idOf(c); a.ov.shown && !id.empty()) // the overlay: a click arms the control, a right click forgets its CCs
                    {
                        if (ev.button.button == SDL_BUTTON_RIGHT)
                        {
                            a.ov.forget(product.aliases(id));
                            a.ov.arm.clear();
                        }
                        else
                            a.ov.arm = a.ov.arm == id ? std::string() : id;
                        break;
                    }
                    if (c.group >= 0)
                        product.pressed(c);
                    const bool valued = App::valued(c);
                    if (c.press) // the product's answer first; else a left press drags
                    {
                        if (!c.press(ev.button.button, twice) && valued && ev.button.button == SDL_BUTTON_LEFT)
                        {
                            a.drag = k;
                            a.dragY = ev.button.y;
                            a.dragV = a.valueOf(c);
                        }
                    }
                    else if ((c.kind == Kind::Button || c.kind == Kind::Hot) && c.click)
                        c.click(ev.button.button);
                    else if (c.kind == Kind::Switch && valued) // the choice clicked
                    {
                        if (const int n = static_cast<int>(c.options.size()); n > 1)
                            a.setValue(c, static_cast<float>(std::max(0, optionAt(c, q.x))) / static_cast<float>(n - 1));
                    }
                    else if (c.kind == Kind::Toggle && valued)
                        a.setValue(c, a.valueOf(c) > 0.5f ? 0.0f : 1.0f);
                    else if (c.kind == Kind::XY && valued && ev.button.button == SDL_BUTTON_LEFT) // the point pressed, then dragged
                    {
                        a.drag = k;
                        a.setPoint(c, q.x, q.y);
                    }
                    else if (c.kind == Kind::Pad && c.click) // higher on the pad, harder
                        c.click(static_cast<int>(std::lround(127.0f * std::clamp(1.15f - (q.y - c.y) / c.b * 0.8f, 0.1f, 1.0f))));
                    else if (c.kind == Kind::Cell && valued && (ev.button.button == SDL_BUTTON_RIGHT || twice))
                        a.setValue(c, 0);
                    else if (valued && (c.kind == Kind::Knob || c.kind == Kind::Fader || c.kind == Kind::Cell) && ev.button.button == SDL_BUTTON_LEFT)
                    {
                        if (twice && c.kind != Kind::Cell)
                            a.setValue(c, a.defaultOf(c));
                        else
                        {
                            a.drag = k;
                            a.dragY = ev.button.y;
                            if (c.kind == Kind::Fader && c.jump)
                                a.setValue(c, 1 - (q.y - c.y) / c.a);
                            a.dragV = a.valueOf(c);
                        }
                    }
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    a.drag = -1;
                    const SDL_FPoint q = toPanel(ev.button.x, ev.button.y);
                    product.release(q.x, q.y);
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                    if (a.drag >= 0 && static_cast<size_t>(a.drag) < ctls.size() && ctls[static_cast<size_t>(a.drag)].kind == Kind::XY)
                    {
                        const SDL_FPoint q = toPanel(ev.motion.x, ev.motion.y);
                        a.setPoint(ctls[static_cast<size_t>(a.drag)], q.x, q.y);
                    }
                    else if (a.drag >= 0 && static_cast<size_t>(a.drag) < ctls.size())
                    {
                        const Control &c = ctls[static_cast<size_t>(a.drag)];
                        const bool fine = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                        if (fine)
                        {
                            a.dragV = a.valueOf(c);
                            a.dragY = ev.motion.y - ev.motion.yrel;
                        }
                        const float dy = (a.dragY - ev.motion.y) * density / a.cv.scale; // panel units
                        a.setValue(c, a.dragV + dy / (fine ? c.travel * 10 : c.travel));
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                {
                    if (setupPage.open || product.overOpen())
                        break;
                    float mx, my;
                    SDL_GetMouseState(&mx, &my);
                    const SDL_FPoint q = toPanel(mx, my);
                    if (product.wheel(q.x, q.y, ev.wheel.y)) // the product's own (a knob's modulation)
                        break;
                    for (size_t i = ctls.size(); i-- > 0;)
                        if (hits(ctls[i], q.x, q.y))
                        {
                            const Control &c = ctls[i];
                            if (!App::valued(c) || c.kind == Kind::Button || c.kind == Kind::Hot || c.kind == Kind::Pad || c.kind == Kind::Toggle || c.kind == Kind::XY)
                                break;
                            const int st = a.stepsOf(c);
                            const float step = st > 1 ? 1.0f / static_cast<float>(st - 1) : (SDL_GetModState() & SDL_KMOD_SHIFT) ? 0.002f : 0.02f;
                            a.setValue(c, a.valueOf(c) + (ev.wheel.y > 0 ? step : -step));
                            break;
                        }
                    break;
                }
                default: break;
                }
            }
            if (a.naming != SDL_TextInputActive(win))
                a.naming ? SDL_StartTextInput(win) : SDL_StopTextInput(win);

            // the controller: its CCs through the overlay
            {
                int ch, cc;
                float val;
                while (e.pollCc(ch, cc, val))
                    if (a.ov.onCc(ch, cc, val, [&a](const std::string &id) { return a.resolve(id); }))
                        a.ov.movedAt = SDL_GetTicks();
                if (SDL_GetTicks() - lastDetect > 3000)
                {
                    lastDetect = SDL_GetTicks();
                    if (const ccov::Map *mp = a.ov.map(); mp && mp->match.empty() && a.settings.ccMap.empty())
                        a.ov.detect();
                }
            }
            if (a.presetReq >= 0)
            {
                a.loadPresetAt(a.presetReq);
                a.presetReq = -1;
            }
            if (e.remote()) // what the host changed: shown, not sent back
            {
                int id;
                float val;
                bool program = false;
                while (e.pollRemote(id, val))
                    if (id < 0)
                    {
                        const int k = static_cast<int>(std::lround(val));
                        if (k >= 0 && k < static_cast<int>(a.presets.size()))
                            a.presetAt = k, a.presetName = a.presets[static_cast<size_t>(k)].name, program = true;
                    }
                    else if (id < np)
                        a.v[static_cast<size_t>(id)] = a.sent[static_cast<size_t>(id)] = val;
                if (program)
                    a.loaded = a.v, a.whole = true;
                if (!e.remoteAlive() || (hostParent && !hostwin::hostAlive(hostParent))) // the plugin, or its window, is gone
                    running = false;
            }
            product.tick(); // the product's own: its values to its engine, what its host sent
            // what changed goes to the engine
            for (int i = 0; i < np; ++i)
                if (a.v[static_cast<size_t>(i)] != a.sent[static_cast<size_t>(i)])
                {
                    e.setParam(i, a.v[static_cast<size_t>(i)]);
                    a.sent[static_cast<size_t>(i)] = a.v[static_cast<size_t>(i)];
                }

            const uint64_t now = SDL_GetTicks();
            if (now - lastPalette > 1000)
            {
                lastPalette = now;
                std::error_code ec;
                auto tm = std::filesystem::last_write_time(panelkit::kPalette, ec);
                if (!ec && tm != paletteTime)
                {
                    const bool firstLook = paletteTime == std::filesystem::file_time_type{};
                    paletteTime = tm;
                    if (!firstLook && a.settings.palette.empty()) // the system's palette, followed
                        lookChanged = true;
                }
            }
            if (lookChanged)
            {
                lookChanged = false;
                a.cv.theme = panelkit::loadTheme(a.settings.palette);
                a.cv.drop();
                a.dropBases();
                a.whole = true;
            }
            if (hostParent) // inside a host's window: its size (0 x 0 at first in some hosts; resized later)
                hostwin::followHost(win, hostParent);

            if (times && SDL_GetTicksNS() - timesAt >= 2000000000ull)
            {
                std::fprintf(stderr, "[%s] %d frames, draw mean %.1f ms worst %.1f ms, present mean %.1f ms, %d events\n", pid.c_str(), frames,
                             frames ? static_cast<double>(drawNs) / frames / 1e6 : 0.0, static_cast<double>(worstNs) / 1e6,
                             frames ? static_cast<double>(presentNs) / frames / 1e6 : 0.0, events);
                timesAt = SDL_GetTicksNS(), drawNs = worstNs = presentNs = 0, frames = events = 0;
            }

            // ── what to draw: the whole panel, or the part that changed; nothing changed, no frame ──
            ctls = product.layout(a.v); // as it is after the events (an engine's knobs)
            if (a.drag >= 0 && static_cast<size_t>(a.drag) < ctls.size())
                ctls[static_cast<size_t>(a.drag)].held = true;
            const std::string key = product.layoutKey(a.v);
            const ui::TopBar::Model bm = a.bar(setupPage.open);
            const std::string bk = App::barKey(bm);
            bool whole = a.whole || !a.canvas || key != a.drawnKey || setupPage.open || wasSetup || a.ov.shown || wasShown || (shotAt && now >= shotAt) ||
                         product.wantsWhole() || a.drawn.size() != a.v.size();
            wasSetup = setupPage.open;
            wasShown = a.ov.shown;
            float x0 = W, y0 = H, x1 = 0, y1 = 0;
            auto add = [&](const SDL_FRect &r) {
                if (r.w >= W && r.h >= H)
                    whole = true;
                x0 = std::min(x0, r.x), y0 = std::min(y0, r.y), x1 = std::max(x1, r.x + r.w), y1 = std::max(y1, r.y + r.h);
            };
            if (!whole)
            {
                for (int i = 0; i < np; ++i)
                {
                    if (a.v[static_cast<size_t>(i)] == a.drawn[static_cast<size_t>(i)])
                        continue;
                    for (const SDL_FRect &r : product.damageOf(i, a.v))
                        add(r);
                    for (const Control &c : ctls)
                        if (c.param == i)
                            add(areaOf(c));
                }
                if (a.seen.size() != ctls.size())
                    whole = true;
                else
                    for (size_t i = 0; i < ctls.size(); ++i)
                    {
                        const App::Seen &was = a.seen[i];
                        const Control &c = ctls[i];
                        if (was.v != a.valueOf(c) || was.vy != App::valueY(c) || was.on != c.on || was.held != c.held || was.label != c.label)
                            add(areaOf(c));
                    }
                if (bk != a.drawnBar)
                    add({0, 0, W, BAR});
                for (const SDL_FRect &r : product.live())
                    add(r);
            }
            if (!whole && x1 <= x0)
            {
                SDL_WaitEventTimeout(nullptr, drags.empty() || drags.back().step >= 125 ? 33 : 4);
                continue;
            }

            const uint64_t t0 = SDL_GetTicksNS();
            if (!a.canvas)
            {
                a.canvas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, static_cast<int>(W * a.cv.scale), static_cast<int>(H * a.cv.scale));
                SDL_SetTextureBlendMode(a.canvas, SDL_BLENDMODE_NONE);
            }
            SDL_SetRenderTarget(ren, a.canvas);
            SDL_SetRenderViewport(ren, nullptr);
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            Clip clip;
            if (!whole)
            {
                clip = {x0, y0, x1 - x0, y1 - y0};
                const SDL_Rect px{static_cast<int>(std::floor(clip.x * a.cv.scale)), static_cast<int>(std::floor(clip.y * a.cv.scale)),
                                  static_cast<int>(std::ceil(clip.w * a.cv.scale)) + 2, static_cast<int>(std::ceil(clip.h * a.cv.scale)) + 2};
                SDL_SetRenderClipRect(ren, &px);
            }
            const SDL_FRect all{0, 0, W * a.cv.scale, H * a.cv.scale};
            SDL_RenderTexture(ren, a.baseFor(key), nullptr, &all);
            product.drawBody(a.cv, a.v, clip);
            a.drawControls(ctls, clip);
            if (clip.touches(0, 0, W, BAR))
                ui::TopBar::draw(painterOf(a.cv), bm);
            product.drawOver(a.cv, clip);
            setupPage.draw(painterOf(a.cv));
            SDL_SetRenderClipRect(ren, nullptr);
            SDL_SetRenderTarget(ren, nullptr);
            a.drawn = a.v;
            a.seen.clear();
            for (const Control &c : ctls)
                a.seen.push_back({a.valueOf(c), App::valueY(c), c.on, c.held, c.label});
            a.drawnKey = key;
            a.drawnBar = bk;
            a.whole = false;
            if (!a.raster.joinable())
                a.rasterizeRest(); // after the first frame: the other layouts' backgrounds

            const SDL_FColor bg = panelkit::color(a.cv.theme.bg);
            SDL_SetRenderViewport(ren, nullptr);
            SDL_SetRenderDrawColorFloat(ren, bg.r, bg.g, bg.b, 1);
            SDL_RenderClear(ren);
            const SDL_FRect shown{crop * a.cv.scale, 0, (W - crop) * a.cv.scale, H * a.cv.scale}; // the column cropped never shows
            const SDL_FRect at{ox + crop * a.cv.scale, oy, (W - crop) * a.cv.scale, H * a.cv.scale};
            SDL_RenderTexture(ren, a.canvas, &shown, &at);
            if (shotAt && SDL_GetTicks() >= shotAt)
            {
                shotAt = 0;
                if (SDL_Surface *shot = SDL_RenderReadPixels(ren, nullptr))
                {
                    SDL_SaveBMP(shot, shotPath.c_str());
                    SDL_DestroySurface(shot);
                }
            }
            const uint64_t ns = SDL_GetTicksNS() - t0; // drawn, before the wait for the screen
            SDL_RenderPresent(ren);
            presentNs += SDL_GetTicksNS() - t0 - ns;
            drawNs += ns, worstNs = std::max(worstNs, ns), ++frames;
            { // at most ~60 frames a second where nothing waits for the screen (no vsync, an offscreen run)
                static uint64_t lastFrame = 0;
                const uint64_t nowNs = SDL_GetTicksNS();
                if (lastFrame && nowNs - lastFrame < 16'000'000)
                    SDL_DelayNS(16'000'000 - (nowNs - lastFrame));
                lastFrame = SDL_GetTicksNS();
            }
        }
        a.dropBases();
        if (a.canvas)
            SDL_DestroyTexture(a.canvas);
        a.cv.drop();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        TTF_Quit();
        if (remoteLink)
            editorlog::say(who, "panel", "ends (the plugin %s)", e.remoteAlive() ? "still there: closed here" : "gone");
        SDL_Quit();
        return 0;
    }
} // namespace synth::panel
