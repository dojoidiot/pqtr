// linkeditor.cpp - Link editor implementation
// Two-level menu: Master (modules) → Detail (dials) + Dial Control area

#include "linkeditor.hpp"
#include "files.hpp"
#include "imgui.h"

namespace desk {

// Module names for master menu (6 golden modules - no geometric)
static const char* MODULE_NAMES[] = {
    "EXP", "WB", "TONE", "COLOR", "SEL", "DTL"
};

// Detail dial names per module
static const char* DIAL_NAMES_EXP[] = {"value"};
static const char* DIAL_NAMES_WB[] = {"temp", "tint"};
static const char* DIAL_NAMES_TONE[] = {"contrast", "highlights", "shadows", "black", "white"};
static const char* DIAL_NAMES_COLOR[] = {"vibrance", "saturation", "density"};
static const char* DIAL_NAMES_SEL[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
static const char* DIAL_NAMES_DTL[] = {"sharp_amt", "sharp_rad", "denoise_lum", "denoise_chr"};

static const char** DIAL_NAMES[] = {
    DIAL_NAMES_EXP,
    DIAL_NAMES_WB,
    DIAL_NAMES_TONE,
    DIAL_NAMES_COLOR,
    DIAL_NAMES_SEL,
    DIAL_NAMES_DTL
};

static const int DIAL_COUNTS[] = {1, 2, 5, 3, 8, 4};

// Dial control width (square, full height)
static const float DIAL_CONTROL_WIDTH = 120.0f;

// Box styling
static const ImVec4 BOX_NORMAL = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
static const ImVec4 BOX_HOVER = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
static const ImVec4 BOX_SELECTED = ImVec4(0.3f, 0.5f, 0.7f, 1.0f);

// Render a selectable box, returns true if clicked
static bool render_box(const char* label, bool selected, float width = 0.0f) {
    ImVec4 color = selected ? BOX_SELECTED : BOX_NORMAL;

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? BOX_SELECTED : BOX_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, BOX_SELECTED);

    bool clicked = false;
    if (width > 0.0f) {
        clicked = ImGui::Button(label, ImVec2(width, 0));
    } else {
        clicked = ImGui::Button(label);
    }

    ImGui::PopStyleColor(3);
    return clicked;
}

bool render_link_editor(State& state) {
    bool changed = false;

    // Check if we have a valid selection
    if (state.selected_project < 0 || state.selected_project >= (int)state.projects.size()) {
        ImGui::TextWrapped("Select a project to edit.");
        return false;
    }

    Project& proj = state.projects[state.selected_project];

    if (state.selected_link < 0 || state.selected_link >= (int)proj.links.size()) {
        ImGui::TextWrapped("Select a Link to edit its modules and dials.");
        return false;
    }

    Link& link = proj.links[state.selected_link];

    // Get available space
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float menu_width = avail.x - DIAL_CONTROL_WIDTH - ImGui::GetStyle().ItemSpacing.x;

    // === LEFT SIDE: Menus ===
    ImGui::BeginChild("##menus", ImVec2(menu_width, avail.y), false);

    // Link name header
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Link: %s", link.name.c_str());
    ImGui::Separator();

    // Master menu (7 module boxes)
    ImGui::Text("Module:");
    for (int m = 0; m < MOD_COUNT; m++) {
        if (m > 0) ImGui::SameLine();
        if (render_box(MODULE_NAMES[m], state.selected_module == m)) {
            state.selected_module = m;
            state.selected_dial = 0;  // Reset dial selection
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Detail menu (dial boxes for selected module)
    ImGui::Text("Dial:");
    int dial_count = DIAL_COUNTS[state.selected_module];
    const char** dial_names = DIAL_NAMES[state.selected_module];

    for (int d = 0; d < dial_count; d++) {
        if (d > 0) ImGui::SameLine();
        if (render_box(dial_names[d], state.selected_dial == d)) {
            state.selected_dial = d;
        }
    }

    // For Selective Color, show sub-dials (hue/sat/lum) when a color is selected
    if (state.selected_module == MOD_SEL) {
        ImGui::Spacing();
        ImGui::Text("Adjust:");
        static int sel_sub = 0;  // 0=hue, 1=sat, 2=lum
        const char* sub_names[] = {"hue", "sat", "lum"};
        for (int s = 0; s < 3; s++) {
            if (s > 0) ImGui::SameLine();
            if (render_box(sub_names[s], sel_sub == s)) {
                sel_sub = s;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Future content area
    ImGui::TextDisabled("(dial value display area)");

    ImGui::EndChild();

    // === RIGHT SIDE: Dial Control ===
    ImGui::SameLine();
    ImGui::BeginChild("##dial_control", ImVec2(DIAL_CONTROL_WIDTH, avail.y), true);

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DIAL");
    ImGui::Separator();

    // Placeholder for dial control widget
    ImVec2 dial_avail = ImGui::GetContentRegionAvail();
    float dial_size = dial_avail.x - 10.0f;

    // Draw placeholder square
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRect(cursor, ImVec2(cursor.x + dial_size, cursor.y + dial_size),
                  IM_COL32(100, 100, 100, 255), 4.0f);

    // Show current dial name in center
    const char* current_dial = DIAL_NAMES[state.selected_module][state.selected_dial];
    ImVec2 text_size = ImGui::CalcTextSize(current_dial);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (dial_size - text_size.x) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dial_size * 0.5f - text_size.y * 0.5f);
    ImGui::Text("%s", current_dial);

    ImGui::EndChild();

    return changed;
}

} // namespace desk
