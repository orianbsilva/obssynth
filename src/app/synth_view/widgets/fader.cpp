#include "fader.hpp"
#include "imgui_internal.h"
#include <cstdio>

namespace Widgets
{

    bool SynthFader(const char *label, float *v,
                    float v_min, float v_max,
                    const char *fmt,
                    float width_px,
                    float height_px,
                    bool *was_clicked)
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        float thumb_h = 18.0f;
        float track_x = pos.x + (width_px - 6.0f) * 0.5f;
        float track_w = 6.0f;
        float track_y0 = pos.y;
        float track_y1 = pos.y + height_px;

        // ─── Label above ──────────────────────────────────────────────────────
        float lw = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorScreenPos({pos.x + (width_px - lw) * 0.5f, pos.y - 14.0f});
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 165, 185, 255));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        // ─── Invisible button ─────────────────────────────────────────────────
        ImGui::PushID(label);
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("##f", {width_px, height_px});
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked(0);
        ImGui::PopID();

        if (was_clicked)
            *was_clicked = clicked;

        bool changed = false;
        ImGuiIO &io = ImGui::GetIO();

        if (active)
        {
            float speed = io.KeyShift ? 0.002f : 0.010f;
            float delta = -io.MouseDelta.y * speed * (v_max - v_min);
            *v += delta;
            if (*v < v_min)
                *v = v_min;
            if (*v > v_max)
                *v = v_max;
            changed = (delta != 0.0f);
        }

        if (hovered && io.MouseWheel != 0.0f)
        {
            *v += io.MouseWheel * (v_max - v_min) * 0.01f;
            if (*v < v_min)
                *v = v_min;
            if (*v > v_max)
                *v = v_max;
            changed = true;
        }

        float t = (*v - v_min) / (v_max - v_min);
        float thumb_y = track_y1 - thumb_h * 0.5f - t * (height_px - thumb_h);

        // ─── Draw track ───────────────────────────────────────────────────────
        // Track background
        dl->AddRectFilled(
            {track_x, track_y0},
            {track_x + track_w, track_y1},
            IM_COL32(20, 20, 32, 255), 3.0f);

        // Fill (value level, bottom-up)
        if (t > 0.0f)
        {
            dl->AddRectFilled(
                {track_x, thumb_y + thumb_h * 0.5f},
                {track_x + track_w, track_y1},
                IM_COL32(68, 136, 255, 180), 3.0f);
        }

        // Track rim
        dl->AddRect(
            {track_x, track_y0},
            {track_x + track_w, track_y1},
            IM_COL32(50, 55, 75, 200), 3.0f, 0, 1.0f);

        // ─── Draw thumb ───────────────────────────────────────────────────────
        ImU32 thumb_col = active    ? IM_COL32(90, 155, 255, 255)
                          : hovered ? IM_COL32(70, 130, 240, 255)
                                    : IM_COL32(55, 58, 80, 255);
        dl->AddRectFilled(
            {pos.x + 1.0f, thumb_y},
            {pos.x + width_px - 1.0f, thumb_y + thumb_h},
            thumb_col, 4.0f);
        dl->AddRect(
            {pos.x + 1.0f, thumb_y},
            {pos.x + width_px - 1.0f, thumb_y + thumb_h},
            IM_COL32(120, 140, 200, 180), 4.0f, 0, 1.0f);
        // Center line on thumb
        float mid_y = thumb_y + thumb_h * 0.5f;
        dl->AddLine({pos.x + 4.0f, mid_y},
                    {pos.x + width_px - 4.0f, mid_y},
                    IM_COL32(200, 210, 255, 140), 1.5f);

        // ─── Value text below ─────────────────────────────────────────────────
        char val_str[32];
        snprintf(val_str, sizeof(val_str), fmt, *v);
        float vw = ImGui::CalcTextSize(val_str).x;
        ImGui::SetCursorScreenPos({pos.x + (width_px - vw) * 0.5f,
                                   pos.y + height_px + 3.0f});
        ImGui::PushStyleColor(ImGuiCol_Text,
                              active ? IM_COL32(100, 180, 255, 255) : IM_COL32(180, 190, 210, 200));
        ImGui::TextUnformatted(val_str);
        ImGui::PopStyleColor();

        // Advance cursor
        ImGui::SetCursorScreenPos({pos.x, pos.y + height_px + 18.0f});

        return changed;
    }

} // namespace Widgets
