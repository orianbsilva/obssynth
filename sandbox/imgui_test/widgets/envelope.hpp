#pragma once
#include "imgui.h"

namespace Widgets
{

    // ─── ADSREnvelope ─────────────────────────────────────────────────────────────
    // Interactive ADSR visualizer drawn inside a child region.
    // All time values are in seconds [0, max_time].
    // sustain is a level [0, 1].
    // Returns true if any parameter changed.
    struct ADSRParams
    {
        float attack = 0.01f; // seconds
        float decay = 0.25f;  // seconds
        float sustain = 0.7f; // level  [0, 1]
        float release = 0.4f; // seconds
    };

    bool ADSREnvelope(const char *id, ADSRParams *p,
                      float max_time = 2.0f,
                      ImVec2 size = {0.0f, 0.0f});

} // namespace Widgets
