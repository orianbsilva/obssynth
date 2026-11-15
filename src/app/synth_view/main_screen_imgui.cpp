// main_screen_imgui.cpp
// ImGui-based UI implementation for SynthViewer.
// Implements the drawImgui* methods declared in main_screen.hpp.
// All synth parameter changes go through setKnobValue() / applyOscKnobValue()
// / applyGlbKnobValue() to keep routing logic in one place.

#include "main_screen.hpp"
#include "../common/audio_constants.hpp"
#include "../lib/synth/core/effect_factory.hpp"
#include "../lib/oscillators/lfo.hpp"
#include "../lib/oscillators/pcm_waveform.hpp"
#include "../../products/obs_synth/obs_synth_mod_source.hpp"
#include "imgui_theme.hpp"
#include "widgets/knob.hpp"
#include "widgets/fader.hpp"
#include "widgets/envelope.hpp"
#include "mod_matrix_grid.hpp"
#include "imgui.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <commdlg.h>
#include <windows.h>

namespace synth
{

    // ─── Internal helpers (file-scope) ────────────────────────────────────────────
    static ImVec4 Accent() { return {0.267f, 0.533f, 1.000f, 1.0f}; }
    static ImVec4 AccentDim() { return {0.165f, 0.282f, 0.604f, 1.0f}; }

    // MIDI Learn click tracking: set per-frame when a knob/fader is clicked
    static bool g_midiLearnKnobClicked = false;
    static int g_midiLearnClickOsc = -1;   // -1 = global
    static int g_midiLearnClickParam = -1; // OscParam or GlobalParam

    /// Call after drawing a knob/fader to check if it was clicked in learn mode
    static void TrackLearnClick(bool wasClicked, int oscIdx, int paramIdx)
    {
        if (wasClicked && !g_midiLearnKnobClicked)
        {
            g_midiLearnKnobClicked = true;
            g_midiLearnClickOsc = oscIdx;
            g_midiLearnClickParam = paramIdx;
        }
    }

    // Per-PartType accent colours: 0=STD(blue), 1=WT(green), 2=FM(orange)
    static ImVec4 OscAccent(int pt)
    {
        switch (pt)
        {
        case 1:
            return {0.18f, 0.72f, 0.45f, 1.0f}; // WT  — teal-green
        case 2:
            return {0.88f, 0.50f, 0.14f, 1.0f}; // FM  — amber-orange
        default:
            return Accent(); // STD — blue
        }
    }
    static ImVec4 OscAccentDim(int pt)
    {
        switch (pt)
        {
        case 1:
            return {0.09f, 0.38f, 0.24f, 1.0f};
        case 2:
            return {0.46f, 0.26f, 0.07f, 1.0f};
        default:
            return AccentDim();
        }
    }
    static ImVec4 OscAccentBorder(int pt)
    {
        switch (pt)
        {
        case 1:
            return {0.10f, 0.52f, 0.30f, 0.80f};
        case 2:
            return {0.65f, 0.34f, 0.07f, 0.80f};
        default:
            return {0.20f, 0.40f, 0.75f, 0.80f};
        }
    }

    static void SectionHeader(const char *title)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Accent());
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, AccentDim());
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
    static void SectionHeaderColored(const char *title, ImVec4 color, ImVec4 colorDim)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, colorDim);
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    static const char *kWaveNames[] = {
        "Sine", "Saw", "Sq", "Tri", "Pls",
        "Noise", "RmpU", "RmpD", "PCM", "FM"};
    static constexpr int kWaveCount = 10;
    static const char *kOscNames[4] = {"OSC 1", "OSC 2", "OSC 3", "OSC 4"};

    // ─── Per-oscillator mini waveform scope ───────────────────────────────────
    // Computes one cycle of the waveform mathematically — no audio state touched.
    static void computeWaveSamples(int wt, float *out, int n, int cycles = 5)
    {
        for (int i = 0; i < n; ++i)
        {
            // t spans 0..cycles, then wrap to get phase within each cycle
            float tRaw = (float)i / (float)n * (float)cycles;
            float t = tRaw - floorf(tRaw); // phase 0..1 within current cycle
            switch (wt)
            {
            case 0:
                out[i] = sinf(t * 6.28318f);
                break; // SINE
            case 1:
                out[i] = 2.0f * t - 1.0f;
                break; // SAW
            case 2:
                out[i] = t < 0.5f ? 1.0f : -1.0f;
                break; // SQUARE
            case 3:
                out[i] = t < 0.5f ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
                break; // TRI
            case 4:
                out[i] = t < 0.25f ? 1.0f : -1.0f;
                break; // PULSE
            case 5:
            { // NOISE — deterministic pseudo-random for display
                unsigned s = (unsigned)(i * 2654435761u) ^ 0xDEADBEEFu;
                s = s ^ (s >> 13);
                s ^= (s << 17);
                s ^= (s >> 5);
                out[i] = ((float)(s & 0x7FFFFFu) / (float)0x3FFFFFu) - 1.0f;
            }
            break;
            case 6:
                out[i] = 2.0f * t - 1.0f;
                break; // RAMP_UP
            case 7:
                out[i] = 1.0f - 2.0f * t;
                break; // RAMP_DOWN
            default:
                out[i] = sinf(t * 6.28318f);
                break; // PCM/FM approx
            }
        }
    }

    static void OscWavePreview(int wt, float w, float h)
    {
        constexpr int N = 64;
        float samples[N];
        computeWaveSamples(wt, samples, N);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList *dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, {pos.x + w, pos.y + h}, IM_COL32(12, 14, 22, 255), 3.0f);
        dl->AddRect(pos, {pos.x + w, pos.y + h}, IM_COL32(45, 65, 120, 180), 3.0f);
        float midY = pos.y + h * 0.5f;
        dl->AddLine({pos.x + 2.0f, midY}, {pos.x + w - 2.0f, midY}, IM_COL32(35, 45, 75, 160));
        ImVec2 pts[N];
        for (int i = 0; i < N; ++i)
        {
            float fx = pos.x + 2.0f + (w - 4.0f) * (float)i / (float)(N - 1);
            float fy = midY - samples[i] * (h * 0.42f);
            pts[i] = {fx, fy};
        }
        dl->AddPolyline(pts, N, IM_COL32(68, 136, 255, 240), 0, 1.5f);
        if (wt == 8) // PCM_CUSTOM
            dl->AddText({pos.x + 4.0f, pos.y + 3.0f}, IM_COL32(90, 210, 90, 220), "PCM");
        else if (wt == 9) // FM_SYNTHESIS
            dl->AddText({pos.x + 4.0f, pos.y + 3.0f}, IM_COL32(210, 170, 60, 220), "FM~");
        ImGui::Dummy({w, h});
    }

    // ─── Wave selector buttons (2 rows of 5) ──────────────────────────────────
    static bool WaveSelector(float *wave_v)
    {
        int wi = (int)(*wave_v + 0.5f);
        bool changed = false;
        const ImVec4 unsel{0.118f, 0.133f, 0.200f, 1.0f};
        for (int row = 0; row < 2; ++row)
        {
            for (int col = 0; col < 5; ++col)
            {
                int i = row * 5 + col;
                if (col > 0)
                    ImGui::SameLine(0.0f, 2.0f);
                bool sel = (wi == i);
                ImGui::PushStyleColor(ImGuiCol_Button, sel ? Accent() : unsel);
                ImGui::PushID(i);
                if (ImGui::SmallButton(kWaveNames[i]))
                {
                    *wave_v = (float)i;
                    changed = true;
                }
                ImGui::PopID();
                ImGui::PopStyleColor();
            }
        }
        return changed;
    }

    // ─── Knob row ──────────────────────────────────────────────────────────────
    // oscIdx/paramIds: for MIDI learn tracking. Pass oscIdx=-1 for global knobs.
    // paramIds: array of param enum values corresponding to each knob (or nullptr to skip tracking)
    static bool KnobRow(const char *id_prefix,
                        float *params[], const char *labels[],
                        const float *mins, const float *maxs,
                        const char *fmts[], int n,
                        float knob_size = 42.0f,
                        int oscIdx = -1, const int *paramIds = nullptr)
    {
        bool changed = false;
        ImGui::PushID(id_prefix);
        for (int i = 0; i < n; ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, 8.0f);
            ImGui::PushID(i);
            bool clicked = false;
            changed |= Widgets::SynthKnob(labels[i], params[i],
                                          mins[i], maxs[i], fmts[i], knob_size, &clicked);
            if (clicked && paramIds)
                TrackLearnClick(true, oscIdx, paramIds[i]);
            ImGui::PopID();
        }
        ImGui::PopID();
        return changed;
    }

    // ─── Single knob centered in available width ───────────────────────────────
    static bool CenteredKnob(const char *label, float *v,
                             float v_min, float v_max, const char *fmt,
                             float knob_size = 42.0f,
                             int oscIdx = -1, int paramIdx = -1)
    {
        float avail = ImGui::GetContentRegionAvail().x;
        float indent = (avail - knob_size) * 0.5f;
        if (indent > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
        bool clicked = false;
        bool changed = Widgets::SynthKnob(label, v, v_min, v_max, fmt, knob_size, &clicked);
        if (clicked && paramIdx >= 0)
            TrackLearnClick(true, oscIdx, paramIdx);
        return changed;
    }

    // ─── Effect type selector combo ───────────────────────────────────────────
    // Returns true and sets *out_type when user picks a new type.
    // kFxTypes / kFxNames list only the actually-implemented effects.
    static constexpr EffectType kFxTypes[] = {
        EffectType::NONE, EffectType::CHORUS,
        EffectType::OVERDRIVE, EffectType::TUBE_REVERB};
    static constexpr const char *kFxNames[] = {
        "No Effect", "Chorus", "Overdrive", "Tube Reverb"};
    static constexpr int kFxCount = 4;

    static bool EffectSelector(const char *combo_id, EffectType current, EffectType *out_type)
    {
        int curIdx = 0;
        for (int i = 0; i < kFxCount; ++i)
            if (kFxTypes[i] == current)
            {
                curIdx = i;
                break;
            }

        bool changed = false;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo(combo_id, kFxNames[curIdx]))
        {
            for (int i = 0; i < kFxCount; ++i)
            {
                bool sel = (i == curIdx);
                if (ImGui::Selectable(kFxNames[i], sel))
                {
                    *out_type = kFxTypes[i];
                    changed = true;
                }
                if (sel)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // ─── Per-oscillator panel ─────────────────────────────────────────────────
    void SynthViewer::drawImguiOscPanel(int oscIdx)
    {
        float *ov = m_oscKnobValues[oscIdx]; // shorthand
        int curPartType = (int)(ov[OSC_PART_TYPE] + 0.5f);
        ImVec4 ptAccent = OscAccent(curPartType);
        ImVec4 ptAccentDim = OscAccentDim(curPartType);
        ImVec4 ptBorder = OscAccentBorder(curPartType);

        ImGui::PushID(oscIdx);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.063f, 0.067f, 0.098f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_Border, ptBorder);
        ImGui::BeginChild("##osc", {0.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor(2);

        // Accent bar at top of panel
        {
            ImVec2 p0 = ImGui::GetWindowPos();
            ImVec2 sz = ImGui::GetWindowSize();
            ImDrawList *dlp = ImGui::GetWindowDrawList();
            ImU32 barCol = ImGui::ColorConvertFloat4ToU32(ptAccent);
            dlp->AddRectFilled(p0, {p0.x + sz.x, p0.y + 3.0f}, barCol);
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);

        SectionHeaderColored(kOscNames[oscIdx], ptAccent, ptAccentDim);

        // ── PartType selector [STD | WT | FM] ────────────────────────────────
        {
            static const char *kPartLabels[] = {"STD", "WT", "FM"};
            for (int p = 0; p < 3; ++p)
            {
                bool sel = (p == curPartType);
                ImVec4 btnCol = sel ? OscAccent(p) : ImVec4{0.118f, 0.133f, 0.200f, 1.0f};
                ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, sel ? OscAccentDim(p) : ImVec4{0.18f, 0.20f, 0.30f, 1.0f});
                if (ImGui::SmallButton(kPartLabels[p]))
                {
                    ov[OSC_PART_TYPE] = (float)p;
                    applyOscKnobValue(oscIdx, OSC_PART_TYPE);
                }
                ImGui::PopStyleColor(2);
                if (p < 2)
                    ImGui::SameLine(0.0f, 2.0f);
            }
        }
        ImGui::Spacing();

        // ── Wave selector + waveform preview ─────────────────────────────────
        if (curPartType == 1) // WAVETABLE ── full-width isometric 3D viewer
        {
            const auto &chain = m_synth.getEngine().getAllocator().getVoice(0).getChain(oscIdx);
            const PCMWaveform *wf = chain.getWtWaveform();

            float viewW = ImGui::GetContentRegionAvail().x - 2.0f;
            if (viewW < 60.0f)
                viewW = 60.0f;
            const float VIEW_H = 118.0f;
            ImVec2 vp = ImGui::GetCursorScreenPos();
            ImGui::Dummy({viewW, VIEW_H}); // reserve layout space so knobs render below
            ImDrawList *vdl = ImGui::GetWindowDrawList();

            // Background + accent border
            vdl->AddRectFilled(vp, {vp.x + viewW, vp.y + VIEW_H}, IM_COL32(7, 11, 17, 255), 4.0f);
            vdl->AddRect(vp, {vp.x + viewW, vp.y + VIEW_H},
                         ImGui::ColorConvertFloat4ToU32(OscAccentBorder(1)), 4.0f, 0, 1.5f);

            // Clip all wave drawing to the viewer rectangle
            vdl->PushClipRect({vp.x + 1.0f, vp.y + 1.0f},
                              {vp.x + viewW - 1.0f, vp.y + VIEW_H - 1.0f}, true);

            if (wf && wf->isLoaded() && wf->isWavBank())
            {
                int nTables = (int)wf->getBankWaveCount();
                size_t spw = wf->getSamplesPerWave();
                const auto &raw = wf->getData();
                float morphPos = chain.getWtMorphPosition(); // 0..nTables-1

                // ── Isometric projection parameters ───────────────────────────
                const int N_MAX_VIS = 40;
                int nVis = (nTables <= N_MAX_VIS) ? nTables : N_MAX_VIS;
                float tStride = (nVis > 1) ? (float)(nTables - 1) / (float)(nVis - 1) : 0.0f;

                // Depth budget — keep waves well inside the frame
                const float PAD = 4.0f;
                const float dX = viewW * 0.15f;
                const float dY = VIEW_H * 0.38f;
                const float tStepX = (nVis > 1) ? dX / (float)(nVis - 1) : 0.0f;
                const float tStepY = (nVis > 1) ? dY / (float)(nVis - 1) : 0.0f;

                const float waveW = viewW - dX - PAD * 2.0f;
                const float baseX = vp.x + PAD;
                // front table sits at ~80% height; leaves 20% below for labels/baseline
                const float baseY = vp.y + VIEW_H * 0.82f;
                // amp must not let waves exceed the frame top (baseY - dY) or bottom (VIEW_H)
                const float maxAmpUp = baseY - (vp.y + PAD + dY + 12.0f); // room above back row
                const float maxAmpDown = (vp.y + VIEW_H - PAD) - baseY;
                const float amp = fminf(maxAmpUp, maxAmpDown) * 0.90f;

                constexpr int DRAW_PTS = 80;

                // ── Draw tables back-to-front (vi = nVis-1 → 0) ───────────────
                for (int vi = nVis - 1; vi >= 0; --vi)
                {
                    int ti = (nVis > 1) ? (int)(vi * tStride + 0.5f) : 0;
                    if (ti < 0)
                        ti = 0;
                    if (ti >= nTables)
                        ti = nTables - 1;

                    float rowX = baseX + (float)vi * tStepX;
                    float rowY = baseY - (float)vi * tStepY;

                    size_t baseIdx = (size_t)ti * spw;
                    if (baseIdx + spw > raw.size())
                        continue;

                    float dist = fabsf(morphPos - (float)ti);
                    bool isCur = (dist < 0.5f);
                    float alpha = isCur ? 0.90f
                                        : fmaxf(0.04f, 0.06f + 0.52f * (1.0f - dist / (float)(nTables > 1 ? nTables - 1 : 1)));

                    vdl->AddLine({rowX, rowY}, {rowX + waveW, rowY},
                                 IM_COL32(28, 75, 48, (int)(alpha * 120)), 0.6f);

                    ImVec2 pts[DRAW_PTS];
                    for (int i = 0; i < DRAW_PTS; ++i)
                    {
                        float ph = (float)i / (float)(DRAW_PTS - 1);
                        size_t si = baseIdx + (size_t)(ph * (float)(spw - 1));
                        float s = raw[si < raw.size() ? si : raw.size() - 1];
                        pts[i] = {rowX + ph * waveW, rowY - s * amp};
                    }
                    ImU32 lineCol = isCur
                                        ? IM_COL32(65, 235, 145, (int)(alpha * 255))
                                        : IM_COL32(30 + (int)(alpha * 90), 115 + (int)(alpha * 90), 60 + (int)(alpha * 40),
                                                   (int)(alpha * 210));
                    vdl->AddPolyline(pts, DRAW_PTS, lineCol, 0, isCur ? 1.8f : 0.7f);
                }

                // ── Interpolated current wave on top ──────────────────────────
                {
                    float t0f = floorf(morphPos);
                    int ti0 = (int)t0f;
                    int ti1 = (ti0 + 1 < nTables) ? ti0 + 1 : nTables - 1;
                    float frac = morphPos - t0f;
                    float morphFrac = (nTables > 1) ? morphPos / (float)(nTables - 1) : 0.0f;
                    float mvi = morphFrac * (float)(nVis - 1);
                    float rowX = baseX + mvi * tStepX;
                    float rowY = baseY - mvi * tStepY;
                    size_t base0 = (size_t)ti0 * spw;
                    size_t base1 = (size_t)ti1 * spw;

                    ImVec2 pts[DRAW_PTS];
                    for (int i = 0; i < DRAW_PTS; ++i)
                    {
                        float ph = (float)i / (float)(DRAW_PTS - 1);
                        size_t si = (size_t)(ph * (float)(spw - 1));
                        float s0 = raw[(base0 + si) < raw.size() ? base0 + si : raw.size() - 1];
                        float s1 = raw[(base1 + si) < raw.size() ? base1 + si : raw.size() - 1];
                        float s = s0 + frac * (s1 - s0);
                        pts[i] = {rowX + ph * waveW, rowY - s * amp};
                    }
                    vdl->AddPolyline(pts, DRAW_PTS, IM_COL32(165, 255, 205, 250), 0, 2.6f);
                }

                // ── Labels (inside clip rect so they stay visible) ─────────────
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "%d tables", nTables);
                vdl->AddText({vp.x + 4.0f, vp.y + 3.0f},
                             IM_COL32(55, 175, 100, 200), lbl);
                snprintf(lbl, sizeof(lbl), "t%.1f", static_cast<double>(morphPos));
                float tw2 = ImGui::CalcTextSize(lbl).x;
                vdl->AddText({vp.x + viewW - tw2 - 4.0f, vp.y + 3.0f},
                             IM_COL32(100, 220, 145, 200), lbl);
            }
            else
            {
                const char *msg = (wf && wf->isLoaded()) ? "No bank" : "Load a bank to see waveforms";
                float tw2 = ImGui::CalcTextSize(msg).x;
                vdl->AddText({vp.x + (viewW - tw2) * 0.5f, vp.y + VIEW_H * 0.5f - 7.0f},
                             IM_COL32(55, 115, 75, 180), msg);
            }

            vdl->PopClipRect();

            // ── Compact controls strip below viewer ───────────────────────────
            ImGui::PushStyleColor(ImGuiCol_Button, OscAccentDim(1));
            if (ImGui::SmallButton("Load Bank..."))
            {
                char filePath[MAX_PATH] = {};
                OPENFILENAMEA ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.lpstrFilter = "WAV Files\0*.wav\0All Files\0*.*\0";
                ofn.lpstrFile = filePath;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameA(&ofn))
                {
                    m_wtPaths[oscIdx] = filePath;
                    m_synth.loadWtBank(oscIdx, m_wtPaths[oscIdx]);
                }
            }
            ImGui::PopStyleColor();
            if (!m_wtPaths[oscIdx].empty())
            {
                const char *start = m_wtPaths[oscIdx].c_str();
                const char *fname = start;
                for (const char *p = start; *p; ++p)
                    if (*p == '\\' || *p == '/')
                        fname = p + 1;
                char dispBuf[24];
                int flen = (int)strlen(fname);
                if (flen <= 18)
                {
                    strncpy(dispBuf, fname, sizeof(dispBuf) - 1);
                    dispBuf[sizeof(dispBuf) - 1] = '\0';
                }
                else
                {
                    memcpy(dispBuf, fname, 8);
                    dispBuf[8] = '\xe2';
                    dispBuf[9] = '\x80';
                    dispBuf[10] = '\xa6';
                    memcpy(dispBuf + 11, fname + flen - 9, 9);
                    dispBuf[20] = '\0';
                }
                ImGui::SameLine(0, 5);
                ImGui::PushStyleColor(ImGuiCol_Text, ptAccent);
                ImGui::TextUnformatted(dispBuf);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", fname);
                ImGui::PopStyleColor();
            }
            // Morph slider — full remaining width
            float morphNorm = ov[OSC_WT_MORPH];
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::SliderFloat("##wtmorph", &morphNorm, 0.0f, 1.0f, "Morph %.2f"))
            {
                ov[OSC_WT_MORPH] = morphNorm;
                applyOscKnobValue(oscIdx, OSC_WT_MORPH);
            }
        }
        else // STD / FM ── original group + scope side-by-side layout
        {
            ImGui::BeginGroup();

            if (curPartType == 2) // FM_ENGINE
            {
                ImGui::Text("FM:");
                // Algorithm combo
                static const char *kAlgoNames[] = {"2op", "2op+FB", "4op Ser", "4op Par", "4op Mix"};
                int algo = (int)(ov[OSC_FM_ALGORITHM] + 0.5f);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::BeginCombo("##fmalgo", kAlgoNames[algo < 5 ? algo : 0]))
                {
                    for (int a = 0; a < 5; ++a)
                    {
                        if (ImGui::Selectable(kAlgoNames[a], a == algo))
                        {
                            ov[OSC_FM_ALGORITHM] = (float)a;
                            applyOscKnobValue(oscIdx, OSC_FM_ALGORITHM);
                        }
                    }
                    ImGui::EndCombo();
                }
                // FM knobs in a row
                static const int kFMParams[] = {OSC_FM_MOD_RATIO, OSC_FM_MOD_INDEX, OSC_FM_FEEDBACK, OSC_FM_OP2_RATIO, OSC_FM_OP2_LEVEL};
                static const float kFMMn[] = {0.1f, 0.0f, 0.0f, 0.1f, 0.0f};
                static const float kFMMx[] = {16.0f, 20.0f, 1.0f, 16.0f, 1.0f};
                for (int k = 0; k < 5; ++k)
                {
                    int pid = kFMParams[k];
                    float &val = ov[pid];
                    float knobSz = 28.0f;
                    if (k > 0)
                        ImGui::SameLine(0.0f, 4.0f);
                    ImGui::PushID(pid);
                    ImGui::SetNextItemWidth(knobSz);
                    if (ImGui::DragFloat("##fmk", &val, (kFMMx[k] - kFMMn[k]) * 0.005f, kFMMn[k], kFMMx[k], "%.2f"))
                        applyOscKnobValue(oscIdx, pid);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", OSC_KNOB_DEFS[pid].label);
                    ImGui::PopID();
                }
                // Labels row
                for (int k = 0; k < 5; ++k)
                {
                    if (k > 0)
                        ImGui::SameLine(0.0f, 4.0f);
                    ImGui::SetNextItemWidth(28.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, ptAccent);
                    ImGui::TextUnformatted(OSC_KNOB_DEFS[kFMParams[k]].label);
                    ImGui::PopStyleColor();
                }
            }
            else // STANDARD_OSC
            {
                ImGui::Text("Wave:");
                if (WaveSelector(&ov[OSC_WAVE]))
                    applyOscKnobValue(oscIdx, OSC_WAVE);
                // PCM load button — only shown when PCM_CUSTOM (index 8) is selected
                if ((int)(ov[OSC_WAVE] + 0.5f) == 8)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, OscAccentDim(0));
                    if (ImGui::SmallButton("Load PCM..."))
                    {
                        char filePath[MAX_PATH] = {};
                        OPENFILENAMEA ofn{};
                        ofn.lStructSize = sizeof(ofn);
                        ofn.lpstrFilter = "WAV Files\0*.wav\0All Files\0*.*\0";
                        ofn.lpstrFile = filePath;
                        ofn.nMaxFile = MAX_PATH;
                        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                        if (GetOpenFileNameA(&ofn))
                        {
                            m_pcmPaths[oscIdx] = filePath;
                            m_synth.loadOscPCM(oscIdx, m_pcmPaths[oscIdx]);
                        }
                    }
                    ImGui::PopStyleColor();
                    if (!m_pcmPaths[oscIdx].empty())
                    {
                        const char *start = m_pcmPaths[oscIdx].c_str();
                        const char *fname = start;
                        for (const char *p = start; *p; ++p)
                            if (*p == '\\' || *p == '/')
                                fname = p + 1;
                        char dispBuf[32];
                        int flen = (int)strlen(fname);
                        if (flen <= 22)
                        {
                            strncpy(dispBuf, fname, sizeof(dispBuf) - 1);
                            dispBuf[sizeof(dispBuf) - 1] = '\0';
                        }
                        else
                        {
                            memcpy(dispBuf, fname, 9);
                            dispBuf[9] = '\xe2';
                            dispBuf[10] = '\x80';
                            dispBuf[11] = '\xa6';
                            memcpy(dispBuf + 12, fname + flen - 10, 10);
                            dispBuf[22] = '\0';
                        }
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{0.5f, 0.8f, 0.5f, 1.0f});
                        ImGui::TextUnformatted(dispBuf);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("%s", fname);
                        ImGui::PopStyleColor();
                    }
                }
            }

            ImGui::EndGroup();
            ImGui::SameLine(0.0f, 8.0f);
            float scopeW = ImGui::GetContentRegionAvail().x - 2.0f;
            if (scopeW < 40.0f)
                scopeW = 40.0f;
            OscWavePreview((int)(ov[OSC_WAVE] + 0.5f), scopeW, 54.0f);
        }

        ImGui::Spacing();

        // ── Tune / Fine / Level / Pan ─────────────────────────────────────────
        {
            int base = oscIdx * OSC_PARAM_COUNT;
            float *prms[] = {&ov[OSC_TUNE], &ov[OSC_FINE_TUNE],
                             &ov[OSC_LEVEL], &ov[OSC_PAN]};
            const char *lbls[] = {"Tune", "Fine", "Level", "Pan"};
            const float mn[] = {-24.0f, -1.0f, 0.0f, -1.0f};
            const float mx[] = {24.0f, 1.0f, 1.0f, 1.0f};
            const char *fmts[] = {"%.0f", "%.2f", "%.2f", "%.2f"};
            const int pids[] = {OSC_TUNE, OSC_FINE_TUNE, OSC_LEVEL, OSC_PAN};
            if (KnobRow("osc_main", prms, lbls, mn, mx, fmts, 4, 40.0f, oscIdx, pids))
            {
                for (int p : {OSC_TUNE, OSC_FINE_TUNE, OSC_LEVEL, OSC_PAN})
                    applyOscKnobValue(oscIdx, p);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Filter ────────────────────────────────────────────────────────────
        ImGui::Text("Filter");
        ImGui::Spacing();
        {
            // Filter type selector
            static const char *kFiltTypeNames[] = {"LPF", "HPF", "BPF", "Notch"};
            static const BiquadType kFiltTypes[] = {
                BiquadType::LOWPASS, BiquadType::HIGHPASS,
                BiquadType::BANDPASS, BiquadType::NOTCH};

            int curFType = (int)(ov[OSC_FILT_TYPE] + 0.5f);
            if (curFType < 0)
                curFType = 0;
            if (curFType > 3)
                curFType = 3;

            for (int w = 0; w < 4; ++w)
            {
                if (w > 0)
                    ImGui::SameLine(0.0f, 3.0f);
                bool sel = (w == curFType);
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      sel ? Accent() : ImVec4{0.118f, 0.133f, 0.200f, 1.0f});
                ImGui::PushID(1000 + w);
                if (ImGui::SmallButton(kFiltTypeNames[w]))
                {
                    ov[OSC_FILT_TYPE] = (float)w;
                    applyOscKnobValue(oscIdx, OSC_FILT_TYPE);
                }
                ImGui::PopID();
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();

            // Cutoff / Reso / HPF / KeyTrack row
            float *fprms[] = {&ov[OSC_CUTOFF], &ov[OSC_RESO], &ov[OSC_HPF], &ov[OSC_KEY_TRACK]};
            const char *flbls[] = {"Cutoff", "Reso", "HPF", "KeyTrk"};
            const float fmn[] = {20.0f, 0.0f, 20.0f, 0.0f};
            const float fmx[] = {20000.0f, 1.0f, 2000.0f, 1.0f};
            const char *ffmts[] = {"%.0f", "%.2f", "%.0f", "%.2f"};
            const int fpids[] = {OSC_CUTOFF, OSC_RESO, OSC_HPF, OSC_KEY_TRACK};
            if (KnobRow("osc_filt", fprms, flbls, fmn, fmx, ffmts, 4, 38.0f, oscIdx, fpids))
            {
                for (int p : {OSC_CUTOFF, OSC_RESO, OSC_HPF, OSC_KEY_TRACK})
                    applyOscKnobValue(oscIdx, p);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Filter Envelope ───────────────────────────────────────────────────
        ImGui::Text("Filter ENV");
        ImGui::Spacing();
        {
            Widgets::ADSRParams fadsr;
            fadsr.attack = ov[OSC_FILT_ATK];
            fadsr.decay = ov[OSC_FILT_DEC];
            fadsr.sustain = ov[OSC_FILT_SUS];
            fadsr.release = ov[OSC_FILT_REL];
            {
                float _a = fadsr.attack, _d = fadsr.decay, _r = fadsr.release;
                float fenvMax = std::max({_a * 3.0f, (_a + _d) * 1.5f, _r * 5.5f, 0.3f});
                if (Widgets::ADSREnvelope("##fenv", &fadsr, fenvMax, {0.0f, 86.0f}))
                {
                    ov[OSC_FILT_ATK] = fadsr.attack;
                    ov[OSC_FILT_DEC] = fadsr.decay;
                    ov[OSC_FILT_SUS] = fadsr.sustain;
                    ov[OSC_FILT_REL] = fadsr.release;
                    for (int p : {OSC_FILT_ATK, OSC_FILT_DEC, OSC_FILT_SUS, OSC_FILT_REL})
                        applyOscKnobValue(oscIdx, p);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Amp Envelope ──────────────────────────────────────────────────────
        ImGui::Text("Amp ENV");
        ImGui::Spacing();
        {
            Widgets::ADSRParams aadsr;
            aadsr.attack = ov[OSC_AMP_ATK];
            aadsr.decay = ov[OSC_AMP_DEC];
            aadsr.sustain = ov[OSC_AMP_SUS];
            aadsr.release = ov[OSC_AMP_REL];
            {
                float _a = aadsr.attack, _d = aadsr.decay, _r = aadsr.release;
                float aenvMax = std::max({_a * 3.0f, (_a + _d) * 1.5f, _r * 5.5f, 0.3f});
                if (Widgets::ADSREnvelope("##aenv", &aadsr, aenvMax, {0.0f, 86.0f}))
                {
                    ov[OSC_AMP_ATK] = aadsr.attack;
                    ov[OSC_AMP_DEC] = aadsr.decay;
                    ov[OSC_AMP_SUS] = aadsr.sustain;
                    ov[OSC_AMP_REL] = aadsr.release;
                    for (int p : {OSC_AMP_ATK, OSC_AMP_DEC, OSC_AMP_SUS, OSC_AMP_REL})
                        applyOscKnobValue(oscIdx, p);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── FX section: per-osc effect selector + parameter knobs ───────────
        ImGui::Text("FX");
        ImGui::Spacing();
        {
            // Read current effect from voice 0 chain
            auto &slot = m_synth.getEngine().getAllocator().getVoice(0).getChain(oscIdx).getEffectSlot();
            EffectType curFx = slot.getEffectType();

            // Selector
            EffectType newFx = curFx;
            char fxComboId[16];
            snprintf(fxComboId, sizeof(fxComboId), "##oscfx%d", oscIdx);
            if (EffectSelector(fxComboId, curFx, &newFx))
            {
                float sr = (float)SAMPLE_RATE;
                m_synth.getEngine().getAllocator().configureVoices([oscIdx, newFx, sr](auto &v)
                                                                   {
                    if (newFx == EffectType::NONE)
                        v.getChain(oscIdx).getEffectSlot().clearEffect();
                    else
                        v.getChain(oscIdx).getEffectSlot().setEffect(createEffect(newFx, sr)); });
                curFx = newFx;
            }

            ImGui::Spacing();

            // Parameter knobs — horizontal row, labels from live effect
            const IEffect *eff = slot.getEffect();
            int nParams = eff ? eff->getNumParameters() : 0;

            if (curFx == EffectType::NONE)
                ImGui::BeginDisabled();

            int fxParams[] = {OSC_FX_P1, OSC_FX_P2, OSC_FX_P3};
            // Sync knob values from live effect before drawing
            for (int pi = 0; pi < 3; ++pi)
                if (eff && pi < nParams)
                    ov[fxParams[pi]] = eff->getParameter(pi);

            // Build label strings (must outlive KnobRow call)
            char lbuf[3][24];
            const char *lbls[3];
            float *prms[3];
            float fxmn[3] = {0.0f, 0.0f, 0.0f};
            float fxmx[3] = {1.0f, 1.0f, 1.0f};
            const char *fxfmt[3] = {"%.2f", "%.2f", "%.2f"};
            for (int pi = 0; pi < 3; ++pi)
            {
                const char *pname = (eff && pi < nParams) ? eff->getParameterName(pi) : nullptr;
                if (pname && pname[0])
                    snprintf(lbuf[pi], sizeof(lbuf[pi]), "%s", pname);
                else
                    snprintf(lbuf[pi], sizeof(lbuf[pi]), "P%d", pi + 1);
                lbls[pi] = lbuf[pi];
                prms[pi] = &ov[fxParams[pi]];
            }
            if (KnobRow("oscfx", prms, lbls, fxmn, fxmx, fxfmt, 3, 34.0f))
            {
                for (int pi = 0; pi < 3; ++pi)
                {
                    if (eff && pi < nParams)
                    {
                        float val = ov[fxParams[pi]];
                        m_synth.getEngine().getAllocator().configureVoices([oscIdx, pi, val](auto &v)
                                                                           {
                            IEffect *e = v.getChain(oscIdx).getEffectSlot().getEffect();
                            if (e) e->setParameter(pi, val); });
                    }
                }
            }

            if (curFx == EffectType::NONE)
                ImGui::EndDisabled();
        }

        ImGui::EndChild();
        ImGui::PopID();
    }

    // ─── Mixer panel ──────────────────────────────────────────────────────────
    void SynthViewer::drawImguiMixerPanel()
    {
        static constexpr float kChW = 58.0f;
        static constexpr float kChGap = 8.0f;
        static constexpr float kFaderH = 90.0f;
        static constexpr float kPanSz = 34.0f;
        const float panel_w = 5.0f * kChW + 4.0f * kChGap + 24.0f;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##mixer", {panel_w, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();

        SectionHeader("MIXER");

        // 4 OSC + 1 Master channel side by side
        for (int i = 0; i < 5; ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, kChGap);
            const bool is_master = (i == 4);

            float *level = is_master ? &m_glbKnobValues[GLB_VOLUME]
                                     : &m_oscKnobValues[i][OSC_LEVEL];
            float *pan = is_master ? nullptr
                                   : &m_oscKnobValues[i][OSC_PAN];

            ImGui::PushID(i);
            ImGui::BeginGroup();

            // Column header
            const char *col_lbl = is_master ? "MST" : kOscNames[i];
            float lw = ImGui::CalcTextSize(col_lbl).x;
            float col_x = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(col_x + (kChW - lw) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  is_master ? ImVec4{0.80f, 0.75f, 0.50f, 1.0f}
                                            : ImVec4{0.60f, 0.65f, 0.80f, 1.0f});
            ImGui::TextUnformatted(col_lbl);
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Fader centered
            float fader_indent = col_x + (kChW - 18.0f) * 0.5f;
            ImGui::SetCursorPosX(fader_indent);
            char fl_id[16];
            snprintf(fl_id, sizeof(fl_id), "L##%d", i);
            if (Widgets::SynthFader(fl_id, level, 0.0f, 1.0f, "%.2f", 18.0f, kFaderH))
            {
                if (is_master)
                    applyGlbKnobValue(GLB_VOLUME);
                else
                    applyOscKnobValue(i, OSC_LEVEL);
            }

            // Pan knob (OSC only)
            if (!is_master && pan)
            {
                float knob_indent = col_x + (kChW - kPanSz) * 0.5f;
                ImGui::SetCursorPosX(knob_indent);
                char fp_id[16];
                snprintf(fp_id, sizeof(fp_id), "P##%d", i);
                if (Widgets::SynthKnob(fp_id, pan, -1.0f, 1.0f, "%.2f", kPanSz))
                    applyOscKnobValue(i, OSC_PAN);
            }

            ImGui::EndGroup();
            ImGui::PopID();
        }

        // ── MOD MATRIX toggle at the bottom of the mixer ─────────────────────
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,
                              m_showModMatrix ? Accent() : ImVec4{0.118f, 0.133f, 0.200f, 1.0f});
        if (ImGui::Button("MOD MATRIX [M]", {-1.0f, 22.0f}))
            m_showModMatrix = !m_showModMatrix;
        ImGui::PopStyleColor();

        ImGui::EndChild();
    }

    // ─── Delay panel ──────────────────────────────────────────────────────────
    void SynthViewer::drawImguiDelayPanel()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##delay", {220.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("STEREO DELAY");

        // Bypass button at top
        bool dlyBypassed = m_synth.getMasterDelay().isBypassed();
        ImGui::PushStyleColor(ImGuiCol_Button,
                              dlyBypassed ? ImVec4{0.7f, 0.2f, 0.2f, 1.0f}
                                          : ImVec4{0.118f, 0.133f, 0.200f, 1.0f});
        if (ImGui::Button(dlyBypassed ? "BYPASS ON" : "Bypass", {-1.0f, 22.0f}))
            m_synth.getMasterDelay().setBypassed(!dlyBypassed);
        ImGui::PopStyleColor();

        ImGui::Spacing();

        if (dlyBypassed)
            ImGui::BeginDisabled();
        static constexpr float kKsz = 40.0f;
        static constexpr float kGap = 8.0f;

        struct DlyKnob
        {
            float *v;
            float mn, mx;
            const char *lbl, *fmt;
            int param;
        };
        DlyKnob row1[] = {
            {&m_glbKnobValues[GLB_DLY_L], 0.01f, 2.0f, "L", "%.2fs", GLB_DLY_L},
            {&m_glbKnobValues[GLB_DLY_R], 0.01f, 2.0f, "R", "%.2fs", GLB_DLY_R},
        };
        DlyKnob row2[] = {
            {&m_glbKnobValues[GLB_DLY_FB], 0.0f, 0.95f, "Fdbk", "%.0f%%", GLB_DLY_FB},
            {&m_glbKnobValues[GLB_DLY_MIX], 0.0f, 1.0f, "Mix", "%.0f%%", GLB_DLY_MIX},
        };

        auto drawRow = [&](DlyKnob *row, int n)
        {
            for (int i = 0; i < n; ++i)
            {
                if (i > 0)
                    ImGui::SameLine(0.0f, kGap);
                ImGui::PushID(row[i].param);
                if (Widgets::SynthKnob(row[i].lbl, row[i].v, row[i].mn, row[i].mx, row[i].fmt, kKsz))
                    applyGlbKnobValue(row[i].param);
                ImGui::PopID();
            }
        };

        drawRow(row1, 2);
        ImGui::Spacing();
        drawRow(row2, 2);
        ImGui::Spacing();

        // Damp — centered
        if (CenteredKnob("Damp", &m_glbKnobValues[GLB_DLY_DMP], 0.0f, 1.0f, "%.0f%%", kKsz))
            applyGlbKnobValue(GLB_DLY_DMP);

        ImGui::Spacing();

        // Ping-pong toggle
        bool pp = m_glbKnobValues[GLB_DLY_PING] > 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Button, pp ? Accent() : ImVec4{0.118f, 0.133f, 0.200f, 1.0f});
        if (ImGui::Button("Ping-Pong", {-1.0f, 22.0f}))
        {
            m_glbKnobValues[GLB_DLY_PING] = pp ? 0.0f : 1.0f;
            applyGlbKnobValue(GLB_DLY_PING);
        }
        ImGui::PopStyleColor();

        if (dlyBypassed)
            ImGui::EndDisabled();

        ImGui::EndChild();
    }

    // ─── Tube saturation panel ─────────────────────────────────────────────────
    void SynthViewer::drawImguiTubePanel()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##tube", {160.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("TUBE SAT");

        // ── Model selector ──────────────────────────────────────────────────
        static const char *kModelNames[] = {"FLAT", "12AX7", "12AT7", "12AU7", "EL34", "6L6"};
        static const TubeModel kModels[] = {
            TubeModel::FLAT,
            TubeModel::AX7, TubeModel::AT7, TubeModel::AU7,
            TubeModel::EL34, TubeModel::P6L6};
        static constexpr int kNumModels = 6;

        TubeModel curModel = m_synth.getMasterSaturation().getModel();
        int curIdx = 0;
        for (int i = 0; i < kNumModels; ++i)
            if (kModels[i] == curModel)
            {
                curIdx = i;
                break;
            }

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##tubemodel", kModelNames[curIdx]))
        {
            for (int i = 0; i < kNumModels; ++i)
            {
                bool sel = (i == curIdx);
                if (ImGui::Selectable(kModelNames[i], sel))
                    m_synth.getMasterSaturation().setModel(kModels[i]);
                if (sel)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // ── Knobs — vertical, centered (dimmed when FLAT) ────────────────────
        ImGui::Spacing();
        if (curModel == TubeModel::FLAT)
            ImGui::BeginDisabled();

        struct
        {
            float *v;
            float mn, mx;
            const char *lbl, *fmt;
            int param;
        } tubeKnobs[] = {
            {&m_glbKnobValues[GLB_TUBE_DRV], 0.5f, 8.0f, "Drive", "%.1f", GLB_TUBE_DRV},
            {&m_glbKnobValues[GLB_TUBE_ASYM], 0.0f, 1.0f, "Asym", "%.2f", GLB_TUBE_ASYM},
            {&m_glbKnobValues[GLB_TUBE_MIX], 0.0f, 1.0f, "Mix", "%.2f", GLB_TUBE_MIX},
        };
        for (auto &k : tubeKnobs)
        {
            ImGui::PushID(k.lbl);
            if (CenteredKnob(k.lbl, k.v, k.mn, k.mx, k.fmt, 40.0f))
                applyGlbKnobValue(k.param);
            ImGui::PopID();
            ImGui::Spacing();
        }

        if (curModel == TubeModel::FLAT)
            ImGui::EndDisabled();

        ImGui::EndChild();
    }

    // ─── MFX panel ────────────────────────────────────────────────────────────
    void SynthViewer::drawImguiMFXPanel()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##mfx", {160.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("MFX");

        // ── Effect type selector ─────────────────────────────────────────────
        EffectType curMFX = m_synth.getMasterFX().getEffectType();
        EffectType newMFX = curMFX;
        if (EffectSelector("##mfxtype", curMFX, &newMFX))
        {
            m_synth.setMasterEffect(newMFX); // both sides, at the engine's rate
            curMFX = newMFX;
        }

        ImGui::Spacing();

        // ── Parameter knobs — vertical, centered ─────────────────────────────
        const IEffect *meff = m_synth.getMasterFX().getEffect();
        int mParams = meff ? meff->getNumParameters() : 0;

        if (curMFX == EffectType::NONE)
            ImGui::BeginDisabled();

        int mfxGlb[] = {GLB_MFX_P1, GLB_MFX_P2, GLB_MFX_P3};
        for (int pi = 0; pi < 3; ++pi)
        {
            const char *pname = (meff && pi < mParams) ? meff->getParameterName(pi) : nullptr;
            char knobLabel[24];
            if (pname && pname[0])
                snprintf(knobLabel, sizeof(knobLabel), "%s", pname);
            else
                snprintf(knobLabel, sizeof(knobLabel), "P%d", pi + 1);

            // Sync from live effect
            if (meff && pi < mParams)
                m_glbKnobValues[mfxGlb[pi]] = meff->getParameter(pi);

            ImGui::PushID(pi);
            if (CenteredKnob(knobLabel, &m_glbKnobValues[mfxGlb[pi]], 0.0f, 1.0f, "%.2f", 40.0f))
            {
                if (meff && pi < mParams)
                    m_synth.setMasterFXParameter(pi, m_glbKnobValues[mfxGlb[pi]]);
            }
            ImGui::PopID();
            ImGui::Spacing();
        }

        if (curMFX == EffectType::NONE)
            ImGui::EndDisabled();

        ImGui::EndChild();
    }

    // ─── Master Filter panel ──────────────────────────────────────────────────
    void SynthViewer::drawImguiMasterFilterPanel()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##mfilt", {150.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("M.FILTER");

        // Bypass button
        bool bypassed = m_glbKnobValues[GLB_MF_BYPASS] > 0.5f;
        if (bypassed)
            ImGui::PushStyleColor(ImGuiCol_Button, {0.6f, 0.1f, 0.1f, 1.0f});
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));

        float bw = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button(bypassed ? "BYPASS ON" : "BYPASS", {bw, 20.0f}))
        {
            bypassed = !bypassed;
            m_glbKnobValues[GLB_MF_BYPASS] = bypassed ? 1.0f : 0.0f;
            m_synth.setMasterFilterBypassed(bypassed);
        }
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Filter type buttons: LPF / HPF / BPF / Notch
        static const char *kMFTypes[] = {"LPF", "HPF", "BPF", "Notch"};
        int mfType = (int)(m_glbKnobValues[GLB_MF_TYPE] + 0.5f);
        float tbw = (bw - ImGui::GetStyle().ItemSpacing.x * 3.0f) / 4.0f;
        for (int ti = 0; ti < 4; ++ti)
        {
            if (ti > 0)
                ImGui::SameLine();
            bool sel = (mfType == ti);
            if (sel)
                ImGui::PushStyleColor(ImGuiCol_Button, {0.22f, 0.45f, 0.72f, 1.0f});
            ImGui::PushID(ti + 1000);
            if (ImGui::Button(kMFTypes[ti], {tbw, 20.0f}))
            {
                m_glbKnobValues[GLB_MF_TYPE] = (float)ti;
                m_synth.setMasterFilterType(static_cast<BiquadType>(ti));
            }
            ImGui::PopID();
            if (sel)
                ImGui::PopStyleColor();
        }
        ImGui::Spacing();

        if (bypassed)
            ImGui::BeginDisabled();

        // Cutoff knob (logarithmic feel: store linear, display Hz)
        if (CenteredKnob("Cutoff", &m_glbKnobValues[GLB_MF_CUTOFF], 20.0f, 20000.0f, "%.0f", 44.0f))
            m_synth.setMasterFilterCutoff(m_glbKnobValues[GLB_MF_CUTOFF]);

        ImGui::Spacing();

        if (CenteredKnob("Reso", &m_glbKnobValues[GLB_MF_RESO], 0.0f, 1.0f, "%.2f", 44.0f))
            m_synth.setMasterFilterResonance(m_glbKnobValues[GLB_MF_RESO]);

        if (bypassed)
            ImGui::EndDisabled();

        ImGui::EndChild();
    }

    // ─── LFO panel ────────────────────────────────────────────────────────────
    void SynthViewer::drawImguiLFOPanel()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##lfo", {200.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("LFOs");

        static const char *kLFOWaveNames[] = {"Sin", "Tri", "Sq", "Saw", "Rnd"};
        static const LFOWaveType kLFOWaveTypes[] = {
            LFOWaveType::SINE, LFOWaveType::TRIANGLE, LFOWaveType::SQUARE,
            LFOWaveType::SAW, LFOWaveType::RANDOM};

        for (int i = 0; i < 4; ++i)
        {
            LFO &lfo = m_synth.getLFO(i);
            ImGui::PushID(i);

            // Row label
            char rowLabel[8];
            snprintf(rowLabel, sizeof(rowLabel), "LFO%d", i + 1);
            ImGui::PushStyleColor(ImGuiCol_Text, Accent());
            ImGui::TextUnformatted(rowLabel);
            ImGui::PopStyleColor();
            ImGui::SameLine(42.0f);

            // Rate knob (inline)
            float rate = lfo.getFrequency();
            ImGui::SetNextItemWidth(36.0f);
            if (Widgets::SynthKnob("##rate", &rate, 0.01f, 20.0f, "%.2f", 34.0f))
                lfo.setFrequency(rate);
            ImGui::SameLine(0.0f, 6.0f);

            // Wave selector buttons
            LFOWaveType curWave = lfo.getWaveType();
            for (int w = 0; w < 5; ++w)
            {
                if (w > 0)
                    ImGui::SameLine(0.0f, 2.0f);
                bool sel = (curWave == kLFOWaveTypes[w]);
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      sel ? Accent() : ImVec4{0.118f, 0.133f, 0.200f, 1.0f});
                ImGui::PushID(w);
                if (ImGui::SmallButton(kLFOWaveNames[w]))
                    lfo.setWaveType(kLFOWaveTypes[w]);
                ImGui::PopID();
                ImGui::PopStyleColor();
            }

            if (i < 3)
                ImGui::Spacing();
            ImGui::PopID();
        }

        ImGui::EndChild();
    }

    // ─── XY Pad panel ─────────────────────────────────────────────────────────
    void SynthViewer::drawImguiXYPad()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
        ImGui::BeginChild("##xypad", {170.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("XY PAD");

        // Square pad — fills available space
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float padSz = avail.x < avail.y ? avail.x : avail.y;
        padSz -= 2.0f;
        if (padSz < 10.0f)
            padSz = 10.0f;

        ImVec2 padPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##xybtn", {padSz, padSz});

        bool active = ImGui::IsItemActive();
        bool hovered = ImGui::IsItemHovered();

        if (active)
        {
            ImVec2 mp = ImGui::GetMousePos();
            float nx = (mp.x - padPos.x) / padSz;
            float ny = 1.0f - (mp.y - padPos.y) / padSz; // Y flipped (top=1)
            m_xyPad[0] = nx < 0.0f ? 0.0f : nx > 1.0f ? 1.0f
                                                      : nx;
            m_xyPad[1] = ny < 0.0f ? 0.0f : ny > 1.0f ? 1.0f
                                                      : ny;

            // Push into modulation matrix [-1..1]
            float srcX = m_xyPad[0] * 2.0f - 1.0f;
            float srcY = m_xyPad[1] * 2.0f - 1.0f;
            m_synth.getEngine().getModMatrix().setSourceValue((int)ObsSynthSrc::XY_X, srcX);
            m_synth.getEngine().getModMatrix().setSourceValue((int)ObsSynthSrc::XY_Y, srcY);
        }

        // Draw pad background + crosshair
        ImDrawList *dl = ImGui::GetWindowDrawList();
        ImU32 bgCol = IM_COL32(20, 22, 38, 255);
        ImU32 gridCol = IM_COL32(50, 60, 100, 120);
        ImU32 dotCol = active    ? IM_COL32(80, 160, 255, 255)
                       : hovered ? IM_COL32(60, 120, 200, 220)
                                 : IM_COL32(50, 100, 180, 200);

        dl->AddRectFilled(padPos, {padPos.x + padSz, padPos.y + padSz}, bgCol);
        // Grid lines
        float mid = padPos.x + padSz * 0.5f;
        float midY = padPos.y + padSz * 0.5f;
        dl->AddLine({mid, padPos.y}, {mid, padPos.y + padSz}, gridCol);
        dl->AddLine({padPos.x, midY}, {padPos.x + padSz, midY}, gridCol);
        dl->AddRect(padPos, {padPos.x + padSz, padPos.y + padSz},
                    IM_COL32(60, 80, 140, 200));

        // Dot at current XY position
        float dotX = padPos.x + m_xyPad[0] * padSz;
        float dotY = padPos.y + (1.0f - m_xyPad[1]) * padSz;
        dl->AddCircleFilled({dotX, dotY}, 6.0f, dotCol);
        dl->AddCircle({dotX, dotY}, 8.0f, IM_COL32(255, 255, 255, 120));

        // Labels
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        char xyTxt[32];
        snprintf(xyTxt, sizeof(xyTxt), "X %.2f  Y %.2f",
                 static_cast<double>(m_xyPad[0] * 2.0f - 1.0f),
                 static_cast<double>(m_xyPad[1] * 2.0f - 1.0f));
        float tw = ImGui::CalcTextSize(xyTxt).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.6f, 0.8f, 1.0f});
        ImGui::TextUnformatted(xyTxt);
        ImGui::PopStyleColor();

        ImGui::EndChild();
    }

    // ─── Scope panel (wave visualizer) ────────────────────────────────────────
    void SynthViewer::drawImguiScopePanel()
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.039f, 0.039f, 0.055f, 1.0f});
        ImGui::BeginChild("##scope", {0.0f, 0.0f}, ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        SectionHeader("SCOPE");

        // Read latest audio from lock-free capture buffer
        float wave_buf[WaveCapture::DISPLAY_SAMPLES];
        m_waveCapture.readTriggered(wave_buf);

        ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGui::PushStyleColor(ImGuiCol_PlotLines, {0.267f, 0.533f, 1.0f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.025f, 0.025f, 0.035f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_PlotLinesHovered, {0.4f, 0.7f, 1.0f, 1.0f});
        ImGui::PlotLines("##wave",
                         wave_buf, WaveCapture::DISPLAY_SAMPLES,
                         0, nullptr,
                         -1.0f, 1.0f,
                         {avail.x, avail.y - 4.0f});
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
    }

    // ─── Mod Matrix panel ─────────────────────────────────────────────────────
    void SynthViewer::drawImguiModMatrix()
    {
        ImGui::SetNextWindowSize({1300.0f, 960.0f}, ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Modulation Matrix", &m_showModMatrix))
        {
            ImGui::End();
            return;
        }

        auto &mm = m_synth.getModMatrix();
        auto &alloc = m_synth.getEngine().getAllocator();

        PartType ptypes[4];
        for (int i = 0; i < 4; ++i)
            ptypes[i] = alloc.getVoice(0).getChain(i).getPartType();

        struct DynCol
        {
            const char *label;
            ObsSynthDst dst;
        };
        struct Section
        {
            const char *name;
            int start;
            int count;
            int osc;
        };

        static constexpr int OSC_BASE[4] = {0, 17, 34, 51};
        static const char *OSC_NAMES[4] = {"OSC 1", "OSC 2", "OSC 3", "OSC 4"};

        // ── OSC table columns (all 15-16 params per PartType) ─────────────────
        std::vector<DynCol> oscCols;
        oscCols.reserve(68);
        Section oscSecs[4] = {};

        for (int i = 0; i < 4; ++i)
        {
            oscSecs[i] = {OSC_NAMES[i], (int)oscCols.size(), 0, i};
            int b = OSC_BASE[i];
            switch (ptypes[i])
            {
            case PartType::WAVETABLE:
                oscCols.push_back({"Pt", (ObsSynthDst)(b + 0)});
                oscCols.push_back({"Fi", (ObsSynthDst)(b + 1)});
                oscCols.push_back({"Mx", (ObsSynthDst)(68 + i)}); // WT_MORPH
                oscCols.push_back({"Lvl", (ObsSynthDst)(b + 3)});
                oscCols.push_back({"Pan", (ObsSynthDst)(b + 4)});
                oscCols.push_back({"Cut", (ObsSynthDst)(b + 7)});
                oscCols.push_back({"Res", (ObsSynthDst)(b + 8)});
                oscCols.push_back({"FEn", (ObsSynthDst)(b + 9)});
                oscCols.push_back({"Atk", (ObsSynthDst)(b + 10)});
                oscCols.push_back({"Dec", (ObsSynthDst)(b + 11)});
                oscCols.push_back({"Sus", (ObsSynthDst)(b + 12)});
                oscCols.push_back({"Rel", (ObsSynthDst)(b + 13)});
                oscCols.push_back({"FxM", (ObsSynthDst)(b + 14)});
                oscCols.push_back({"Fx1", (ObsSynthDst)(b + 15)});
                oscCols.push_back({"Fx2", (ObsSynthDst)(b + 16)});
                break;
            case PartType::FM_ENGINE:
                oscCols.push_back({"Pt", (ObsSynthDst)(b + 0)});
                oscCols.push_back({"Fi", (ObsSynthDst)(b + 1)});
                oscCols.push_back({"FMi", (ObsSynthDst)(b + 6)});
                oscCols.push_back({"Lvl", (ObsSynthDst)(b + 3)});
                oscCols.push_back({"Pan", (ObsSynthDst)(b + 4)});
                oscCols.push_back({"Cut", (ObsSynthDst)(b + 7)});
                oscCols.push_back({"Res", (ObsSynthDst)(b + 8)});
                oscCols.push_back({"FEn", (ObsSynthDst)(b + 9)});
                oscCols.push_back({"Atk", (ObsSynthDst)(b + 10)});
                oscCols.push_back({"Dec", (ObsSynthDst)(b + 11)});
                oscCols.push_back({"Sus", (ObsSynthDst)(b + 12)});
                oscCols.push_back({"Rel", (ObsSynthDst)(b + 13)});
                oscCols.push_back({"FxM", (ObsSynthDst)(b + 14)});
                oscCols.push_back({"Fx1", (ObsSynthDst)(b + 15)});
                oscCols.push_back({"Fx2", (ObsSynthDst)(b + 16)});
                break;
            default: // STANDARD_OSC
                oscCols.push_back({"Pt", (ObsSynthDst)(b + 0)});
                oscCols.push_back({"Fi", (ObsSynthDst)(b + 1)});
                oscCols.push_back({"Wv", (ObsSynthDst)(b + 2)});
                oscCols.push_back({"Lvl", (ObsSynthDst)(b + 3)});
                oscCols.push_back({"Pan", (ObsSynthDst)(b + 4)});
                oscCols.push_back({"Pwm", (ObsSynthDst)(b + 5)});
                oscCols.push_back({"Cut", (ObsSynthDst)(b + 7)});
                oscCols.push_back({"Res", (ObsSynthDst)(b + 8)});
                oscCols.push_back({"FEn", (ObsSynthDst)(b + 9)});
                oscCols.push_back({"Atk", (ObsSynthDst)(b + 10)});
                oscCols.push_back({"Dec", (ObsSynthDst)(b + 11)});
                oscCols.push_back({"Sus", (ObsSynthDst)(b + 12)});
                oscCols.push_back({"Rel", (ObsSynthDst)(b + 13)});
                oscCols.push_back({"FxM", (ObsSynthDst)(b + 14)});
                oscCols.push_back({"Fx1", (ObsSynthDst)(b + 15)});
                oscCols.push_back({"Fx2", (ObsSynthDst)(b + 16)});
                break;
            }
            oscSecs[i].count = (int)oscCols.size() - oscSecs[i].start;
        }

        // ── General table columns ──────────────────────────────────────────────
        std::vector<DynCol> genCols;
        Section genSec = {"General", 0, 0, -1};
        genCols.push_back({"L1r", ObsSynthDst::LFO1_RATE});
        genCols.push_back({"L2r", ObsSynthDst::LFO2_RATE});
        genCols.push_back({"L3r", ObsSynthDst::LFO3_RATE});
        genCols.push_back({"L4r", ObsSynthDst::LFO4_RATE});
        genCols.push_back({"L1d", ObsSynthDst::LFO1_DEPTH});
        genCols.push_back({"L2d", ObsSynthDst::LFO2_DEPTH});
        genCols.push_back({"L3d", ObsSynthDst::LFO3_DEPTH});
        genCols.push_back({"L4d", ObsSynthDst::LFO4_DEPTH});
        genCols.push_back({"Mx1", ObsSynthDst::MIX_CH1_LVL});
        genCols.push_back({"Mx2", ObsSynthDst::MIX_CH2_LVL});
        genCols.push_back({"Mx3", ObsSynthDst::MIX_CH3_LVL});
        genCols.push_back({"Mx4", ObsSynthDst::MIX_CH4_LVL});
        genCols.push_back({"FXm", ObsSynthDst::MFX_MIX});
        genCols.push_back({"FXp1", ObsSynthDst::MFX_P1});
        genCols.push_back({"FXp2", ObsSynthDst::MFX_P2});
        genCols.push_back({"TDr", ObsSynthDst::TUBE_DRIVE});
        genCols.push_back({"TAs", ObsSynthDst::TUBE_ASYM});
        genCols.push_back({"TCh", ObsSynthDst::TUBE_CHAOS});
        genCols.push_back({"MVol", ObsSynthDst::MASTER_VOL});
        genSec.count = (int)genCols.size();

        // ── Shared style constants ─────────────────────────────────────────────
        static constexpr ImU32 SEC_BG[2] = {IM_COL32(22, 25, 40, 210), IM_COL32(18, 21, 35, 210)};
        static constexpr ImU32 SEC_SEP = IM_COL32(60, 70, 110, 220);
        static constexpr ImU32 PT_ACCENT[3] = {
            IM_COL32(90, 130, 200, 255), // STD — blue
            IM_COL32(60, 200, 130, 255), // WT  — green
            IM_COL32(200, 120, 50, 255), // FM  — orange
        };
        const float label_w = 40.0f;
        const float sec_hdr_h = 14.0f;
        const float col_lbl_h = 13.0f;
        const float cell_h = 17.0f;
        const float table_h = sec_hdr_h + col_lbl_h + GRID_TOTAL_ROWS * cell_h + 4.0f;
        int numRows = GRID_TOTAL_ROWS;

        // ── Lambda: render one table inside a child window (gives clip rect) ───
        auto renderTable = [&](const char *child_id,
                               const std::vector<DynCol> &cols,
                               Section *sections, int numSections)
        {
            int numCols = (int)cols.size();

            ImGui::BeginChild(child_id, {0.0f, table_h},
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            float avail_w_child = ImGui::GetContentRegionAvail().x;
            float cell_w = (avail_w_child - label_w - 4.0f) / (float)numCols;
            if (cell_w < 8.0f)
                cell_w = 8.0f;
            if (cell_w > 30.0f)
                cell_w = 30.0f;

            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 win_pos = ImGui::GetWindowPos();
            ImVec2 win_size = ImGui::GetWindowSize();
            dl->PushClipRect(win_pos,
                             {win_pos.x + win_size.x, win_pos.y + win_size.y}, true);

            ImVec2 cp = ImGui::GetCursorScreenPos();
            float ox = cp.x;
            float oy = cp.y;
            float total_h = sec_hdr_h + col_lbl_h + numRows * cell_h;

            // Section header row
            for (int si = 0; si < numSections; ++si)
            {
                const Section &sec = sections[si];
                float sx0 = ox + label_w + sec.start * cell_w;
                float sx1 = sx0 + sec.count * cell_w;
                float sxm = (sx0 + sx1) * 0.5f;
                dl->AddRectFilled({sx0, oy}, {sx1, oy + sec_hdr_h}, SEC_BG[si & 1]);
                ImU32 accent = (sec.osc >= 0)
                                   ? PT_ACCENT[(int)ptypes[sec.osc]]
                                   : IM_COL32(120, 120, 160, 200);
                dl->AddLine({sx0, oy + sec_hdr_h - 1.0f},
                            {sx1 - 1.0f, oy + sec_hdr_h - 1.0f}, accent, 2.0f);
                float tw = ImGui::CalcTextSize(sec.name).x;
                dl->AddText({sxm - tw * 0.5f, oy + 1.0f},
                            IM_COL32(210, 215, 240, 255), sec.name);
                if (si < numSections - 1)
                    dl->AddLine({sx1, oy}, {sx1, oy + total_h}, SEC_SEP, 2.0f);
            }
            oy += sec_hdr_h;

            // Column label row
            for (int c = 0; c < numCols; ++c)
            {
                float cx = ox + label_w + c * cell_w + cell_w * 0.5f;
                float tw = ImGui::CalcTextSize(cols[c].label).x;
                dl->AddText({cx - tw * 0.5f, oy},
                            IM_COL32(100, 115, 145, 200), cols[c].label);
            }
            oy += col_lbl_h;

            // Data rows
            for (int r = 0; r < numRows; ++r)
            {
                float row_y = oy + r * cell_h;
                if (r & 1)
                    dl->AddRectFilled({ox, row_y},
                                      {ox + label_w + numCols * cell_w, row_y + cell_h},
                                      IM_COL32(0, 0, 0, 18));
                dl->AddText({ox + 2.0f, row_y + (cell_h - ImGui::GetTextLineHeight()) * 0.5f},
                            IM_COL32(160, 165, 185, 220), GRID_ROWS[r].label);

                int srcId = (int)GRID_ROWS[r].src_id;
                for (int c = 0; c < numCols; ++c)
                {
                    float cx0 = ox + label_w + c * cell_w + 1.0f;
                    float cy0 = row_y + 1.0f;
                    float cx1 = cx0 + cell_w - 2.0f;
                    float cy1 = cy0 + cell_h - 2.0f;
                    int dstId = (int)cols[c].dst;

                    int slot_idx = -1;
                    float slot_amt = 0.0f;
                    for (int s = 0; s < MAX_MOD_SLOTS; ++s)
                    {
                        const auto *sl = mm.getSlot(s);
                        if (sl && sl->enabled && sl->source == srcId && sl->destination == dstId)
                        {
                            slot_idx = s;
                            slot_amt = sl->amount;
                            break;
                        }
                    }

                    bool on = (slot_idx >= 0);
                    float t_pos = (on && slot_amt >= 0.0f) ? slot_amt : 0.0f;
                    float t_neg = (on && slot_amt < 0.0f) ? -slot_amt : 0.0f;
                    ImU32 bg = on
                                   ? (slot_amt >= 0.0f
                                          ? IM_COL32((int)(34 + t_pos * 34), (int)(68 + t_pos * 68), (int)(200 + t_pos * 55), 220)
                                          : IM_COL32((int)(200 + t_neg * 55), (int)(60 + t_neg * 20), 60, 220))
                                   : IM_COL32(18, 20, 30, 200);
                    ImU32 rim = on ? IM_COL32(100, 160, 255, 200) : IM_COL32(35, 40, 58, 180);
                    dl->AddRectFilled({cx0, cy0}, {cx1, cy1}, bg, 2.0f);
                    dl->AddRect({cx0, cy0}, {cx1, cy1}, rim, 2.0f, 0, 1.0f);

                    ImGui::SetCursorScreenPos({cx0, cy0});
                    ImGui::PushID(r * 1000 + dstId);
                    if (ImGui::InvisibleButton("##cell", {cell_w - 2.0f, cell_h - 2.0f}))
                    {
                        if (!on)
                            mm.addRoute(srcId, dstId, 0.5f);
                        else
                            mm.removeRoute(slot_idx);
                    }
                    if (ImGui::IsItemHovered() && on && ImGui::GetIO().MouseWheel != 0.0f)
                    {
                        float new_amt = slot_amt + ImGui::GetIO().MouseWheel * 0.05f;
                        if (new_amt > 1.0f)
                            new_amt = 1.0f;
                        if (new_amt < -1.0f)
                            new_amt = -1.0f;
                        if (std::fabs(new_amt) < 0.02f)
                            mm.removeRoute(slot_idx);
                        else
                            mm.setSlotAmount(slot_idx, new_amt);
                    }
                    if (ImGui::IsItemHovered() && on)
                        ImGui::SetTooltip("%s \xe2\x86\x92 %s  %.2f",
                                          GRID_ROWS[r].label, cols[c].label,
                                          static_cast<double>(slot_amt));
                    ImGui::PopID();
                }
            }

            dl->PopClipRect();
            ImGui::EndChild();
        };

        // ── Table 1: Oscillators ───────────────────────────────────────────────
        renderTable("##mm_osc", oscCols, oscSecs, 4);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Table 2: General ──────────────────────────────────────────────────
        renderTable("##mm_gen", genCols, &genSec, 1);

        ImGui::End();
    }

    // ─── Master ImGui UI ──────────────────────────────────────────────────────
    void SynthViewer::drawImguiUI()
    {
        // Sync knobs from live engine every frame so MIDI CC changes are reflected
        syncKnobsFromEngine();

        // Reset MIDI learn click tracking for this frame
        g_midiLearnKnobClicked = false;
        g_midiLearnClickOsc = -1;
        g_midiLearnClickParam = -1;

        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowPos({0.0f, 0.0f});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f, 8.0f});
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        float avail_w = ImGui::GetContentRegionAvail().x;
        float avail_h = ImGui::GetContentRegionAvail().y;

        // ─── Row 1: 4 OSC panels ──────────────────────────────────────────────
        // Give oscillators more vertical room so knobs/envelopes fit without scroll
        float osc_row_h = avail_h * 0.78f;
        float osc_col_w = (avail_w - 18.0f) / 4.0f;

        for (int i = 0; i < 4; ++i)
        {
            if (i > 0)
                ImGui::SameLine(0.0f, 6.0f);
            ImGui::BeginChild(ImGui::GetID((void *)(intptr_t)i),
                              {osc_col_w, osc_row_h}, ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            drawImguiOscPanel(i);
            ImGui::EndChild();
        }

        // ─── Row 2: bottom strip — in signal-path order ───────────────────────
        // LFO → Mixer → Master Filter → Tube Sat → Delay → MFX → XY Pad → Buttons → Scope
        float remaining_h = avail_h - osc_row_h - 10.0f;
        ImGui::Spacing();

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiLFOPanel();
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiMixerPanel();
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiMasterFilterPanel();
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiTubePanel();
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiDelayPanel();
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiMFXPanel();
        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {9999.0f, remaining_h});
        drawImguiXYPad();
        ImGui::SameLine(0.0f, 6.0f);

        // ─── Buttons panel ────────────────────────────────────────────────────
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.063f, 0.067f, 0.098f, 1.0f});
            ImGui::BeginChild("##buttons_panel", {100.0f, remaining_h}, ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleColor();

            // MIDI MAP
            if (m_midiLearnActive)
                ImGui::PushStyleColor(ImGuiCol_Button, {0.8f, 0.1f, 0.1f, 1.0f});
            else
                ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.15f, 0.2f, 1.0f});
            if (ImGui::Button(m_midiLearnActive ? "MAP [ON]" : "MAP", {-1.0f, 22.0f}))
            {
                m_midiLearnActive = !m_midiLearnActive;
                m_midiLearnTarget = {-1, -1};
            }
            ImGui::PopStyleColor();

            if (m_midiLearnActive)
                ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "Click knob\nthen CC");

            // Clear Maps
            ImGui::Spacing();
            if (ImGui::Button("Clear Map", {-1.0f, 22.0f}))
                m_midiLearnMap.clear();

            // Mapping count
            ImGui::Spacing();
            ImGui::Text("Maps: %d", m_midiLearnMap.getCount());

            ImGui::EndChild();
        }
        ImGui::SameLine(0.0f, 6.0f);

        // Scope fills remaining width
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.039f, 0.039f, 0.055f, 1.0f});
            ImGui::BeginChild("##scope_wrap", {0.0f, remaining_h},
                              ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleColor();
            drawImguiScopePanel();
            ImGui::EndChild();
        }

        ImGui::End(); // ##root

        // MIDI learn: if in learn mode and a knob was clicked, set target
        if (m_midiLearnActive && g_midiLearnKnobClicked)
        {
            m_midiLearnTarget = {g_midiLearnClickOsc, g_midiLearnClickParam};
        }

        // MIDI learn overlay: highlight targeted knob
        drawImguiMidiLearnOverlay();

        // Mod matrix overlay
        if (m_showModMatrix)
            drawImguiModMatrix();
    }

    // ─── MIDI Learn Overlay ──────────────────────────────────────────────────
    void SynthViewer::drawImguiMidiLearnOverlay()
    {
        if (!m_midiLearnActive)
            return;

        // Show CC assignment badges on all mapped knobs
        ImDrawList *fg = ImGui::GetForegroundDrawList();
        for (const auto &mapping : m_midiLearnMap.getMappings())
        {
            if (!mapping.active)
                continue;
            // Find the knob rect for this target
            for (int k = 0; k < m_knobRectCount; ++k)
            {
                if (m_knobRects[k].oscIdx == mapping.target.oscIdx &&
                    m_knobRects[k].paramIdx == mapping.target.paramIdx)
                {
                    // Draw CC badge
                    float cx = (float)m_knobRects[k].x + m_knobRects[k].size * 0.5f;
                    float cy = (float)m_knobRects[k].y;
                    char badge[8];
                    snprintf(badge, sizeof(badge), "%d", mapping.cc);
                    fg->AddRectFilled({cx - 10, cy - 12}, {cx + 10, cy + 2},
                                      IM_COL32(200, 60, 60, 220), 3.0f);
                    fg->AddText({cx - 8, cy - 11}, IM_COL32(255, 255, 255, 255), badge);
                    break;
                }
            }
        }

        // Highlight the awaiting-CC target knob
        if (m_midiLearnTarget.isValid())
        {
            for (int k = 0; k < m_knobRectCount; ++k)
            {
                if (m_knobRects[k].oscIdx == m_midiLearnTarget.oscIdx &&
                    m_knobRects[k].paramIdx == m_midiLearnTarget.paramIdx)
                {
                    float x = (float)m_knobRects[k].x;
                    float y = (float)m_knobRects[k].y;
                    float sz = (float)m_knobRects[k].size;
                    // Pulsing border
                    float t = (float)ImGui::GetTime();
                    float alpha = 0.5f + 0.5f * sinf(t * 6.0f);
                    fg->AddRect({x - 2, y - 2}, {x + sz + 2, y + sz + 16},
                                IM_COL32(255, 80, 80, (int)(alpha * 255)), 4.0f, 0, 2.0f);
                    break;
                }
            }
        }
    }

} // namespace synth
