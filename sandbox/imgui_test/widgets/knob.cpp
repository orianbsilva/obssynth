#include "knob.hpp"
#include "imgui_internal.h"
#include <cmath>
#include <cstdio>

static constexpr float k_PI = 3.14159265358979323846f;
static constexpr float k_ArcStart = k_PI * 0.75f; // -135°  (bottom-left)
static constexpr float k_ArcRange = k_PI * 1.5f;  //  270° sweep

namespace Widgets
{

    bool SynthKnob(const char *label, float *v,
                   float v_min, float v_max,
                   const char *fmt,
                   float size_px)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        float radius = size_px * 0.5f;
        ImVec2 center = {pos.x + radius, pos.y + radius};

        // ─── Invisible button for input ───────────────────────────────────────
        ImGui::PushID(label);
        ImGui::InvisibleButton("##k", {size_px, size_px});
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        ImGui::PopID();

        bool changed = false;

        if (active)
        {
            float speed = io.KeyShift ? 0.002f : 0.012f;
            float delta = -io.MouseDelta.y * speed * (v_max - v_min);
            *v += delta;
            if (*v < v_min)
                *v = v_min;
            if (*v > v_max)
                *v = v_max;
            changed = (delta != 0.0f);
        }

        // Mouse wheel support
        if (hovered && io.MouseWheel != 0.0f)
        {
            float step = (v_max - v_min) * 0.01f;
            *v += io.MouseWheel * step;
            if (*v < v_min)
                *v = v_min;
            if (*v > v_max)
                *v = v_max;
            changed = true;
        }

        float t = (*v - v_min) / (v_max - v_min);
        float angle = k_ArcStart + t * k_ArcRange;

        // ─── Drawing ──────────────────────────────────────────────────────────
        // Shadow
        dl->AddCircleFilled(center, radius + 2.0f,
                            IM_COL32(0, 0, 0, 80), 48);

        // Body gradient (approximated with two circles)
        ImU32 body_col = active    ? IM_COL32(52, 52, 72, 255)
                         : hovered ? IM_COL32(44, 44, 62, 255)
                                   : IM_COL32(34, 34, 50, 255);
        dl->AddCircleFilled(center, radius, body_col, 48);

        // Inner highlight (top-left specular)
        dl->AddCircleFilled(
            {center.x - radius * 0.22f, center.y - radius * 0.22f},
            radius * 0.55f,
            IM_COL32(255, 255, 255, 14), 32);

        // Rim
        ImU32 rim_col = hovered || active
                            ? IM_COL32(80, 140, 255, 200)
                            : IM_COL32(60, 60, 90, 180);
        dl->AddCircle(center, radius, rim_col, 48, 1.5f);

        // Arc background (track)
        dl->PathArcTo(center, radius - 5.0f,
                      k_ArcStart, k_ArcStart + k_ArcRange, 48);
        dl->PathStroke(IM_COL32(30, 30, 45, 255), false, 3.0f);

        // Arc value fill (accent color)
        if (t > 0.0f)
        {
            dl->PathArcTo(center, radius - 5.0f,
                          k_ArcStart, k_ArcStart + t * k_ArcRange, 48);
            dl->PathStroke(IM_COL32(68, 136, 255, 230), false, 3.0f);
        }

        // Indicator dot
        float dot_x = center.x + (radius - 8.0f) * cosf(angle);
        float dot_y = center.y + (radius - 8.0f) * sinf(angle);
        dl->AddCircleFilled({dot_x, dot_y}, 3.0f,
                            IM_COL32(220, 230, 255, 255), 12);

        // ─── Label + value below ──────────────────────────────────────────────
        char val_str[32];
        snprintf(val_str, sizeof(val_str), fmt, *v);

        float text_w = ImGui::CalcTextSize(label).x;
        float val_w = ImGui::CalcTextSize(val_str).x;
        float total_w = size_px > text_w ? size_px : text_w;

        // Label
        ImGui::SetCursorScreenPos({pos.x + (total_w - text_w) * 0.5f,
                                   pos.y + size_px + 3.0f});
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 165, 185, 255));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        // Value
        ImGui::SetCursorScreenPos({pos.x + (total_w - val_w) * 0.5f,
                                   pos.y + size_px + 16.0f});
        ImGui::PushStyleColor(ImGuiCol_Text,
                              active ? IM_COL32(100, 180, 255, 255) : IM_COL32(200, 210, 230, 200));
        ImGui::TextUnformatted(val_str);
        ImGui::PopStyleColor();

        // Advance cursor past the whole widget (knob + label + value)
        ImGui::SetCursorScreenPos({pos.x + total_w + 4.0f, pos.y});
        ImGui::Dummy({0.0f, size_px + 32.0f});
        ImGui::SetCursorScreenPos({pos.x, pos.y + size_px + 32.0f});

        return changed;
    }

    bool WaveKnob(const char *label, float *v, float v_min, float v_max, float size_px)
    {
        return SynthKnob(label, v, v_min, v_max, "%.2f", size_px);
    }

} // namespace Widgets
