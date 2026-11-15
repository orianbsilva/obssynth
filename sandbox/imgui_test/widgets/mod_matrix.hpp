#pragma once
#include "imgui.h"

namespace Widgets
{

    // ─── ModMatrix ────────────────────────────────────────────────────────────────
    // Simplified modulation matrix grid.
    // rows x cols cells, toggle on/off.
    // Returns true if any cell changed.

    struct ModMatrixState
    {
        static constexpr int ROWS = 10; // Sources
        static constexpr int COLS = 8;  // Destinations
        float amounts[ROWS][COLS] = {}; // 0.0 = off, non-zero = amount

        static const char *SourceName(int r);
        static const char *DestName(int c);
    };

    bool ModMatrix(const char *id, ModMatrixState *state,
                   ImVec2 size = {0.0f, 0.0f});

} // namespace Widgets
