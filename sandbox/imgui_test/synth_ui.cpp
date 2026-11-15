#include "synth_ui.hpp"
#include "imgui.h"
#include "imgui-knobs.h"
#include <cstdio>

static const char *kOscNames[4] = {"OSC 1", "OSC 2", "OSC 3", "OSC 4"};
static const char *kWaveNames[] = {"Sine", "Saw", "Square", "Triangle"};

// ─── Accent color helper ──────────────────────────────────────────────────────
static ImVec4 Accent() { return {0.267f, 0.533f, 1.000f, 1.0f}; }
static ImVec4 AccentDim() { return {0.165f, 0.282f, 0.604f, 1.0f}; }

// ─── Section header ───────────────────────────────────────────────────────────
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

// ─── Knob row helper ──────────────────────────────────────────────────────────
// Draws n knobs from params array with labels[], formats[], min[], max[].
// Keeps them on the same line with SameLine.
static bool KnobRow(const char *id_prefix,
                    float *params[], const char *labels[],
                    const float *mins, const float *maxs,
                    const char *fmts[], int n,
                    float knob_size = 44.0f)
{
    bool changed = false;
    ImGui::PushID(id_prefix);
    for (int i = 0; i < n; ++i)
    {
        if (i > 0)
            ImGui::SameLine(0.0f, 10.0f);
        ImGui::PushID(i);
        changed |= Widgets::SynthKnob(labels[i], params[i],
                                      mins[i], maxs[i], fmts[i], knob_size);
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}

// ─── Wave selector ────────────────────────────────────────────────────────────
static void WaveSelector(float *wave_idx)
{
    int wi = (int)(*wave_idx + 0.5f);
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            ImGui::SameLine(0.0f, 4.0f);
        bool sel = (wi == i);
        if (sel)
            ImGui::PushStyleColor(ImGuiCol_Button, Accent());
        else
            ImGui::PushStyleColor(ImGuiCol_Button, {0.118f, 0.133f, 0.200f, 1.0f});
        ImGui::PushID(i);
        if (ImGui::SmallButton(kWaveNames[i]))
            *wave_idx = (float)i;
        ImGui::PopID();
        ImGui::PopStyleColor();
    }
}

// ─── Draw one oscillator panel ───────────────────────────────────────────────
static void DrawOscPanel(int idx, OscParams &p,
                         Widgets::ADSRParams &fadsr,
                         Widgets::ADSRParams &aadsr)
{
    ImGui::PushID(idx);

    // Panel child window
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.063f, 0.067f, 0.098f, 1.0f});
    ImGui::BeginChild("##osc", {0.0f, 0.0f}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    SectionHeader(kOscNames[idx]);

    // ── Waveform selector
    ImGui::Text("Wave:");
    ImGui::SameLine();
    WaveSelector(&p.wave);

    ImGui::Spacing();

    // ── Tune / Fine / Level / Pan knobs on one row
    {
        float *prms[] = {&p.tune, &p.fine, &p.level, &p.pan};
        const char *lbls[] = {"Tune", "Fine", "Level", "Pan"};
        const float mn[] = {-24.0f, -100.0f, 0.0f, -1.0f};
        const float mx[] = {24.0f, 100.0f, 1.0f, 1.0f};
        const char *fmts[] = {"%.0f st", "%.0f ct", "%.2f", "%.2f"};
        KnobRow("osc_main", prms, lbls, mn, mx, fmts, 4, 42.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Filter ENV + Cutoff/Res
    ImGui::Text("Filter Envelope");
    ImGui::Spacing();
    fadsr.attack = p.f_attack;
    fadsr.decay = p.f_decay;
    fadsr.sustain = p.f_sustain;
    fadsr.release = p.f_release;
    if (Widgets::ADSREnvelope("##fenv", &fadsr, 2.0f, {0.0f, 90.0f}))
    {
        p.f_attack = fadsr.attack;
        p.f_decay = fadsr.decay;
        p.f_sustain = fadsr.sustain;
        p.f_release = fadsr.release;
    }
    ImGui::Spacing();
    {
        float *prms[] = {&p.f_cutoff, &p.f_res};
        const char *lbls[] = {"Cutoff", "Res"};
        const float mn[] = {0.0f, 0.0f};
        const float mx[] = {1.0f, 1.0f};
        const char *fmts[] = {"%.2f", "%.2f"};
        KnobRow("osc_flt", prms, lbls, mn, mx, fmts, 2, 38.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Amp ENV
    ImGui::Text("Amp Envelope");
    ImGui::Spacing();
    aadsr.attack = p.a_attack;
    aadsr.decay = p.a_decay;
    aadsr.sustain = p.a_sustain;
    aadsr.release = p.a_release;
    if (Widgets::ADSREnvelope("##aenv", &aadsr, 2.0f, {0.0f, 90.0f}))
    {
        p.a_attack = aadsr.attack;
        p.a_decay = aadsr.decay;
        p.a_sustain = aadsr.sustain;
        p.a_release = aadsr.release;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Chorus knobs
    ImGui::Text("Chorus");
    ImGui::Spacing();
    {
        float *prms[] = {&p.chorus_rate, &p.chorus_depth, &p.chorus_mix};
        const char *lbls[] = {"Rate", "Depth", "Mix"};
        const float mn[] = {0.0f, 0.0f, 0.0f};
        const float mx[] = {10.0f, 1.0f, 1.0f};
        const char *fmts[] = {"%.1f Hz", "%.2f", "%.2f"};
        KnobRow("osc_chr", prms, lbls, mn, mx, fmts, 3, 38.0f);
    }

    ImGui::EndChild();
    ImGui::PopID();
}

// ─── Mixer panel ─────────────────────────────────────────────────────────────
static void DrawMixerPanel(OscParams osc[], MasterParams &mp)
{
    // Each channel column: fader(18) + gap(6) + pan-knob(34) = ~58px
    // 4 OSC + 1 Master + inter-col gaps(4*8) + panel padding(2*10) = 5*58 + 32 + 20 = 342
    static constexpr float kChW = 58.0f; // per-channel column width
    static constexpr float kChGap = 8.0f;
    static constexpr float kFaderH = 90.0f;
    static constexpr float kPanSz = 34.0f;
    const float panel_w = 5.0f * kChW + 4.0f * kChGap + 24.0f; // 4 OSC + 1 Master

    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
    ImGui::BeginChild("##mixer", {panel_w, 0.0f}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    SectionHeader("MIXER");

    // Draw 4 OSC channels side by side, then Master
    for (int i = 0; i < 5; ++i) // 0-3 = OSC, 4 = Master
    {
        if (i > 0)
            ImGui::SameLine(0.0f, kChGap);

        ImGui::PushID(i);
        ImGui::BeginGroup();

        const bool is_master = (i == 4);
        float *level = is_master ? &mp.master_vol : &osc[i].level;
        float *pan = is_master ? &mp.master_pan : &osc[i].pan;

        // ── Column header label ──────────────────────────────────────────
        const char *col_lbl = is_master ? "MST" : kOscNames[i];
        float lw = ImGui::CalcTextSize(col_lbl).x;
        float col_start_x = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(col_start_x + (kChW - lw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              is_master ? ImVec4{0.80f, 0.75f, 0.50f, 1.0f}
                                        : ImVec4{0.60f, 0.65f, 0.80f, 1.0f});
        ImGui::TextUnformatted(col_lbl);
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // ── Fader centered in column ──────────────────────────────────────
        float fader_indent = col_start_x + (kChW - 18.0f) * 0.5f;
        ImGui::SetCursorPosX(fader_indent);
        char fl_id[16];
        snprintf(fl_id, sizeof(fl_id), "L##%d", i);
        Widgets::SynthFader(fl_id, level, 0.0f, 1.0f, "%.2f", 18.0f, kFaderH);

        // ── Pan knob centered in column ───────────────────────────────────
        float knob_indent = col_start_x + (kChW - kPanSz) * 0.5f;
        ImGui::SetCursorPosX(knob_indent);
        char fp_id[16];
        snprintf(fp_id, sizeof(fp_id), "P##%d", i);
        Widgets::SynthKnob(fp_id, pan, -1.0f, 1.0f, "%.2f", kPanSz);

        ImGui::EndGroup();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ─── Delay panel ─────────────────────────────────────────────────────────────
static void DrawDelayPanel(MasterParams &mp)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
    ImGui::BeginChild("##delay", {220.0f, 0.0f}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    SectionHeader("STEREO DELAY");

    float *prms[] = {&mp.delay_l_time, &mp.delay_r_time,
                     &mp.delay_fb, &mp.delay_mix, &mp.delay_damping};
    const char *lbls[] = {"L Time", "R Time", "Fdbk", "Mix", "Damp"};
    const float mn[] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    const float mx[] = {2.0f, 2.0f, 0.99f, 1.0f, 1.0f};
    const char *fmts[] = {"%.2fs", "%.2fs", "%.0f%%", "%.0f%%", "%.0f%%"};
    // Adjust % display
    for (int i = 0; i < 5; ++i)
    {
        if (i > 1)
        {
            char lbl_buf[32];
            snprintf(lbl_buf, sizeof(lbl_buf), "%s", lbls[i]);
        }
    }

    KnobRow("dly", prms, lbls, mn, mx, fmts, 5, 40.0f);

    ImGui::EndChild();
}

// ─── Tube panel ──────────────────────────────────────────────────────────────
static void DrawTubePanel(MasterParams &mp)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
    ImGui::BeginChild("##tube", {160.0f, 0.0f}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    SectionHeader("TUBE SAT");

    float *prms[] = {&mp.tube_drive, &mp.tube_asym, &mp.tube_mix};
    const char *lbls[] = {"Drive", "Asym", "Mix"};
    const float mn[] = {0.0f, -1.0f, 0.0f};
    const float mx[] = {1.0f, 1.0f, 1.0f};
    const char *fmts[] = {"%.2f", "%.2f", "%.2f"};
    KnobRow("tube", prms, lbls, mn, mx, fmts, 3, 40.0f);

    ImGui::EndChild();
}

// ─── MFX panel ───────────────────────────────────────────────────────────────
static void DrawMFXPanel(MasterParams &mp)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {0.055f, 0.059f, 0.086f, 1.0f});
    ImGui::BeginChild("##mfx", {160.0f, 0.0f}, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    SectionHeader("MFX");

    float *prms[] = {&mp.mfx_p1, &mp.mfx_p2, &mp.mfx_p3};
    const char *lbls[] = {"P1", "P2", "P3"};
    const float mn[] = {0.0f, 0.0f, 0.0f};
    const float mx[] = {1.0f, 1.0f, 1.0f};
    const char *fmts[] = {"%.2f", "%.2f", "%.2f"};
    KnobRow("mfx", prms, lbls, mn, mx, fmts, 3, 40.0f);

    ImGui::EndChild();
}

// ─── SynthUIState::Init ───────────────────────────────────────────────────────
void SynthUIState::Init()
{
    for (int i = 0; i < 4; ++i)
    {
        filter_adsr[i] = {osc[i].f_attack, osc[i].f_decay,
                          osc[i].f_sustain, osc[i].f_release};
        amp_adsr[i] = {osc[i].a_attack, osc[i].a_decay,
                       osc[i].a_sustain, osc[i].a_release};
    }
}

// ─── DrawSynthUI ─────────────────────────────────────────────────────────────
void DrawSynthUI(SynthUIState &state)
{
    ImGuiIO &io = ImGui::GetIO();

    // Full-screen backdrop window
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f, 8.0f});
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;

    // ─── Row 1: OSC panels (4 equal columns) ─────────────────────────────
    float osc_row_h = avail_h * 0.60f;
    float osc_col_w = (avail_w - 30.0f) / 4.0f;

    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            ImGui::SameLine(0.0f, 6.0f);
        ImGui::PushID(i);
        ImGui::SetNextWindowSizeConstraints({osc_col_w, osc_row_h},
                                            {osc_col_w, osc_row_h});

        // Child size is set by next child
        float child_w = osc_col_w;
        float child_h = osc_row_h;

        ImVec2 saved = ImGui::GetCursorPos();
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

        // Size the osc child to fill
        ImVec2 child_sz = {child_w, child_h};

        // The DrawOscPanel uses BeginChild with 0,0 which fills — constrain cursor
        ImGui::SetNextWindowSizeConstraints(child_sz, child_sz);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f, 8.0f});
        float prev_x = ImGui::GetCursorPosX();
        float prev_y = ImGui::GetCursorPosY();

        // We need to size the child explicitly since DrawOscPanel uses 0,0
        // Override: use a wrapper child
        ImGui::BeginChild(ImGui::GetID("oscwrap"), child_sz, ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar(); // WindowPadding
        DrawOscPanel(i, state.osc[i], state.filter_adsr[i], state.amp_adsr[i]);
        ImGui::EndChild();

        ImGui::PopStyleVar(); // ChildRounding
        ImGui::EndGroup();
        ImGui::PopID();
    }

    // ─── Row 2: Bottom bar ────────────────────────────────────────────────
    float remaining_h = avail_h - osc_row_h - 12.0f;
    ImGui::Spacing();

    // Mixer
    ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {999.0f, remaining_h});
    DrawMixerPanel(state.osc, state.master);
    ImGui::SameLine(0.0f, 6.0f);

    // Delay
    ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {999.0f, remaining_h});
    DrawDelayPanel(state.master);
    ImGui::SameLine(0.0f, 6.0f);

    // Tube
    ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {999.0f, remaining_h});
    DrawTubePanel(state.master);
    ImGui::SameLine(0.0f, 6.0f);

    // MFX
    ImGui::SetNextWindowSizeConstraints({0.0f, remaining_h}, {999.0f, remaining_h});
    DrawMFXPanel(state.master);
    ImGui::SameLine(0.0f, 6.0f);

    // Mod Matrix button + optional panel
    ImGui::BeginGroup();
    {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              state.show_mod_matrix ? Accent() : ImVec4{0.118f, 0.133f, 0.200f, 1.0f});
        if (ImGui::Button("MOD MATRIX", {100.0f, 28.0f}))
            state.show_mod_matrix = !state.show_mod_matrix;
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.118f, 0.133f, 0.200f, 1.0f});
        if (ImGui::Button("ImGui Demo", {100.0f, 22.0f}))
            state.show_imgui_demo = !state.show_imgui_demo;
        ImGui::PopStyleColor();
    }
    ImGui::EndGroup();

    ImGui::End(); // ##root

    // ─── Mod Matrix popup window ──────────────────────────────────────────
    if (state.show_mod_matrix)
    {
        ImGui::SetNextWindowSize({700.0f, 320.0f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Modulation Matrix", &state.show_mod_matrix))
            Widgets::ModMatrix("##mm", &state.mod_matrix);
        ImGui::End();
    }

    if (state.show_imgui_demo)
        ImGui::ShowDemoWindow(&state.show_imgui_demo);
}
