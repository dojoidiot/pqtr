// projects.cpp - Workspace and Pipe panel implementation

#include "projects.hpp"
#include "files.hpp"
#include "imgui.h"
#include <cstring>

namespace desk {

// ============================================================
// Workspace Panel - Simple list of RAW files
// ============================================================

bool render_workspace_panel(State& state) {
    bool selection_changed = false;

    // Add RAW button
    if (ImGui::SmallButton("+##add_raw")) {
        if (state.project_folder_set) {
            open_raw_file_dialog(state);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add RAW file");
    }

    ImGui::Separator();

    if (!state.project_folder_set) {
        ImGui::TextWrapped("No folder set.\n\nClick 'Open Folder' to select a workspace.");
        return false;
    }

    if (state.projects.empty()) {
        ImGui::TextWrapped("No RAW files.\n\nClick + to add a RAW file.");
        return false;
    }

    // Simple selectable list of RAW files
    for (int p = 0; p < static_cast<int>(state.projects.size()); p++) {
        Project& proj = state.projects[p];

        ImGui::PushID(p);

        bool is_selected = (state.selection.project == p);

        if (ImGui::Selectable(proj.name.c_str(), is_selected)) {
            if (state.selection.project != p) {
                state.selection.project = p;
                state.selection.link = -1;
                selection_changed = true;
                // Auto-open pipe panel when selecting a RAW
                state.panels.pipe = true;
            }
        }

        // Context menu for hiding
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Hide")) {
                proj.hidden = true;
                save_desk_json(proj);
                if (state.selection.project == p) {
                    state.selection.project = -1;
                    state.selection.link = -1;
                    selection_changed = true;
                }
                state.projects.erase(state.projects.begin() + p);
                p--;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    return selection_changed;
}

// ============================================================
// Pipe Panel - Tree of Links/Modules/Dials
// ============================================================

// Module dial definitions for tree display
struct DialDef {
    const char* name;
    const char* key;
};

static const DialDef GEOMETRIC_DIALS[] = {
    {"Crop Top", "crop_top"},
    {"Crop Right", "crop_right"},
    {"Crop Bottom", "crop_bottom"},
    {"Crop Left", "crop_left"},
    {"Scale", "scale"},
    {"Tilt", "tilt_angle"},
};

static const DialDef COLOR_CORRECTION_DIALS[] = {
    {"Exposure", "exposure"},
    {"Temperature", "temperature"},
    {"Tint", "tint"},
};

static const DialDef TONE_MAPPING_DIALS[] = {
    {"Contrast", "contrast"},
    {"Highlights", "highlights"},
    {"Shadows", "shadows"},
    {"Black", "black"},
    {"White", "white"},
};

static const DialDef GLOBAL_COLOR_DIALS[] = {
    {"Vibrance", "vibrance"},
    {"Saturation", "saturation"},
    {"Density", "color_density"},
};

static const DialDef DETAIL_DIALS[] = {
    {"Sharp Amt", "sharpen_amount"},
    {"Sharp Rad", "sharpen_radius"},
    {"Denoise L", "denoise_luminance"},
    {"Denoise C", "denoise_chroma"},
};

// Check if dial differs from default (0.5 for most, special cases handled)
static bool dial_is_set(const char* key, float value) {
    // Geometric: crop defaults to 0, scale/tilt to 0.5
    if (strcmp(key, "crop_top") == 0 || strcmp(key, "crop_right") == 0 ||
        strcmp(key, "crop_bottom") == 0 || strcmp(key, "crop_left") == 0)
        return value != 0.0f;
    // Tone mapping: black/white special defaults
    if (strcmp(key, "black") == 0) return value != 0.15f;
    if (strcmp(key, "white") == 0) return value != 0.85f;
    // Detail: most default to 0, sharpen_radius to 0.4
    if (strcmp(key, "sharpen_amount") == 0 || strcmp(key, "denoise_luminance") == 0 ||
        strcmp(key, "denoise_chroma") == 0)
        return value != 0.0f;
    if (strcmp(key, "sharpen_radius") == 0) return value != 0.4f;
    // Default is 0.5
    return value != 0.5f;
}

// Render dials for a module, returns true if any dial was clicked
static bool render_module_dials(const Module& mod, const DialDef* dials, int count,
                                State& state, int link_idx, int mod_idx) {
    bool changed = false;

    for (int d = 0; d < count; d++) {
        auto it = mod.dials.find(dials[d].key);
        if (it == mod.dials.end()) continue;

        float value = it->second;
        bool is_set = dial_is_set(dials[d].key, value);

        ImGui::PushID(d);

        // Only show dials that are set (non-default)
        if (is_set) {
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_NoTreePushOnOpen;

            // Highlight if this dial is hot
            if (state.selection.link == link_idx &&
                state.selection.is_hot(mod_idx, d)) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            char label[64];
            snprintf(label, sizeof(label), "%s: %.2f", dials[d].name, value);
            ImGui::TreeNodeEx(label, flags);

            // Clicking tree leaf makes dial hot
            if (ImGui::IsItemClicked()) {
                state.selection.link = link_idx;
                state.selection.set_hot(mod_idx, d);  // Make hot
                state.panels.link_editor = true;
                changed = true;
            }
        }

        ImGui::PopID();
    }

    return changed;
}

// Render selective color module (8 colors x 3 dials)
static bool render_selective_color(const Module& mod, State& state, int link_idx) {
    bool changed = false;
    const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
    const char* color_names[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Magenta"};

    for (int c = 0; c < 8; c++) {
        // Check if any dial for this color is set
        std::string hue_key = std::string(colors[c]) + "_hue";
        std::string sat_key = std::string(colors[c]) + "_saturation";
        std::string lum_key = std::string(colors[c]) + "_luminance";

        auto hue_it = mod.dials.find(hue_key);
        auto sat_it = mod.dials.find(sat_key);
        auto lum_it = mod.dials.find(lum_key);

        bool has_hue = hue_it != mod.dials.end() && hue_it->second != 0.5f;
        bool has_sat = sat_it != mod.dials.end() && sat_it->second != 0.5f;
        bool has_lum = lum_it != mod.dials.end() && lum_it->second != 0.5f;

        if (!has_hue && !has_sat && !has_lum) continue;

        ImGui::PushID(c);

        if (ImGui::TreeNode(color_names[c])) {
            if (has_hue) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (state.selection.link == link_idx &&
                    state.selection.is_hot(MOD_SELECTIVE_COLOR, c, 0)) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                char label[32];
                snprintf(label, sizeof(label), "Hue: %.2f", hue_it->second);
                ImGui::TreeNodeEx(label, flags);
                if (ImGui::IsItemClicked()) {
                    state.selection.link = link_idx;
                    state.selection.set_hot(MOD_SELECTIVE_COLOR, c, 0);
                    state.panels.link_editor = true;
                    changed = true;
                }
            }
            if (has_sat) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (state.selection.link == link_idx &&
                    state.selection.is_hot(MOD_SELECTIVE_COLOR, c, 1)) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                char label[32];
                snprintf(label, sizeof(label), "Sat: %.2f", sat_it->second);
                ImGui::TreeNodeEx(label, flags);
                if (ImGui::IsItemClicked()) {
                    state.selection.link = link_idx;
                    state.selection.set_hot(MOD_SELECTIVE_COLOR, c, 1);
                    state.panels.link_editor = true;
                    changed = true;
                }
            }
            if (has_lum) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (state.selection.link == link_idx &&
                    state.selection.is_hot(MOD_SELECTIVE_COLOR, c, 2)) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }
                char label[32];
                snprintf(label, sizeof(label), "Lum: %.2f", lum_it->second);
                ImGui::TreeNodeEx(label, flags);
                if (ImGui::IsItemClicked()) {
                    state.selection.link = link_idx;
                    state.selection.set_hot(MOD_SELECTIVE_COLOR, c, 2);
                    state.panels.link_editor = true;
                    changed = true;
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    return changed;
}

// Check if a module has any non-default dials
static bool module_has_settings(const Module& mod) {
    for (const auto& [key, value] : mod.dials) {
        if (dial_is_set(key.c_str(), value)) return true;
    }
    return false;
}

bool render_pipe_panel(State& state) {
    bool selection_changed = false;

    if (!state.has_project()) {
        ImGui::TextDisabled("Select a RAW file");
        return false;
    }

    Project& proj = state.current_project();

    // Add link button
    if (ImGui::SmallButton("+##add_link")) {
        proj.links.push_back(Link("Link " + std::to_string(proj.links.size() + 1)));
        save_pipe_json(proj);
        state.needs_reprocess = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add Link");
    }

    ImGui::Separator();

    if (proj.links.empty()) {
        ImGui::TextWrapped("No links.\n\nClick + to add a processing link.");
        return false;
    }

    // Tree of links
    for (int l = 0; l < static_cast<int>(proj.links.size()); l++) {
        Link& link = proj.links[l];

        ImGui::PushID(l);

        ImGuiTreeNodeFlags link_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                        ImGuiTreeNodeFlags_DefaultOpen;
        if (state.selection.link == l) {
            link_flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool link_open = ImGui::TreeNodeEx(link.name.c_str(), link_flags);

        // Select link on click - clear hot on link switch
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            if (state.selection.link != l) {
                state.selection.link = l;
                state.selection.clear_hot();  // Hot clears on link switch
                selection_changed = true;
            }
            state.panels.link_editor = true;
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                link.editing_name = true;
            }
            if (ImGui::MenuItem("Remove")) {
                proj.links.erase(proj.links.begin() + l);
                save_pipe_json(proj);
                if (state.selection.link == l) {
                    state.selection.link = -1;
                }
                state.needs_reprocess = true;
                ImGui::EndPopup();
                ImGui::PopID();
                if (link_open) ImGui::TreePop();
                continue;
            }
            ImGui::EndPopup();
        }

        // Inline rename
        if (link.editing_name) {
            static char name_buf[64];
            strncpy(name_buf, link.name.c_str(), sizeof(name_buf) - 1);
            ImGui::SetNextItemWidth(120);
            if (ImGui::InputText("##rename", name_buf, sizeof(name_buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                link.name = name_buf;
                link.editing_name = false;
                save_pipe_json(proj);
            }
            if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
                link.editing_name = false;
            }
        }

        if (link_open) {
            // Geometric
            if (module_has_settings(link.geometric)) {
                bool geo_open = ImGui::TreeNode("Geometric");
                // Click on module name navigates (shows dials) but doesn't make hot
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    state.selection.link = l;
                    state.selection.module = MOD_GEOMETRIC;
                    state.selection.dial = 0;
                    state.panels.link_editor = true;
                    selection_changed = true;
                }
                if (geo_open) {
                    render_module_dials(link.geometric, GEOMETRIC_DIALS, 6, state, l, MOD_GEOMETRIC);
                    ImGui::TreePop();
                }
            }

            // Color Correction
            if (module_has_settings(link.color_correction)) {
                bool cc_open = ImGui::TreeNode("Color Correction");
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    state.selection.link = l;
                    state.selection.module = MOD_COLOR_CORRECTION;
                    state.selection.dial = 0;
                    state.panels.link_editor = true;
                    selection_changed = true;
                }
                if (cc_open) {
                    render_module_dials(link.color_correction, COLOR_CORRECTION_DIALS, 3, state, l, MOD_COLOR_CORRECTION);
                    ImGui::TreePop();
                }
            }

            // Tone Mapping
            if (module_has_settings(link.tone_mapping)) {
                bool tone_open = ImGui::TreeNode("Tone Mapping");
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    state.selection.link = l;
                    state.selection.module = MOD_TONE_MAPPING;
                    state.selection.dial = 0;
                    state.panels.link_editor = true;
                    selection_changed = true;
                }
                if (tone_open) {
                    render_module_dials(link.tone_mapping, TONE_MAPPING_DIALS, 5, state, l, MOD_TONE_MAPPING);
                    ImGui::TreePop();
                }
            }

            // Global Color
            if (module_has_settings(link.global_color)) {
                bool gc_open = ImGui::TreeNode("Global Color");
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    state.selection.link = l;
                    state.selection.module = MOD_GLOBAL_COLOR;
                    state.selection.dial = 0;
                    state.panels.link_editor = true;
                    selection_changed = true;
                }
                if (gc_open) {
                    render_module_dials(link.global_color, GLOBAL_COLOR_DIALS, 3, state, l, MOD_GLOBAL_COLOR);
                    ImGui::TreePop();
                }
            }

            // Selective Color
            if (module_has_settings(link.selective_color)) {
                bool sc_open = ImGui::TreeNode("Selective Color");
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    state.selection.link = l;
                    state.selection.module = MOD_SELECTIVE_COLOR;
                    state.selection.dial = 0;
                    state.selection.detail = 0;
                    state.panels.link_editor = true;
                    selection_changed = true;
                }
                if (sc_open) {
                    render_selective_color(link.selective_color, state, l);
                    ImGui::TreePop();
                }
            }

            // Detail
            if (module_has_settings(link.detail)) {
                bool dtl_open = ImGui::TreeNode("Detail");
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    state.selection.link = l;
                    state.selection.module = MOD_DETAIL;
                    state.selection.dial = 0;
                    state.panels.link_editor = true;
                    selection_changed = true;
                }
                if (dtl_open) {
                    render_module_dials(link.detail, DETAIL_DIALS, 4, state, l, MOD_DETAIL);
                    ImGui::TreePop();
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    return selection_changed;
}

// ============================================================
// Info Panel
// ============================================================

void render_info_panel(State& state) {
    if (state.raw_info.empty()) {
        ImGui::TextDisabled("No metadata loaded");
        return;
    }

    // Scrolling table of name-value pairs
    if (ImGui::BeginTable("##info_table", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, ImGui::GetContentRegionAvail().y))) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& [key, value] : state.raw_info) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(value.c_str());
        }

        ImGui::EndTable();
    }
}

} // namespace desk
