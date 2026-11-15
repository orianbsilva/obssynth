#pragma once
#include "imgui.h"
#include <cmath>

namespace Widgets
{

    // ─── SynthKnob ────────────────────────────────────────────────────────────────
    // Returns true if value changed.
    // label     : unique ID string (displayed below the knob)
    // v         : pointer to float parameter [v_min, v_max]
    // v_min/max : range
    // fmt       : printf format for value display (e.g. "%.1f")
    // size_px   : diameter in pixels (default 48)
    bool SynthKnob(const char *label, float *v,
                   float v_min, float v_max,
                   const char *fmt = "%.2f",
                   float size_px = 48.0f);

    // Knob with selectable waveform icons above (uses colored segments)
    bool WaveKnob(const char *label, float *v, float v_min, float v_max,
                  float size_px = 48.0f);

} // namespace Widgets
