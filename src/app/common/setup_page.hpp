#pragma once
// SETUP: the page over a panel where the person chooses the audio output (backend, output, buffer), what plays
// the product (the MIDI inputs, the channel, the controller's CC map) and its colours (LOOK). In a host (the panel as
// a plugin's editor) only LOOK: the audio and the MIDI are the host's. Shared by the panels: each lends its own
// drawing (Painter); the page works on the product's AudioOut and MidiIn (platform_io.hpp) and its CC overlay,
// and saves every change to <data>/<product>/settings.json at once. Panel units: 1600 × 900.
#include "cc_overlay.hpp"
#include "palette_choice.hpp"
#include "painter.hpp"
#include "platform_io.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace synth::setup
{
    using ui::Painter;

    // the CC map SETUP chose ("" the controller present's), on the overlay
    inline void applyCcMap(ccov::Overlay &ov, const std::string &name)
    {
        if (name.empty())
        {
            ov.detect();
            return;
        }
        for (size_t i = 0; i < ov.maps.size(); ++i)
            if (ov.maps[i].name == name)
            {
                while (ov.active != static_cast<int>(i)) // next() also finds the map's client (its pads' notes)
                    ov.next();
                return;
            }
    }

    class Page
    {
    public:
        bool open = false;
        bool inHost = false;          // the panel is a plugin's editor: LOOK only
        std::function<void()> onLook; // the palette changed: the panel takes it (the settings' palette)

        Page(std::string product, const char *audioName, io::Settings &settings, io::MidiIn &midi, ccov::Overlay &ov,
             std::function<bool()> reopen, std::function<const io::AudioOut &()> out)
            : product_(std::move(product)), audioName_(audioName), s_(settings), midi_(midi), ov_(ov), reopen_(std::move(reopen)), out_(std::move(out)) {}

        void show(bool on)
        {
            open = on;
            if (on)
            {
                devices_ = io::audioDevices(backendShown());
                sources_ = midi_.sources();
                lastLook_ = SDL_GetTicks();
            }
        }

        void draw(const Painter &p)
        {
            hot_.clear();
            if (!open)
                return;
            if (SDL_GetTicks() - lastLook_ > 2000) // a controller plugged in or out
            {
                sources_ = midi_.sources();
                lastLook_ = SDL_GetTicks();
            }
            p.rect(0, 0, 1600, 900, "#000000", 0.62f, true);
            const float X = 380, Y = 96, W = 840, H = 720;
            p.rect(X, Y, W, H, p.bg, 0.97f, true);
            p.rect(X, Y, W, H, p.accent, 0.5f, false);
            p.say("SETUP", X + 24, Y + 34, 16, p.ink, 1, true, false, 1.0f, 0);
            p.say(audioName_, X + 120, Y + 34, 10, p.accent, 0.8f, false, true, 1.0f, 0);
            button(p, X + W - 44, Y + 14, 28, 28, "×", false, [this] { open = false; });

            float y = Y + 74;
            if (inHost)
            {
                section(p, X + 24, y, "AUDIO AND MIDI");
                p.say("IN A HOST: ITS TRACK'S MIDI IN, ITS MIXER OUT. THE HOST'S OWN SETTINGS CHOOSE THEM.", X + 24, y + 30, 8, p.dim, 1, false,
                      true, 1.0f, 0);
                look(p, X, y + 70);
                p.say("ESC OR × CLOSES  ·  SAVED: " + io::settingsPath(product_), X + W / 2, Y + H - 16, 7, p.dim, 1, false, true, 1.0f, 1);
                return;
            }

            // ── audio
            section(p, X + 24, y, "AUDIO OUT");
            const io::AudioOut &now = out_();
            std::string state = now.running() ? now.backend() + "  ·  " + (now.device().empty() ? std::string("DEFAULT") : shortName(now.device(), 40)) + "  ·  48 kHz"
                                              : std::string("NO OUTPUT OPEN");
            p.say(state, X + W - 24, y, 8, now.running() ? p.accent : "#d05050", 0.9f, false, true, 1.0f, 2);
            y += 24;
            p.say("BACKEND", X + 24, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            float bx = X + 140;
            button(p, bx, y, 70, 22, "AUTO", s_.backend.empty(), [this] { setBackend(""); });
            bx += 78;
            for (const io::Backend &b : io::audioBackends())
            {
                const std::string name = b.name;
                button(p, bx, y, 100, 22, name, s_.backend == name, [this, name] { setBackend(name); });
                bx += 108;
            }
            y += 34;
            p.say("OUTPUT", X + 24, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            if (s_.backend.empty())
                p.say("THE USUAL ORDER, EACH BACKEND'S DEFAULT (CHOOSE A BACKEND TO CHOOSE ITS OUTPUT)", X + 140, y + 15, 7.5f, p.dim, 1, false, true, 1.0f, 0);
            else
            {
                const int rows = 6, n = static_cast<int>(devices_.size());
                devFirst_ = std::clamp(devFirst_, 0, std::max(0, n - rows));
                for (int k = 0; k < rows && devFirst_ + k < n; ++k)
                {
                    const AudioDevice &d = devices_[static_cast<size_t>(devFirst_ + k)];
                    const bool on = d.id == s_.device;
                    const float ry = y + static_cast<float>(k) * 25;
                    std::string id = d.id;
                    button(p, X + 140, ry, W - 200, 22, "", on, [this, id] { s_.device = id; apply(); });
                    p.say(shortName(d.name, 64), X + 152, ry + 15, 8, on ? p.accent : p.ink, 1, false, false, 1.0f, 0);
                    if (!d.id.empty() && d.id != d.name)
                        p.say(shortName(d.id, 34), X + W - 72, ry + 15, 6.5f, p.dim, 1, false, true, 1.0f, 2);
                }
                if (n > rows)
                {
                    button(p, X + W - 52, y, 28, 22, "▲", false, [this] { devFirst_ -= 3; });
                    button(p, X + W - 52, y + 125, 28, 22, "▼", false, [this] { devFirst_ += 3; });
                }
                if (n == 0)
                    p.say("NONE FOUND", X + 140, y + 15, 8, p.dim, 1, false, true, 1.0f, 0);
            }
            y += 160;
            p.say("BUFFER", X + 24, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            bx = X + 140;
            button(p, bx, y, 70, 22, "AUTO", s_.frames == 0, [this] { s_.frames = 0; apply(); });
            bx += 78;
            for (int f : io::audioBuffers())
            {
                button(p, bx, y, 64, 22, std::to_string(f), s_.frames == f, [this, f] { s_.frames = f; apply(); });
                bx += 72;
            }
            if (s_.frames > 0)
            {
                char ms[48];
                std::snprintf(ms, sizeof ms, "%.1f ms A BLOCK", static_cast<double>(s_.frames) * 1000.0 / 48000.0);
                p.say(ms, X + W - 24, y + 15, 8, p.accent, 0.9f, false, true, 1.0f, 2);
            }

            // ── MIDI
            y += 56;
            section(p, X + 24, y, "MIDI IN");
            p.say(midiWhere(), X + W - 24, y, 7.5f, p.dim, 1, false, true, 1.0f, 2);
            y += 24;
            const int rows = 6, n = static_cast<int>(sources_.size());
            srcFirst_ = std::clamp(srcFirst_, 0, std::max(0, n - rows));
            for (int k = 0; k < rows && srcFirst_ + k < n; ++k)
            {
                const MidiInterface::Source src = sources_[static_cast<size_t>(srcFirst_ + k)];
                const float ry = y + static_cast<float>(k) * 25;
                button(p, X + 140, ry, W - 200, 22, "", src.connected, [this, src] { toggle(src); });
                p.rect(X + 150, ry + 6, 10, 10, src.connected ? p.accent : p.display, 1, true);
                p.rect(X + 150, ry + 6, 10, 10, p.dim, 0.8f, false);
                p.say(shortName(src.name, 64), X + 172, ry + 15, 8, src.connected ? p.accent : p.ink, 1, false, false, 1.0f, 0);
            }
            if (n > rows)
            {
                button(p, X + W - 52, y, 28, 22, "▲", false, [this] { srcFirst_ -= 3; });
                button(p, X + W - 52, y + 125, 28, 22, "▼", false, [this] { srcFirst_ += 3; });
            }
            if (n == 0)
                p.say("NOTHING TO PLAY IT: A CONTROLLER OR A SEQUENCER APPEARS HERE", X + 140, y + 15, 8, p.dim, 1, false, true, 1.0f, 0);
            p.say("INPUTS", X + 24, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            y += 160;
            p.say("CHANNEL", X + 24, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            stepper(p, X + 140, y, 180, s_.channel < 0 ? std::string("ALL") : std::to_string(s_.channel + 1), [this](int d)
                    { s_.channel = ((s_.channel + 1 + d) % 17 + 17) % 17 - 1; midi_.setChannel(s_.channel); save(); });
            p.say("CC MAP", X + 360, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            const std::string map = s_.ccMap.empty() ? "AUTO  ·  " + ov_.name() : s_.ccMap;
            stepper(p, X + 450, y, W - 474, map, [this](int d) { stepMap(d); });

            look(p, X, y + 58);

            p.say("ESC OR × CLOSES  ·  EVERY CHANGE IS SAVED: " + io::settingsPath(product_), X + W / 2, Y + H - 16, 7, p.dim, 1, false, true, 1.0f, 1);
        }

        // a click in panel units; true when the page took it (it takes every click while open)
        bool click(float x, float y)
        {
            if (!open)
                return false;
            for (auto it = hot_.rbegin(); it != hot_.rend(); ++it)
                if (x >= it->x && x <= it->x + it->w && y >= it->y && y <= it->y + it->h)
                {
                    auto f = it->f; // the list is rebuilt by draw(); the action may change what it shows
                    f();
                    break;
                }
            return true;
        }

    private:
        struct Hot
        {
            float x, y, w, h;
            std::function<void()> f;
        };
        std::string product_, audioName_;
        io::Settings &s_;
        io::MidiIn &midi_;
        ccov::Overlay &ov_;
        std::function<bool()> reopen_;
        std::function<const io::AudioOut &()> out_;
        std::vector<Hot> hot_;
        std::vector<AudioDevice> devices_;
        std::vector<MidiInterface::Source> sources_;
        int devFirst_ = 0, srcFirst_ = 0;
        uint64_t lastLook_ = 0;

        std::string backendShown() const { return s_.backend; }

        void save() { io::saveSettings(product_, s_); }

        void apply()
        {
            save();
            reopen_();
        }

        void setBackend(const std::string &b)
        {
            if (b == s_.backend)
                return;
            s_.backend = b;
            s_.device.clear();
            devices_ = io::audioDevices(b);
            if (b == "ASIO" && !devices_.empty()) // ASIO has no default: its first driver (VASIO-32 when present)
                s_.device = devices_.front().id;
            devFirst_ = 0;
            apply();
        }

        void toggle(const MidiInterface::Source &src)
        {
            const bool on = !src.connected;
            midi_.setSource(src, on);
            s_.inputs[src.name] = on;
            save();
            sources_ = midi_.sources();
        }

        void stepMap(int d)
        {
            std::vector<std::string> names{""};
            for (const ccov::Map &m : ov_.maps)
                names.push_back(m.name);
            auto it = std::find(names.begin(), names.end(), s_.ccMap);
            const long i = it == names.end() ? 0 : it - names.begin(), n = static_cast<long>(names.size());
            s_.ccMap = names[static_cast<size_t>(((i + d) % n + n) % n)];
            applyCcMap(ov_, s_.ccMap);
            save();
        }

        std::string midiWhere() const
        {
#ifdef _WIN32
            return "THE DEVICES OPEN FOR " + product_;
#else
            return "JOINED TO " + std::string(audioName_) + ":INPUT";
#endif
        }

        static std::string shortName(const std::string &s, size_t n) { return s.size() > n ? s.substr(0, n - 1) + "…" : s; }

        // ── LOOK: the palette (the system's, or one of Carbonian's carried inside)
        void look(const Painter &p, float X, float y)
        {
            section(p, X + 24, y, "LOOK");
            y += 24;
            p.say("PALETTE", X + 24, y + 16, 8, p.dim, 1, true, false, 1.0f, 0);
            stepper(p, X + 140, y, 360, panelkit::paletteLabel(s_.palette), [this](int d) { stepPalette(d); });
            // the choices as swatches: one click each
            float sx = X + 520;
            for (const std::string &c : panelkit::paletteChoices())
            {
                const auto v = panelkit::paletteValues(c);
                const auto bg = v.find("background"), ac = v.find("accent");
                const bool on = c == s_.palette;
                p.rect(sx, y, 30, 22, bg != v.end() ? bg->second : p.display, 1, true);
                p.rect(sx + 8, y + 7, 14, 8, ac != v.end() ? ac->second : p.dim, 1, true);
                p.rect(sx, y, 30, 22, on ? p.accent : p.dim, on ? 1.0f : 0.4f, false);
                std::string choice = c;
                hot_.push_back({sx, y, 30, 22, [this, choice] { setPalette(choice); }});
                sx += 34;
            }
        }

        void setPalette(const std::string &c)
        {
            if (c == s_.palette)
                return;
            s_.palette = c;
            save();
            if (onLook)
                onLook();
        }
        void stepPalette(int d)
        {
            const auto names = panelkit::paletteChoices();
            auto it = std::find(names.begin(), names.end(), s_.palette);
            const long i = it == names.end() ? 0 : it - names.begin(), n = static_cast<long>(names.size());
            setPalette(names[static_cast<size_t>(((i + d) % n + n) % n)]);
        }

        void section(const Painter &p, float x, float y, const std::string &t)
        {
            p.say(t, x, y, 10, p.ink, 1, true, false, 1.0f, 0);
            p.rect(x, y + 6, 792, 1, p.dim, 0.4f, true);
        }

        void button(const Painter &p, float x, float y, float w, float h, const std::string &label, bool on, std::function<void()> f)
        {
            p.box(x, y, w, h, on);
            if (!label.empty())
                p.say(label, x + w / 2, y + h / 2 + 3.5f, 8, on ? p.accent : p.ink, 1, true, false, 1.0f, 1);
            hot_.push_back({x, y, w, h, std::move(f)});
        }

        void stepper(const Painter &p, float x, float y, float w, const std::string &value, std::function<void(int)> f)
        {
            p.rect(x, y, w, 22, p.display, 1, true);
            p.say("‹", x + 10, y + 16, 12, p.dim, 1, false, false, 1.0f, 1);
            p.say("›", x + w - 10, y + 16, 12, p.dim, 1, false, false, 1.0f, 1);
            p.say(shortName(value, static_cast<size_t>(std::max(8.0f, w / 7.5f))), x + w / 2, y + 15, 8, p.accent, 1, false, true, 1.0f, 1);
            hot_.push_back({x, y, w / 2, 22, [f] { f(-1); }});
            hot_.push_back({x + w / 2, y, w / 2, 22, [f] { f(1); }});
        }
    };
} // namespace synth::setup
