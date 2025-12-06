// workarea.cpp - Work area implementation

#include "workarea.hpp"
#include "imgui.h"
#include <cstdint>

namespace desk {

void render_work_area(State& state) {
    if (!state.project_folder_set) {
        ImGui::TextWrapped("Select a root folder to begin.");
        return;
    }

    if (!state.has_project()) {
        ImGui::TextWrapped("Select a project to view its output.");
        return;
    }

    const Project& proj = state.current_project();

    // Show loading message while processing
    if (state.is_working) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("Loading...");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - text_size.x) * 0.5f,
            (avail.y - text_size.y) * 0.5f
        ));
        ImGui::TextDisabled("Loading...");
        return;
    }

    // During tuning, keep showing current view (don't switch)
    // Work area stays unchanged until tune completes

    if (!state.texture.loaded) {
        if (!state.error_message.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", state.error_message.c_str());
        } else {
            ImGui::TextWrapped("No output image available.\n\nProcess the RAW file to generate output.");
        }
        return;
    }

    // Get available space
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Sanity check - ensure we have valid space
    if (avail.x <= 0 || avail.y <= 0) {
        return;
    }

    // Calculate image size maintaining aspect ratio, fitting within available space
    float img_w = static_cast<float>(state.texture.width);
    float img_h = static_cast<float>(state.texture.height);

    if (img_w <= 0 || img_h <= 0) {
        return;
    }

    float scale_x = avail.x / img_w;
    float scale_y = avail.y / img_h;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    float display_w = img_w * scale;
    float display_h = img_h * scale;

    // Center the image
    float offset_x = (avail.x - display_w) * 0.5f;
    float offset_y = (avail.y - display_h) * 0.5f;

    ImVec2 cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(cursor.x + offset_x, cursor.y + offset_y));

    // Display the image
    ImGui::Image((ImTextureID)(intptr_t)state.texture.id, ImVec2(display_w, display_h));

    // Show image info on hover
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", proj.name.c_str());
        ImGui::Text("%d x %d", state.texture.width, state.texture.height);
        ImGui::EndTooltip();
    }
}

} // namespace desk
