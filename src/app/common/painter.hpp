#pragma once
// How a shared piece of a panel (the top bar, SETUP) draws: with the panel's own hand (its words, rectangles and
// buttons), in panel units (1600 × 900), in the panel's colours.
#include <functional>
#include <string>

namespace synth::ui
{
    struct Painter
    {
        std::function<void(float x, float y, float w, float h, const std::string &hex, float a, bool fill)> rect;
        // align: 0 left, 1 centre, 2 right; tracking: the letters' spacing
        std::function<void(const std::string &txt, float x, float y, float size, const std::string &hex, float a, bool bold, bool mono, float tracking, int align)> say;
        std::function<void(float x, float y, float w, float h, bool on)> box;
        std::string bg, ink, dim, accent, display;
    };
} // namespace synth::ui
