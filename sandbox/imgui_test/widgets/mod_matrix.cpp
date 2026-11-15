#include "mod_matrix.hpp"
#include "imgui_internal.h"

namespace Widgets
{

    const char *ModMatrixState::SourceName(int r)
    {
        static const char *names[] = {
            "LFO 1", "LFO 2", "LFO 3", "LFO 4",
            "Velocity", "Aftertouch", "ModWheel",
            "Pitch Bend", "XY-X", "XY-Y"};
        return (r >= 0 && r < ROWS) ? names[r] : "?";
    }

    const char *ModMatrixState::DestName(int c)
    {
        static const char *names[] = {
            "Pitch", "Filter Freq", "Resonance", "Amp",
            "OSC1 Level", "OSC2 Level", "Delay Mix", "Tube Drive"};
        return (c >= 0 && c < COLS) ? names[c] : "?";
    }

    bool ModMatrix(const char *id, ModMatrixState *state, ImVec2 size)
    {
        bool changed = false;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (size.x <= 0.0f)
            size.x = avail.x;
        if (size.y <= 0.0f)
            size.y = avail.y;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {2.0f, 2.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {2.0f, 2.0f});

        // Header row (destinations)
        constexpr int ROWS = ModMatrixState::ROWS;
        constexpr int COLS = ModMatrixState::COLS;

        // Label column width
        float label_w = 80.0f;
        float cell_w = (size.x - label_w - 8.0f) / (float)COLS;
        float cell_h = (size.y - 20.0f) / (float)(ROWS + 1); // +1 for header
        if (cell_w < 18.0f)
            cell_w = 18.0f;
        if (cell_h < 16.0f)
            cell_h = 16.0f;

        ImGui::BeginChild(id, size, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_HorizontalScrollbar);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetWindowPos();

        // ─── Header row (destination names) ──────────────────────────────────
        for (int c = 0; c < COLS; ++c)
        {
            float hx = cp.x + label_w + c * cell_w + cell_w * 0.5f;
            const char *dname = ModMatrixState::DestName(c);
            ImVec2 ts = ImGui::CalcTextSize(dname);
            // Rotated text via DrawList rotation — not natively supported;
            // draw truncated horizontal for now
            char abbr[8] = {};
            for (int i = 0; i < 7 && dname[i]; ++i)
                abbr[i] = dname[i];
            dl->AddText({hx - ImGui::CalcTextSize(abbr).x * 0.5f, cp.y},
                        IM_COL32(140, 150, 170, 200), abbr);
        }

        // ─── Rows (source → destination cells) ───────────────────────────────
        for (int r = 0; r < ROWS; ++r)
        {
            float ry = cp.y + 18.0f + r * cell_h;

            // Source label
            const char *sname = ModMatrixState::SourceName(r);
            dl->AddText({cp.x + 2.0f, ry + (cell_h - ImGui::GetTextLineHeight()) * 0.5f},
                        IM_COL32(160, 165, 185, 220), sname);

            for (int c = 0; c < COLS; ++c)
            {
                float cx0 = cp.x + label_w + c * cell_w + 2.0f;
                float cy0 = ry + 2.0f;
                float cx1 = cx0 + cell_w - 4.0f;
                float cy1 = cy0 + cell_h - 4.0f;

                float val = state->amounts[r][c];
                bool on = (val != 0.0f);

                ImU32 bg_col = on
                                   ? IM_COL32(68, 136, 255, 200)
                                   : IM_COL32(22, 24, 36, 220);
                ImU32 rim_col = on
                                    ? IM_COL32(120, 175, 255, 255)
                                    : IM_COL32(45, 50, 70, 200);

                dl->AddRectFilled({cx0, cy0}, {cx1, cy1}, bg_col, 3.0f);
                dl->AddRect({cx0, cy0}, {cx1, cy1}, rim_col, 3.0f, 0, 1.0f);

                // Value text if active
                if (on)
                {
                    char vbuf[8];
                    snprintf(vbuf, sizeof(vbuf), "%.0f%%", val * 100.0f);
                    ImVec2 vts = ImGui::CalcTextSize(vbuf);
                    if (vts.x < cell_w - 4.0f)
                        dl->AddText({cx0 + (cell_w - 4.0f - vts.x) * 0.5f,
                                     cy0 + (cell_h - 4.0f - ImGui::GetTextLineHeight()) * 0.5f},
                                    IM_COL32(220, 235, 255, 255), vbuf);
                }

                // Hit test
                ImGui::SetCursorScreenPos({cx0, cy0});
                ImGui::PushID(r * COLS + c);
                if (ImGui::InvisibleButton("##cell", {cell_w - 4.0f, cell_h - 4.0f}))
                {
                    state->amounts[r][c] = on ? 0.0f : 0.5f;
                    changed = true;
                }
                // Mouse wheel to adjust amount
                if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f && on)
                {
                    state->amounts[r][c] += ImGui::GetIO().MouseWheel * 0.05f;
                    if (state->amounts[r][c] < 0.0f)
                        state->amounts[r][c] = 0.0f;
                    if (state->amounts[r][c] > 1.0f)
                        state->amounts[r][c] = 1.0f;
                    changed = true;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        return changed;
    }

} // namespace Widgets
