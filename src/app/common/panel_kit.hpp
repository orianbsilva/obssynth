#pragma once
// What a front panel drawn as hardware needs, shared by the products' panels (common/panel_app.hpp): the colours (Carbonian's palette, reread when it changes, else neutral greys and
// amber), SVG to texture (SDL3_image), words letter-spaced glyph by glyph (SDL3_ttf: Inter, JetBrains Mono), and a
// Canvas that draws in panel units at the window's scale: text, rectangles, lines, buttons, LEDs, knob caps that
// turn, faders. The static parts (panels, knob bodies, fader slots) go into one SVG rasterized once (svg* builders).
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "palette_choice.hpp"
#include "panel_fonts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace panelkit
{
    struct Theme
    {
        std::string bg = "#121213", panelTop = "#34343a", panelBottom = "#26262a", barTop = "#2c2c30", barBottom = "#1c1c1f";
        std::string display = "#0b0c0a", trace = "#7fa36f", traceHi = "#9fd38a", grid = "#1f2a1e", btnOn = "#3a3833";
        std::string btnOff = "#1d1d1f", row = "#2c2a26", ledOff = "#3a2c18", accent = "#e8a33d", ink = "#d9d6cf", dim = "#8d8a84";
        std::string name = "neutral";
    };

    inline const char *kPalette = kSystemPalette;

    // choice: SETUP's ("" the system's, else a palette by name: palette_choice.hpp)
    inline Theme loadTheme(const std::string &choice = "")
    {
        Theme t;
        std::map<std::string, std::string> p = paletteValues(choice);
        if (p.empty())
            return t;
        auto get = [&](const char *k, std::string &out) { if (p.count(k)) out = p[k]; };
        get("background", t.bg);
        get("surface_2", t.panelTop);
        get("surface", t.panelBottom);
        get("surface_2", t.barTop);
        get("surface", t.barBottom);
        get("idle_bg", t.display);
        get("ramp_2", t.trace);
        get("ramp_3", t.traceHi);
        get("line", t.grid);
        get("line", t.btnOn);
        get("surface", t.btnOff);
        get("surface_2", t.row);
        get("muted", t.ledOff);
        get("accent", t.accent);
        get("foreground", t.ink);
        get("muted", t.dim);
        get("label", t.name);
        return t;
    }

    inline SDL_FColor color(const std::string &hex, float a = 1.0f)
    {
        unsigned v = 0;
        std::sscanf(hex.c_str() + (hex[0] == '#'), "%06x", &v);
        return {static_cast<float>((v >> 16) & 255) / 255.0f, static_cast<float>((v >> 8) & 255) / 255.0f, static_cast<float>(v & 255) / 255.0f, a};
    }

    inline SDL_Texture *rasterize(SDL_Renderer *r, const std::string &svg, int w, int h)
    {
        SDL_IOStream *io = SDL_IOFromConstMem(svg.data(), svg.size());
        SDL_Surface *surf = io ? IMG_LoadSizedSVG_IO(io, w, h) : nullptr;
        if (io)
            SDL_CloseIO(io);
        if (!surf)
            return nullptr;
        SDL_Texture *t = SDL_CreateTextureFromSurface(r, surf);
        SDL_DestroySurface(surf);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
        return t;
    }

    inline std::string svgOpen(double w, double h)
    {
        char b[160];
        std::snprintf(b, sizeof b, "<svg xmlns='http://www.w3.org/2000/svg' width='%g' height='%g' viewBox='0 0 %g %g'>", w, h, w, h);
        return b;
    }

    struct Fonts
    {
        std::map<std::tuple<int, bool, bool>, TTF_Font *> open;
        TTF_Font *get(float px, bool bold, bool mono)
        {
            auto key = std::make_tuple(static_cast<int>(std::lround(px * 4)), bold, mono);
            auto it = open.find(key);
            if (it != open.end())
                return it->second;
            TTF_Font *f = nullptr;
            for (const std::string &path : synth::panelFonts(bold, mono))
                if ((f = TTF_OpenFont(path.c_str(), px)))
                    break;
            if (f)
            {
                TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
                TTF_SetFontKerning(f, true);
            }
            open[key] = f;
            return f;
        }
        void clear()
        {
            for (auto &[k, f] : open)
                if (f)
                    TTF_CloseFont(f);
            open.clear();
        }
    };

    inline SDL_Texture *renderWords(SDL_Renderer *r, TTF_Font *font, const std::string &txt, SDL_FColor c, float tracking)
    {
        if (!font || txt.empty())
            return nullptr;
        SDL_Color col{static_cast<Uint8>(c.r * 255), static_cast<Uint8>(c.g * 255), static_cast<Uint8>(c.b * 255), static_cast<Uint8>(c.a * 255)};
        if (tracking < 0.5f)
        {
            SDL_Surface *line = TTF_RenderText_Blended(font, txt.c_str(), txt.size(), col);
            if (!line)
                return nullptr;
            SDL_Texture *t = SDL_CreateTextureFromSurface(r, line);
            SDL_DestroySurface(line);
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
            return t;
        }
        std::vector<std::string> glyphs;
        for (size_t i = 0; i < txt.size();)
        {
            unsigned char ch = static_cast<unsigned char>(txt[i]);
            size_t n = ch < 0x80 ? 1 : ch < 0xE0 ? 2 : ch < 0xF0 ? 3 : 4;
            glyphs.push_back(txt.substr(i, n));
            i += n;
        }
        int total = 0;
        const int height = TTF_GetFontHeight(font);
        std::vector<int> widths;
        for (auto &g : glyphs)
        {
            int w = 0, h = 0;
            TTF_GetStringSize(font, g.c_str(), g.size(), &w, &h);
            widths.push_back(w);
            total += w;
        }
        total += static_cast<int>(std::ceil(tracking * static_cast<float>(glyphs.size() - 1))) + 2;
        SDL_Surface *line = SDL_CreateSurface(std::max(1, total), std::max(1, height), SDL_PIXELFORMAT_ARGB8888);
        SDL_SetSurfaceBlendMode(line, SDL_BLENDMODE_NONE);
        SDL_FillSurfaceRect(line, nullptr, 0);
        float x = 0;
        for (size_t i = 0; i < glyphs.size(); ++i)
        {
            if (glyphs[i] != " ")
            {
                SDL_Surface *g = TTF_RenderText_Blended(font, glyphs[i].c_str(), glyphs[i].size(), col);
                if (g)
                {
                    SDL_SetSurfaceBlendMode(g, SDL_BLENDMODE_NONE); // as the font drew it (blending darkens the fringe)
                    SDL_Rect d{static_cast<int>(std::lround(x)), 0, g->w, g->h};
                    SDL_BlitSurface(g, nullptr, line, &d);
                    SDL_DestroySurface(g);
                }
            }
            x += static_cast<float>(widths[i]) + tracking;
        }
        SDL_Texture *t = SDL_CreateTextureFromSurface(r, line);
        SDL_DestroySurface(line);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        return t;
    }

    // ── the static parts, as SVG ─────────────────────────────────────────────────────────────────────
    inline void svgDefs(std::ostringstream &s, const Theme &t)
    {
        s << "<defs><linearGradient id='pg' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='" << t.panelTop << "'/><stop offset='1' stop-color='"
          << t.panelBottom << "'/></linearGradient><linearGradient id='bar' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='" << t.barTop
          << "'/><stop offset='1' stop-color='" << t.barBottom << "'/></linearGradient>"
          << "<radialGradient id='kb' cx='.4' cy='.35' r='.8'><stop offset='0' stop-color='#3a3a3d'/><stop offset='.7' stop-color='#18181a'/><stop offset='1' stop-color='#0c0c0d'/></radialGradient>"
          << "<radialGradient id='sh' cx='.5' cy='.5' r='.5'><stop offset='.7' stop-color='#000' stop-opacity='.5'/><stop offset='1' stop-color='#000' stop-opacity='0'/></radialGradient></defs>";
    }
    inline void svgPanel(std::ostringstream &s, float x, float y, float w, float h)
    {
        s << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' rx='6' fill='url(#pg)' stroke='#0b0b0c' stroke-width='2'/>"
          << "<rect x='" << x + 1 << "' y='" << y + 1 << "' width='" << w - 2 << "' height='" << h - 2 << "' rx='5' fill='none' stroke='#fff' stroke-opacity='.06'/>";
    }
    inline void svgRule(std::ostringstream &s, float x, float y, float w)
    {
        s << "<line x1='" << x << "' y1='" << y << "' x2='" << x + w << "' y2='" << y << "' stroke='#000' stroke-opacity='.55'/>";
    }
    inline void svgDisplay(std::ostringstream &s, const Theme &t, float x, float y, float w, float h)
    {
        s << "<rect x='" << x << "' y='" << y << "' width='" << w << "' height='" << h << "' rx='4' fill='" << t.display << "' stroke='#000'/>";
    }
    inline void svgKnob(std::ostringstream &s, const Theme &t, float cx, float cy, float r)
    {
        const double x = static_cast<double>(cx), y = static_cast<double>(cy), rr = static_cast<double>(r);
        for (int i = 0; i <= 10; ++i)
        {
            const double a = (-135 + 27.0 * i - 90) * M_PI / 180.0, r1 = rr + 3, r2 = rr + ((i % 5 == 0) ? 7 : 5);
            s << "<line x1='" << x + r1 * std::cos(a) << "' y1='" << y + r1 * std::sin(a) << "' x2='" << x + r2 * std::cos(a) << "' y2='"
              << y + r2 * std::sin(a) << "' stroke='" << t.dim << "' stroke-width='1.1'/>";
        }
        s << "<circle cx='" << x << "' cy='" << y + 2 << "' r='" << rr + 4 << "' fill='url(#sh)'/>";
        s << "<circle cx='" << x << "' cy='" << y << "' r='" << rr << "' fill='url(#kb)' stroke='#050505'/>";
        for (int i = 0; i < 24; ++i)
        {
            const double a = i * 15 * M_PI / 180.0;
            s << "<line x1='" << x + (rr - 1) * std::cos(a) << "' y1='" << y + (rr - 1) * std::sin(a) << "' x2='" << x + rr * 0.8 * std::cos(a)
              << "' y2='" << y + rr * 0.8 * std::sin(a) << "' stroke='#2a2a2c' stroke-width='.7'/>";
        }
    }
    inline void svgFader(std::ostringstream &s, const Theme &t, float x, float y, float h)
    {
        s << "<rect x='" << x - 3 << "' y='" << y << "' width='6' height='" << h << "' rx='3' fill='#0a0a0b' stroke='#000'/>";
        for (int i = 0; i <= 8; ++i)
        {
            const float yy = y + h * static_cast<float>(i) / 8;
            s << "<line x1='" << x - 11 << "' y1='" << yy << "' x2='" << x - 7 << "' y2='" << yy << "' stroke='" << t.dim << "' stroke-width='.8'/>";
        }
    }

    enum Align { Left, Centre, Right };

    // ── what moves, drawn each frame in panel units ─────────────────────────────────────────────────
    struct Canvas
    {
        SDL_Renderer *r = nullptr;
        float scale = 1;
        // a part drawn in units of its own (a body laid out for another size): x' = x * zoom + offX; sizes * zoom
        float zoom = 1, offX = 0, offY = 0;
        float X(float x) const { return (x * zoom + offX) * scale; }
        float Y(float y) const { return (y * zoom + offY) * scale; }
        Theme theme;
        Fonts fonts;
        std::map<std::string, SDL_Texture *> words, values, pieces;
        std::map<int, SDL_Texture *> caps;

        void drop()
        {
            for (auto *m : {&words, &values, &pieces})
            {
                for (auto &[k, t] : *m)
                    SDL_DestroyTexture(t);
                m->clear();
            }
            for (auto &[k, t] : caps)
                SDL_DestroyTexture(t);
            caps.clear();
            fonts.clear();
        }

        void say(const std::string &txt, float x, float y, float size, const std::string &hex, float alpha, bool bold, bool mono, float tracking, Align align,
                 bool label = true)
        {
            if (txt.empty())
                return;
            size *= 1.2f * zoom;
            tracking *= zoom;
            char kb[96];
            std::snprintf(kb, sizeof kb, "|%g|%d%d|%s|%g|%g", static_cast<double>(size), bold, mono, hex.c_str(), static_cast<double>(alpha), static_cast<double>(tracking));
            const std::string key = txt + kb;
            auto &cache = label ? words : values;
            auto it = cache.find(key);
            if (it == cache.end())
            {
                if (!label && cache.size() > 600)
                {
                    for (auto &[k, t] : cache)
                        SDL_DestroyTexture(t);
                    cache.clear();
                }
                it = cache.emplace(key, renderWords(r, fonts.get(size * scale, bold, mono), txt, color(hex, alpha), tracking * scale)).first;
            }
            if (!it->second)
                return;
            float w, h;
            SDL_GetTextureSize(it->second, &w, &h);
            const SDL_FRect d{std::round(X(x) - (align == Centre ? w / 2 : align == Right ? w : 0)), std::round(Y(y) - h * 0.78f), w, h};
            SDL_RenderTexture(r, it->second, nullptr, &d);
        }

        void rect(float x, float y, float w, float h, const std::string &hex, float a = 1, bool fill = true)
        {
            const SDL_FColor c = color(hex, a);
            SDL_SetRenderDrawColorFloat(r, c.r, c.g, c.b, c.a);
            const SDL_FRect d{X(x), Y(y), w * zoom * scale, h * zoom * scale};
            if (fill)
                SDL_RenderFillRect(r, &d);
            else
                SDL_RenderRect(r, &d);
        }

        void lines(std::vector<SDL_FPoint> pts, const std::string &hex, float a, float thick)
        {
            const SDL_FColor c = color(hex, a);
            SDL_SetRenderDrawColorFloat(r, c.r, c.g, c.b, c.a);
            for (auto &p : pts)
                p = {X(p.x), Y(p.y)};
            const int n = std::max(1, static_cast<int>(std::lround(thick * zoom * scale / 1.5f)));
            for (int k = 0; k < n; ++k)
            {
                SDL_RenderLines(r, pts.data(), static_cast<int>(pts.size()));
                for (auto &p : pts)
                    p.y += 1.0f;
            }
        }

        void box(float x, float y, float w, float h, bool on)
        {
            char k[64];
            std::snprintf(k, sizeof k, "box%gx%g%d", static_cast<double>(w * zoom), static_cast<double>(h * zoom), on);
            auto it = pieces.find(k);
            if (it == pieces.end())
            {
                std::ostringstream s;
                s << svgOpen(static_cast<double>(w), static_cast<double>(h)) << "<rect x='.5' y='.5' width='" << w - 1 << "' height='" << h - 1 << "' rx='3' fill='"
                  << (on ? theme.btnOn : theme.btnOff) << "' stroke='" << (on ? theme.accent : "#000") << "'/></svg>";
                it = pieces.emplace(k, rasterize(r, s.str(), static_cast<int>(w * zoom * scale), static_cast<int>(h * zoom * scale))).first;
            }
            const SDL_FRect d{X(x), Y(y), w * zoom * scale, h * zoom * scale};
            SDL_RenderTexture(r, it->second, nullptr, &d);
        }

        // an LED (a toggle's), lit or not, with its glow
        void led(float cx, float cy, bool on)
        {
            const std::string k = on ? "led1" : "led0";
            auto it = pieces.find(k);
            if (it == pieces.end())
            {
                std::ostringstream s;
                s << svgOpen(24, 24) << "<defs><radialGradient id='g' cx='.5' cy='.5' r='.5'><stop offset='.3' stop-color='" << theme.accent
                  << "' stop-opacity='.35'/><stop offset='1' stop-color='" << theme.accent << "' stop-opacity='0'/></radialGradient></defs>";
                if (on)
                    s << "<circle cx='12' cy='12' r='12' fill='url(#g)'/>";
                s << "<circle cx='12' cy='12' r='4' fill='" << (on ? theme.accent : theme.ledOff) << "' stroke='#000'/></svg>";
                it = pieces.emplace(k, rasterize(r, s.str(), static_cast<int>(24 * zoom * scale), static_cast<int>(24 * zoom * scale))).first;
            }
            const SDL_FRect d{X(cx) - 12 * zoom * scale, Y(cy) - 12 * zoom * scale, 24 * zoom * scale, 24 * zoom * scale};
            SDL_RenderTexture(r, it->second, nullptr, &d);
        }

        // a knob's cap at its value (0..1): the body is in the static SVG (svgKnob)
        void cap(float cx, float cy, float rad, float v)
        {
            cx = (cx * zoom + offX), cy = (cy * zoom + offY), rad *= zoom;
            const int rk = static_cast<int>(std::lround(rad * 10));
            auto it = caps.find(rk);
            if (it == caps.end())
            {
                const double rr = static_cast<double>(rad), size = rr * 2 + 2, c = size / 2;
                std::ostringstream s;
                s << svgOpen(size, size)
                  << "<defs><radialGradient id='c' cx='.35' cy='.3' r='.9'><stop offset='0' stop-color='#f2f2ef'/><stop offset='.45' stop-color='#a9a9a6'/>"
                     "<stop offset='1' stop-color='#55554f'/></radialGradient></defs>"
                  << "<circle cx='" << c << "' cy='" << c << "' r='" << rr * 0.62 << "' fill='url(#c)' stroke='#111'/>"
                  << "<line x1='" << c << "' y1='" << c - rr * 0.15 << "' x2='" << c << "' y2='" << c - rr * 0.9 << "' stroke='#f4f2ec' stroke-width='"
                  << std::max(1.4, rr / 10) << "' stroke-linecap='round'/></svg>";
                it = caps.emplace(rk, rasterize(r, s.str(), static_cast<int>((rad * 2 + 2) * scale), static_cast<int>((rad * 2 + 2) * scale))).first;
            }
            const float size = (rad * 2 + 2) * scale;
            const SDL_FRect d{cx * scale - size / 2, cy * scale - size / 2, size, size};
            SDL_RenderTextureRotated(r, it->second, nullptr, &d, -135.0 + 270.0 * static_cast<double>(v), nullptr, SDL_FLIP_NONE);
        }

        // a fader's cap at its value (0..1), and a meter beside it (0..1, or below 0 for none)
        void faderCap(float x, float y, float h, float v, float meter)
        {
            if (zoom != 1 || offX != 0 || offY != 0) // in panel units, then as they are
            {
                const float z = zoom, ox = offX, oy = offY;
                zoom = 1, offX = offY = 0;
                // the cap's piece is 20 x 12 units: a zoomed fader keeps its cap's size (as its svgFader keeps the slot)
                faderCap(x * z + ox, y * z + oy, h * z, v, meter);
                zoom = z, offX = ox, offY = oy;
                return;
            }
            if (meter >= 0.0f)
            {
                rect(x + 9, y, 4, h, theme.display);
                const float m = std::min(1.0f, meter);
                rect(x + 9, y + h * (1 - m), 4, h * m, m > 0.95f ? theme.accent : theme.traceHi);
            }
            auto it = pieces.find("fadercap");
            if (it == pieces.end())
            {
                std::ostringstream sv;
                sv << svgOpen(20, 12) << "<defs><linearGradient id='c' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='#e9e9e6'/><stop offset='1' stop-color='#6b6b68'/></linearGradient></defs>"
                   << "<rect x='.5' y='.5' width='19' height='11' rx='2' fill='url(#c)' stroke='#111'/><line x1='2' y1='6' x2='18' y2='6' stroke='#f8f8f4' stroke-width='1.4'/></svg>";
                it = pieces.emplace("fadercap", rasterize(r, sv.str(), static_cast<int>(20 * scale), static_cast<int>(12 * scale))).first;
            }
            const float ky = y + h * (1 - v);
            const SDL_FRect d{(x - 10) * scale, (ky - 6) * scale, 20 * scale, 12 * scale};
            SDL_RenderTexture(r, it->second, nullptr, &d);
        }
    };
} // namespace panelkit
