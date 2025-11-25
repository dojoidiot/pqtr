// linkeditor.cpp - Link editor implementation

#include "linkeditor.hpp"
#include "files.hpp"
#include "imgui.h"

namespace desk {

// Helper to render a dial slider
static bool render_dial(const char* label, float* value, float min = 0.0f, float max = 1.0f) {
    ImGui::PushItemWidth(-1);
    bool changed = ImGui::SliderFloat(label, value, min, max, "%.2f");
    ImGui::PopItemWidth();
    return changed;
}


bool render_link_editor(State& state) {
    bool changed = false;

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Link Editor");
    ImGui::Separator();

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

    ImGui::Text("Link: %s", link.name.c_str());
    ImGui::Separator();

    // Module 1: Geometric (6 dials)
    if (ImGui::CollapsingHeader("Geometric", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Crop");
        if (render_dial("Top##crop", &link.geometric.dials["crop_top"])) changed = true;
        if (render_dial("Right##crop", &link.geometric.dials["crop_right"])) changed = true;
        if (render_dial("Bottom##crop", &link.geometric.dials["crop_bottom"])) changed = true;
        if (render_dial("Left##crop", &link.geometric.dials["crop_left"])) changed = true;

        ImGui::Spacing();
        ImGui::Text("Transform");
        if (render_dial("Scale##zoom", &link.geometric.dials["scale"])) changed = true;
        if (render_dial("Tilt##rot", &link.geometric.dials["tilt_angle"])) changed = true;

        ImGui::Unindent();
    }

    // Module 2: Color Correction (3 dials)
    if (ImGui::CollapsingHeader("Color Correction", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("White Balance");
        if (render_dial("Temperature", &link.color_correction.dials["temperature"])) changed = true;
        if (render_dial("Tint", &link.color_correction.dials["tint"])) changed = true;

        ImGui::Spacing();
        if (render_dial("Exposure", &link.color_correction.dials["exposure"])) changed = true;

        ImGui::Unindent();
    }

    // Module 3: Tone Mapping (5 dials)
    if (ImGui::CollapsingHeader("Tone Mapping", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        if (render_dial("Contrast", &link.tone_mapping.dials["contrast"])) changed = true;

        ImGui::Spacing();
        ImGui::Text("Curve");
        if (render_dial("Highlights", &link.tone_mapping.dials["highlights"])) changed = true;
        if (render_dial("Shadows", &link.tone_mapping.dials["shadows"])) changed = true;

        ImGui::Spacing();
        ImGui::Text("Clipping");
        if (render_dial("Black Point", &link.tone_mapping.dials["black"])) changed = true;
        if (render_dial("White Point", &link.tone_mapping.dials["white"])) changed = true;

        ImGui::Unindent();
    }

    // Module 4: Global Color (3 dials)
    if (ImGui::CollapsingHeader("Global Color")) {
        ImGui::Indent();

        if (render_dial("Vibrance", &link.global_color.dials["vibrance"])) changed = true;
        if (render_dial("Saturation", &link.global_color.dials["saturation"])) changed = true;
        if (render_dial("Color Density", &link.global_color.dials["color_density"])) changed = true;

        ImGui::Unindent();
    }

    // Module 5: Selective Color (24 dials - 8 colors x 3)
    if (ImGui::CollapsingHeader("Selective Color")) {
        ImGui::Indent();

        const char* colors[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Magenta"};
        const char* color_keys[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};

        for (int c = 0; c < 8; c++) {
            if (ImGui::TreeNode(colors[c])) {
                std::string hue_key = std::string(color_keys[c]) + "_hue";
                std::string sat_key = std::string(color_keys[c]) + "_saturation";
                std::string lum_key = std::string(color_keys[c]) + "_luminance";

                char label[32];
                snprintf(label, sizeof(label), "Hue##%s", color_keys[c]);
                if (render_dial(label, &link.selective_color.dials[hue_key])) changed = true;

                snprintf(label, sizeof(label), "Saturation##%s", color_keys[c]);
                if (render_dial(label, &link.selective_color.dials[sat_key])) changed = true;

                snprintf(label, sizeof(label), "Luminance##%s", color_keys[c]);
                if (render_dial(label, &link.selective_color.dials[lum_key])) changed = true;

                ImGui::TreePop();
            }
        }

        ImGui::Unindent();
    }

    // Module 6: Detail (4 dials)
    if (ImGui::CollapsingHeader("Detail")) {
        ImGui::Indent();

        ImGui::Text("Sharpen");
        if (render_dial("Amount##sharp", &link.detail.dials["sharpen_amount"])) changed = true;
        if (render_dial("Radius##sharp", &link.detail.dials["sharpen_radius"])) changed = true;

        ImGui::Spacing();
        ImGui::Text("Denoise");
        if (render_dial("Luminance##denoise", &link.detail.dials["denoise_luminance"])) changed = true;
        if (render_dial("Chroma##denoise", &link.detail.dials["denoise_chroma"])) changed = true;

        ImGui::Unindent();
    }

    // Auto-save on change
    if (changed) {
        save_pipe_json(proj);
        state.needs_reprocess = true;
    }

    return changed;
}

} // namespace desk
