// workarea.cpp - Work area implementation

#include "workarea.hpp"
#include "imgui.h"

namespace desk {

void render_work_area(State& state) {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Image");
    ImGui::Separator();

    if (!state.root_folder_set) {
        ImGui::TextWrapped("Select a root folder to begin.");
        return;
    }

    if (state.selected_project < 0 || state.selected_project >= (int)state.projects.size()) {
        ImGui::TextWrapped("Select a project to view its output.");
        return;
    }

    const Project& proj = state.projects[state.selected_project];

    if (!state.texture_loaded) {
        // Show status
        if (!state.error_message.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", state.error_message.c_str());
        } else {
            ImGui::TextWrapped("No output image available.\n\nProcess the RAW file to generate output.");
        }
        return;
    }

    // Get available space
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Calculate image size maintaining aspect ratio
    float img_aspect = (float)state.texture_width / (float)state.texture_height;
    float avail_aspect = avail.x / avail.y;

    float display_w, display_h;
    if (img_aspect > avail_aspect) {
        // Image is wider than available space
        display_w = avail.x;
        display_h = avail.x / img_aspect;
    } else {
        // Image is taller than available space
        display_h = avail.y;
        display_w = avail.y * img_aspect;
    }

    // Center the image
    float offset_x = (avail.x - display_w) * 0.5f;
    float offset_y = (avail.y - display_h) * 0.5f;

    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x + offset_x, cursor.y + offset_y));

    // Display the image
    ImGui::Image((ImTextureID)(intptr_t)state.texture_id, ImVec2(display_w, display_h));

    // Show image info on hover
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", proj.name.c_str());
        ImGui::Text("%d x %d", state.texture_width, state.texture_height);
        ImGui::EndTooltip();
    }
}

} // namespace desk
