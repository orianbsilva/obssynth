#pragma once
#include "imgui.h"

namespace Widgets
{

    // ─── SynthFader ───────────────────────────────────────────────────────────────
    // Vertical fader with label above and value display below.
    // Returns true if value changed.
    // width_px  : width of the fader track (default 18)
    // height_px : height of the fader track (default 100)
    // was_clicked: optional output, set to true if the fader was left-clicked this frame
    bool SynthFader(const char *label, float *v,
                    float v_min, float v_max,
                    const char *fmt = "%.2f",
                    float width_px = 18.0f,
                    float height_px = 100.0f,
                    bool *was_clicked = nullptr);

} // namespace Widgets
