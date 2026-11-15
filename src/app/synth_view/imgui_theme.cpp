#include "imgui_theme.hpp"
#include "imgui.h"

void ApplySynthViewTheme()
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);

    // ─── Layout ───────────────────────────────────────────────────────────
    style.WindowPadding = {10.0f, 10.0f};
    style.FramePadding = {6.0f, 4.0f};
    style.ItemSpacing = {8.0f, 6.0f};
    style.ItemInnerSpacing = {4.0f, 4.0f};
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    // ─── Colors ───────────────────────────────────────────────────────────
    ImVec4 *c = style.Colors;

    // Background palette
    // #0D0D12  bg window
    // #131318  child bg
    // #1A1A24  frame bg
    // #1E1E2A  frame bg hover
    // #4488FF  accent
    // #2A5ACC  accent dimmed
    // #E8E8F0  text
    // #6068A0  text disabled

    c[ImGuiCol_WindowBg] = {0.051f, 0.051f, 0.071f, 1.00f};
    c[ImGuiCol_ChildBg] = {0.075f, 0.075f, 0.094f, 1.00f};
    c[ImGuiCol_PopupBg] = {0.075f, 0.075f, 0.094f, 0.96f};

    c[ImGuiCol_Border] = {0.180f, 0.190f, 0.290f, 0.70f};
    c[ImGuiCol_BorderShadow] = {0.000f, 0.000f, 0.000f, 0.00f};

    c[ImGuiCol_FrameBg] = {0.102f, 0.102f, 0.141f, 1.00f};
    c[ImGuiCol_FrameBgHovered] = {0.118f, 0.125f, 0.180f, 1.00f};
    c[ImGuiCol_FrameBgActive] = {0.133f, 0.149f, 0.220f, 1.00f};

    c[ImGuiCol_TitleBg] = {0.039f, 0.039f, 0.055f, 1.00f};
    c[ImGuiCol_TitleBgActive] = {0.043f, 0.063f, 0.145f, 1.00f};
    c[ImGuiCol_TitleBgCollapsed] = {0.039f, 0.039f, 0.055f, 0.75f};

    c[ImGuiCol_MenuBarBg] = {0.039f, 0.039f, 0.055f, 1.00f};

    c[ImGuiCol_ScrollbarBg] = {0.039f, 0.039f, 0.055f, 0.80f};
    c[ImGuiCol_ScrollbarGrab] = {0.180f, 0.200f, 0.340f, 1.00f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.240f, 0.267f, 0.420f, 1.00f};
    c[ImGuiCol_ScrollbarGrabActive] = {0.267f, 0.533f, 1.000f, 1.00f};

    c[ImGuiCol_CheckMark] = {0.267f, 0.533f, 1.000f, 1.00f};
    c[ImGuiCol_SliderGrab] = {0.220f, 0.427f, 0.870f, 1.00f};
    c[ImGuiCol_SliderGrabActive] = {0.267f, 0.533f, 1.000f, 1.00f};

    c[ImGuiCol_Button] = {0.118f, 0.133f, 0.200f, 1.00f};
    c[ImGuiCol_ButtonHovered] = {0.165f, 0.255f, 0.545f, 1.00f};
    c[ImGuiCol_ButtonActive] = {0.267f, 0.533f, 1.000f, 1.00f};

    c[ImGuiCol_Header] = {0.165f, 0.188f, 0.310f, 0.80f};
    c[ImGuiCol_HeaderHovered] = {0.200f, 0.278f, 0.510f, 0.90f};
    c[ImGuiCol_HeaderActive] = {0.267f, 0.533f, 1.000f, 1.00f};

    c[ImGuiCol_Separator] = {0.180f, 0.190f, 0.290f, 0.60f};
    c[ImGuiCol_SeparatorHovered] = {0.267f, 0.533f, 1.000f, 0.70f};
    c[ImGuiCol_SeparatorActive] = {0.267f, 0.533f, 1.000f, 1.00f};

    c[ImGuiCol_ResizeGrip] = {0.267f, 0.533f, 1.000f, 0.20f};
    c[ImGuiCol_ResizeGripHovered] = {0.267f, 0.533f, 1.000f, 0.60f};
    c[ImGuiCol_ResizeGripActive] = {0.267f, 0.533f, 1.000f, 0.90f};

    c[ImGuiCol_Tab] = {0.082f, 0.094f, 0.157f, 0.86f};
    c[ImGuiCol_TabHovered] = {0.200f, 0.278f, 0.510f, 0.80f};
    c[ImGuiCol_TabSelected] = {0.165f, 0.282f, 0.604f, 1.00f};
    c[ImGuiCol_TabSelectedOverline] = {0.267f, 0.533f, 1.000f, 1.00f};
    c[ImGuiCol_TabDimmed] = {0.055f, 0.063f, 0.110f, 0.97f};
    c[ImGuiCol_TabDimmedSelected] = {0.110f, 0.165f, 0.357f, 1.00f};
    c[ImGuiCol_TabDimmedSelectedOverline] = {0.267f, 0.533f, 1.000f, 0.50f};

    c[ImGuiCol_PlotLines] = {0.400f, 0.700f, 1.000f, 1.00f};
    c[ImGuiCol_PlotLinesHovered] = {0.600f, 0.850f, 1.000f, 1.00f};
    c[ImGuiCol_PlotHistogram] = {0.267f, 0.533f, 1.000f, 1.00f};
    c[ImGuiCol_PlotHistogramHovered] = {0.400f, 0.700f, 1.000f, 1.00f};

    c[ImGuiCol_TableHeaderBg] = {0.082f, 0.090f, 0.141f, 1.00f};
    c[ImGuiCol_TableBorderStrong] = {0.180f, 0.190f, 0.290f, 1.00f};
    c[ImGuiCol_TableBorderLight] = {0.110f, 0.118f, 0.180f, 1.00f};
    c[ImGuiCol_TableRowBg] = {0.000f, 0.000f, 0.000f, 0.00f};
    c[ImGuiCol_TableRowBgAlt] = {1.000f, 1.000f, 1.000f, 0.03f};

    c[ImGuiCol_TextSelectedBg] = {0.267f, 0.533f, 1.000f, 0.35f};
    c[ImGuiCol_DragDropTarget] = {0.267f, 0.533f, 1.000f, 0.90f};

    c[ImGuiCol_NavCursor] = {0.267f, 0.533f, 1.000f, 0.80f};
    c[ImGuiCol_NavWindowingHighlight] = {1.000f, 1.000f, 1.000f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg] = {0.600f, 0.600f, 0.600f, 0.20f};
    c[ImGuiCol_ModalWindowDimBg] = {0.039f, 0.039f, 0.055f, 0.60f};

    c[ImGuiCol_Text] = {0.910f, 0.910f, 0.941f, 1.00f};
    c[ImGuiCol_TextDisabled] = {0.376f, 0.408f, 0.627f, 1.00f};
}
