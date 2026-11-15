#include "envelope.hpp"
#include "imgui_internal.h"
#include <cmath>

namespace Widgets
{

    static bool DraggablePoint(const char *id, ImVec2 screen_pos,
                               float radius, ImVec2 &drag_delta,
                               bool &hovered_out)
    {
        ImGui::SetCursorScreenPos({screen_pos.x - radius,
                                   screen_pos.y - radius});
        ImGui::PushID(id);
        ImGui::InvisibleButton(id, {radius * 2.0f, radius * 2.0f});
        bool hov = ImGui::IsItemHovered();
        bool act = ImGui::IsItemActive();
        ImGui::PopID();

        hovered_out = hov || act;
        drag_delta = {0.0f, 0.0f};

        if (act)
        {
            ImGuiIO &io = ImGui::GetIO();
            drag_delta = io.MouseDelta;
        }
        return act && (ImGui::GetIO().MouseDelta.x != 0.0f || ImGui::GetIO().MouseDelta.y != 0.0f);
    }

    bool ADSREnvelope(const char *id, ADSRParams *p, float max_time, ImVec2 size)
    {
        ImGuiIO &io = ImGui::GetIO();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (size.x <= 0.0f)
            size.x = avail.x;
        if (size.y <= 0.0f)
            size.y = 120.0f;

        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        float W = size.x;
        float H = size.y;
        float pad = 10.0f;
        float cw = W - 2.0f * pad;
        float ch = H - 2.0f * pad;

        // ─── Child window for clipping ────────────────────────────────────────
        ImGui::BeginChild(id, size, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetWindowPos();

        // ─── Background ───────────────────────────────────────────────────────
        dl->AddRectFilled({cp.x, cp.y}, {cp.x + W, cp.y + H},
                          IM_COL32(13, 13, 20, 255), 6.0f);
        dl->AddRect({cp.x, cp.y}, {cp.x + W, cp.y + H},
                    IM_COL32(40, 45, 65, 200), 6.0f, 0, 1.0f);

        // Grid lines (horizontal, 4 levels)
        for (int i = 1; i < 4; ++i)
        {
            float gy = cp.y + pad + ch * (1.0f - i / 4.0f);
            dl->AddLine({cp.x + pad, gy}, {cp.x + pad + cw, gy},
                        IM_COL32(35, 38, 55, 180), 1.0f);
        }

        // ─── Build envelope points ────────────────────────────────────────────
        // X = time mapped to cw, Y = level mapped to ch (flipped: 1=top)
        auto timeX = [&](float t)
        {
            return cp.x + pad + (t / max_time) * cw;
        };
        auto levelY = [&](float l)
        {
            return cp.y + pad + (1.0f - l) * ch;
        };

        float t_attack = p->attack;
        float t_decay = t_attack + p->decay;
        // sustain held until 80% of max_time
        float t_sus_end = max_time * 0.80f;
        float t_end = t_sus_end + p->release;

        ImVec2 p0 = {timeX(0.0f), levelY(0.0f)};            // Start
        ImVec2 p1 = {timeX(t_attack), levelY(1.0f)};        // Attack peak
        ImVec2 p2 = {timeX(t_decay), levelY(p->sustain)};   // Decay end
        ImVec2 p3 = {timeX(t_sus_end), levelY(p->sustain)}; // Sustain end
        ImVec2 p4 = {timeX(t_end < max_time ? t_end : max_time),
                     levelY(0.0f)}; // Release end

        // Filled area under envelope
        ImVec2 fill_pts[] = {p0, p1, p2, p3, p4, {p4.x, cp.y + pad + ch}, {p0.x, cp.y + pad + ch}};
        dl->AddConvexPolyFilled(fill_pts, 7, IM_COL32(68, 136, 255, 30));

        // Envelope line (piecewise with subtle bezier feel via line segments)
        dl->AddLine(p0, p1, IM_COL32(68, 136, 255, 220), 2.0f);
        dl->AddLine(p1, p2, IM_COL32(68, 136, 255, 220), 2.0f);
        dl->AddLine(p2, p3, IM_COL32(68, 136, 255, 180), 2.0f);
        dl->AddLine(p3, p4, IM_COL32(68, 136, 255, 220), 2.0f);

        // Sustain level dashed line
        float dash = 4.0f;
        for (float x = p0.x; x < p3.x - dash; x += dash * 2.0f)
        {
            dl->AddLine({x, p2.y}, {x + dash, p2.y},
                        IM_COL32(68, 136, 255, 60), 1.0f);
        }

        // ─── Draggable control points ─────────────────────────────────────────
        bool changed = false;
        float pt_r = 6.0f;

        // A1: Attack time (p1.x)
        {
            ImVec2 delta;
            bool hov;
            ImGui::SetCursorScreenPos({p1.x - pt_r, p1.y - pt_r});
            DraggablePoint("##A", p1, pt_r, delta, hov);
            if (delta.x != 0.0f)
            {
                p->attack += delta.x / cw * max_time;
                if (p->attack < 0.001f)
                    p->attack = 0.001f;
                if (p->attack > max_time * 0.4f)
                    p->attack = max_time * 0.4f;
                changed = true;
            }
            ImU32 c = hov ? IM_COL32(120, 190, 255, 255) : IM_COL32(68, 136, 255, 255);
            dl->AddCircleFilled(p1, pt_r, c, 12);
            dl->AddCircle(p1, pt_r, IM_COL32(200, 220, 255, 200), 12, 1.5f);
        }
        // A2: Decay time + Sustain level (p2)
        {
            ImVec2 delta;
            bool hov;
            ImGui::SetCursorScreenPos({p2.x - pt_r, p2.y - pt_r});
            DraggablePoint("##D", p2, pt_r, delta, hov);
            if (delta.x != 0.0f || delta.y != 0.0f)
            {
                p->decay += delta.x / cw * max_time;
                if (p->decay < 0.001f)
                    p->decay = 0.001f;
                if (p->decay > max_time * 0.4f)
                    p->decay = max_time * 0.4f;
                p->sustain -= delta.y / ch;
                if (p->sustain < 0.0f)
                    p->sustain = 0.0f;
                if (p->sustain > 1.0f)
                    p->sustain = 1.0f;
                changed = true;
            }
            ImU32 c = hov ? IM_COL32(120, 190, 255, 255) : IM_COL32(68, 136, 255, 255);
            dl->AddCircleFilled(p2, pt_r, c, 12);
            dl->AddCircle(p2, pt_r, IM_COL32(200, 220, 255, 200), 12, 1.5f);
        }
        // A3: Release time (p4.x)
        {
            ImVec2 delta;
            bool hov;
            ImGui::SetCursorScreenPos({p4.x - pt_r, p4.y - pt_r});
            DraggablePoint("##R", p4, pt_r, delta, hov);
            if (delta.x != 0.0f)
            {
                p->release += delta.x / cw * max_time;
                if (p->release < 0.001f)
                    p->release = 0.001f;
                if (p->release > max_time * 0.5f)
                    p->release = max_time * 0.5f;
                changed = true;
            }
            ImU32 c = hov ? IM_COL32(120, 190, 255, 255) : IM_COL32(68, 136, 255, 255);
            dl->AddCircleFilled(p4, pt_r, c, 12);
            dl->AddCircle(p4, pt_r, IM_COL32(200, 220, 255, 200), 12, 1.5f);
        }

        // ─── Parameter labels ─────────────────────────────────────────────────
        auto DrawParamLabel = [&](ImVec2 at, const char *name, float val)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s %.2f", name, val);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText({at.x - ts.x * 0.5f, cp.y + H - 14.0f},
                        IM_COL32(140, 150, 170, 200), buf);
        };
        DrawParamLabel({(p0.x + p1.x) * 0.5f, 0}, "A", p->attack);
        DrawParamLabel({(p1.x + p2.x) * 0.5f, 0}, "D", p->decay);
        DrawParamLabel({p2.x + 10.0f, 0}, "S", p->sustain);
        DrawParamLabel({(p3.x + p4.x) * 0.5f, 0}, "R", p->release);

        ImGui::EndChild();
        return changed;
    }

} // namespace Widgets
