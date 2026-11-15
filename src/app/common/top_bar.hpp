#pragma once
// The products' top bar: the name with its number (OBSSYNTH4D, OBSDRUMS16), the display (a status line above;
// ‹ the preset or kit › below, its name editable when saving), SAVE, CC (the overlay) and SETUP. Each panel draws
// its own bar's background (rasterized with the rest) and what is its alone (the synth's pages, the master knob);
// this draws the live parts and tells what a click hit. Panel units (1600 × 900).
#include "painter.hpp"

#include <SDL3/SDL.h>

#include <cctype>
#include <string>

namespace synth::ui
{
    class TopBar
    {
    public:
        enum Hit
        {
            None,
            Prev,  // ‹
            Next,  // ›
            Title, // the name (the synth: the list)
            Save,
            Cc,    // right click: the next map
            Setup,
        };

        struct Model
        {
            std::string name, number; // "OBSSYNTH", "4D"
            float numberX = 204;      // where the number starts (the name's width, in panel units)
            std::string status;       // the display's line above
            std::string title;        // the preset or kit
            bool changed = false;     // a • after the title
            bool naming = false;      // the title being written (SAVE)
            std::string nameEdit;
            bool cc = false, setup = false;
        };

        // the display and the buttons, where both panels have them
        static constexpr float DX = 690, DY = 11, DW = 420, DH = 36;

        static void draw(const Painter &p, const Model &m)
        {
            p.say(m.name, 24, 37, 22, p.ink, 1, false, false, 5, 0);
            p.say(m.number, m.numberX, 37, 22, p.accent, 1, true, false, 2, 0);
            std::string status = m.status;
            for (auto &c : status)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            p.say(status, DX + DW / 2, 24, 8, p.accent, 0.6f, false, true, 2, 1);
            if (m.naming)
            {
                std::string t = m.nameEdit;
                for (auto &c : t)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                p.say(t + ((SDL_GetTicks() / 500) % 2 ? " " : "_"), DX + DW / 2, 44, 14, p.ink, 1, false, true, 1, 1);
            }
            else
            {
                std::string t = m.title;
                for (auto &c : t)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                p.say(t + (m.changed ? "  •" : ""), DX + DW / 2, 44, 14, p.accent, 1, false, true, 1, 1);
            }
            p.say("‹", DX + 17, 40, 18, p.dim, 1, false, false, 0, 1);
            p.say("›", DX + DW - 17, 40, 18, p.dim, 1, false, false, 0, 1);
            button(p, 1124, "SAVE", m.naming);
            button(p, 1198, "CC", m.cc);
            button(p, 1272, "SETUP", m.setup);
        }

        static Hit hit(float x, float y)
        {
            if (y >= DY && y <= DY + DH)
            {
                if (x >= DX + 2 && x < DX + 32)
                    return Prev;
                if (x > DX + DW - 32 && x <= DX + DW - 2)
                    return Next;
                if (x >= DX + 32 && x <= DX + DW - 32)
                    return Title;
            }
            if (y >= 16 && y <= 42)
            {
                if (x >= 1124 && x <= 1188)
                    return Save;
                if (x >= 1198 && x <= 1262)
                    return Cc;
                if (x >= 1272 && x <= 1346)
                    return Setup;
            }
            return None;
        }

    private:
        static void button(const Painter &p, float x, const char *label, bool on)
        {
            const float w = std::string(label) == "SETUP" ? 74 : 64;
            p.box(x, 16, w, 26, on);
            p.say(label, x + w / 2, 34, 10, on ? p.accent : p.ink, 1, true, false, 1.5f, 1);
        }
    };
} // namespace synth::ui
