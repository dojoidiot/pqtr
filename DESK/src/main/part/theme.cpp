// theme.cpp - UI theme implementation

#include "theme.hpp"
#include "imgui.h"

namespace desk {

void apply_theme() {
    auto& style = ImGui::GetStyle();

    // Rounding
    style.ChildRounding = 0;
    style.GrabRounding = 0;
    style.FrameRounding = 2;
    style.PopupRounding = 0;
    style.ScrollbarRounding = 0;
    style.TabRounding = 2;
    style.WindowRounding = 4;

    // Padding and spacing
    style.FramePadding = {2, 1};
    style.WindowTitleAlign = {0.0f, 0.5f};

    // Colors (Modern Dark Theme)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = {1.0f, 1.0f, 1.0f, 1.00f};
    colors[ImGuiCol_TextDisabled] = {0.25f, 0.25f, 0.25f, 1.00f};
    colors[ImGuiCol_WindowBg] = {0.09f, 0.09f, 0.09f, 0.94f};
    colors[ImGuiCol_ChildBg] = {0.11f, 0.11f, 0.11f, 1.00f};
    colors[ImGuiCol_PopupBg] = {0.11f, 0.11f, 0.11f, 0.94f};
    colors[ImGuiCol_Border] = {0.07f, 0.08f, 0.08f, 1.00f};
    colors[ImGuiCol_BorderShadow] = {0.00f, 0.00f, 0.00f, 0.00f};
    colors[ImGuiCol_FrameBg] = {0.35f, 0.35f, 0.35f, 0.54f};
    colors[ImGuiCol_FrameBgHovered] = {0.31f, 0.29f, 0.27f, 1.00f};
    colors[ImGuiCol_FrameBgActive] = {0.40f, 0.36f, 0.33f, 0.67f};
    colors[ImGuiCol_TitleBg] = {0.1f, 0.1f, 0.1f, 1.00f};
    colors[ImGuiCol_TitleBgActive] = {0.2f, 0.2f, 0.2f, 1.00f};
    colors[ImGuiCol_TitleBgCollapsed] = {0.0f, 0.0f, 0.0f, 0.61f};
    colors[ImGuiCol_MenuBarBg] = {0.18f, 0.18f, 0.18f, 0.94f};
    colors[ImGuiCol_ScrollbarBg] = {0.00f, 0.00f, 0.00f, 0.16f};
    colors[ImGuiCol_ScrollbarGrab] = {0.24f, 0.22f, 0.21f, 1.00f};
    colors[ImGuiCol_ScrollbarGrabHovered] = {0.31f, 0.29f, 0.27f, 1.00f};
    colors[ImGuiCol_ScrollbarGrabActive] = {0.40f, 0.36f, 0.33f, 1.00f};
    colors[ImGuiCol_CheckMark] = {0.84f, 0.84f, 0.84f, 1.0f};
    colors[ImGuiCol_SliderGrab] = {0.8f, 0.8f, 0.8f, 1.0f};
    colors[ImGuiCol_SliderGrabActive] = {0.55f, 0.55f, 0.55f, 1.00f};
    colors[ImGuiCol_Button] = {0.35f, 0.35f, 0.35f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = {0.45f, 0.45f, 0.45f, 1.0f};
    colors[ImGuiCol_ButtonActive] = {0.25f, 0.25f, 0.25f, 1.0f};
    colors[ImGuiCol_Header] = {0.84f, 0.36f, 0.05f, 0.0f};
    colors[ImGuiCol_HeaderHovered] = {0.25f, 0.25f, 0.25f, 0.80f};
    colors[ImGuiCol_HeaderActive] = {0.42f, 0.42f, 0.42f, 1.00f};
    colors[ImGuiCol_Separator] = {0.35f, 0.35f, 0.35f, 0.50f};
    colors[ImGuiCol_SeparatorHovered] = {0.31f, 0.29f, 0.27f, 0.78f};
    colors[ImGuiCol_SeparatorActive] = {0.40f, 0.36f, 0.33f, 1.00f};
    colors[ImGuiCol_ResizeGrip] = {1.0f, 1.0f, 1.0f, 0.25f};
    colors[ImGuiCol_ResizeGripHovered] = {1.00f, 1.0f, 1.0f, 0.4f};
    colors[ImGuiCol_ResizeGripActive] = {1.00f, 1.00f, 1.0f, 0.95f};
    colors[ImGuiCol_Tab] = {0.18f, 0.18f, 0.18f, 1.0f};
    colors[ImGuiCol_TabHovered] = {0.58f, 0.58f, 0.58f, 0.80f};
    colors[ImGuiCol_TabActive] = {0.6f, 0.60f, 0.60f, 1.00f};
    colors[ImGuiCol_TabUnfocused] = {0.07f, 0.10f, 0.15f, 0.97f};
    colors[ImGuiCol_TabUnfocusedActive] = {0.14f, 0.26f, 0.42f, 1.00f};
    colors[ImGuiCol_PlotLines] = {0.66f, 0.60f, 0.52f, 1.00f};
    colors[ImGuiCol_PlotLinesHovered] = {0.98f, 0.29f, 0.20f, 1.00f};
    colors[ImGuiCol_PlotHistogram] = {0.60f, 0.59f, 0.10f, 1.00f};
    colors[ImGuiCol_PlotHistogramHovered] = {0.72f, 0.73f, 0.15f, 1.00f};
    colors[ImGuiCol_TextSelectedBg] = {0.27f, 0.52f, 0.53f, 0.35f};
    colors[ImGuiCol_DragDropTarget] = {0.60f, 0.59f, 0.10f, 0.90f};
    colors[ImGuiCol_NavHighlight] = {0.51f, 0.65f, 0.60f, 1.00f};
    colors[ImGuiCol_NavWindowingHighlight] = {1.00f, 1.00f, 1.00f, 0.70f};
    colors[ImGuiCol_NavWindowingDimBg] = {0.80f, 0.80f, 0.80f, 0.20f};
    colors[ImGuiCol_ModalWindowDimBg] = {0.11f, 0.13f, 0.13f, 0.35f};
}

} // namespace desk
