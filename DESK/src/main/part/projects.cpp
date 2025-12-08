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

// Hot dial highlight color (cyan/blue)
static const ImVec4 HOT_DIAL_COLOR = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);

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

            // Check if this dial is hot
            bool is_hot = state.selection.link == link_idx &&
                          state.selection.is_hot(mod_idx, d);
            if (is_hot) {
                flags |= ImGuiTreeNodeFlags_Selected;
                ImGui::PushStyleColor(ImGuiCol_Text, HOT_DIAL_COLOR);
            }

            char label[64];
            snprintf(label, sizeof(label), "%s: %.2f", dials[d].name, value);
            ImGui::TreeNodeEx(label, flags);

            if (is_hot) {
                ImGui::PopStyleColor();
            }

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

        // Auto-expand if hot dial is in this color
        bool color_is_hot = state.selection.link == link_idx &&
                            state.selection.hot &&
                            state.selection.hot_module == MOD_SELECTIVE_COLOR &&
                            state.selection.hot_dial == c;
        if (color_is_hot) ImGui::SetNextItemOpen(true);

        if (ImGui::TreeNode(color_names[c])) {
            if (has_hue) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                bool is_hot = state.selection.link == link_idx &&
                              state.selection.is_hot(MOD_SELECTIVE_COLOR, c, 0);
                if (is_hot) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                    ImGui::PushStyleColor(ImGuiCol_Text, HOT_DIAL_COLOR);
                }
                char label[32];
                snprintf(label, sizeof(label), "Hue: %.2f", hue_it->second);
                ImGui::TreeNodeEx(label, flags);
                if (is_hot) ImGui::PopStyleColor();
                if (ImGui::IsItemClicked()) {
                    state.selection.link = link_idx;
                    state.selection.set_hot(MOD_SELECTIVE_COLOR, c, 0);
                    state.panels.link_editor = true;
                    changed = true;
                }
            }
            if (has_sat) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                bool is_hot = state.selection.link == link_idx &&
                              state.selection.is_hot(MOD_SELECTIVE_COLOR, c, 1);
                if (is_hot) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                    ImGui::PushStyleColor(ImGuiCol_Text, HOT_DIAL_COLOR);
                }
                char label[32];
                snprintf(label, sizeof(label), "Sat: %.2f", sat_it->second);
                ImGui::TreeNodeEx(label, flags);
                if (is_hot) ImGui::PopStyleColor();
                if (ImGui::IsItemClicked()) {
                    state.selection.link = link_idx;
                    state.selection.set_hot(MOD_SELECTIVE_COLOR, c, 1);
                    state.panels.link_editor = true;
                    changed = true;
                }
            }
            if (has_lum) {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                bool is_hot = state.selection.link == link_idx &&
                              state.selection.is_hot(MOD_SELECTIVE_COLOR, c, 2);
                if (is_hot) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                    ImGui::PushStyleColor(ImGuiCol_Text, HOT_DIAL_COLOR);
                }
                char label[32];
                snprintf(label, sizeof(label), "Lum: %.2f", lum_it->second);
                ImGui::TreeNodeEx(label, flags);
                if (is_hot) ImGui::PopStyleColor();
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

// Helper: render image preview in pipe panel
static void render_pipe_preview(State& state) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 50 || avail.y < 50) return;

    // Select which texture to show based on pipe_view
    Texture* tex = nullptr;
    const char* label = "";
    switch (state.selection.pipe_view) {
        case PipeView::BASE:
            tex = &state.base_texture;
            label = "base (scene-linear)";
            break;
        case PipeView::VIEW:
            tex = &state.embedded_texture;
            label = "view (camera preview)";
            break;
        case PipeView::BODY:
        default:
            tex = &state.texture;
            label = "body (output)";
            break;
    }

    if (!tex || !tex->loaded) {
        ImGui::TextDisabled("No image");
        return;
    }

    // Calculate size maintaining aspect ratio
    float img_w = static_cast<float>(tex->width);
    float img_h = static_cast<float>(tex->height);
    float scale_x = avail.x / img_w;
    float scale_y = (avail.y - 20) / img_h;  // Leave room for label
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    float disp_w = img_w * scale;
    float disp_h = img_h * scale;

    // Center horizontally
    float offset_x = (avail.x - disp_w) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

    ImGui::Image((ImTextureID)(intptr_t)tex->id, ImVec2(disp_w, disp_h));

    // Label below
    ImGui::TextDisabled("%s", label);
}

bool render_pipe_panel(State& state) {
    bool selection_changed = false;

    if (!state.has_project()) {
        ImGui::TextDisabled("Select a RAW file");
        return false;
    }

    Project& proj = state.current_project();

    // Title row: project name left, buttons right
    ImGui::Text("%s", proj.name.c_str());

    // Right-align + and Load buttons
    float button_width = ImGui::CalcTextSize("+ Load").x + ImGui::GetStyle().ItemSpacing.x * 3 + ImGui::GetStyle().FramePadding.x * 4;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - button_width + ImGui::GetStyle().ItemSpacing.x);

    if (ImGui::SmallButton("+")) {
        proj.links.push_back(Link("Link " + std::to_string(proj.links.size() + 1)));
        state.selection.link = static_cast<int>(proj.links.size()) - 1;
        save_pipe_json(proj);
        state.needs_reprocess = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add Link");
    }

    ImGui::SameLine();

    if (ImGui::SmallButton("Load")) {
        proj.links.push_back(Link("Preset " + std::to_string(proj.links.size() + 1)));
        state.selection.link = static_cast<int>(proj.links.size()) - 1;
        open_link_load_dialog(state);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Load Link Preset");
    }

    ImGui::Separator();

    // === HEAD node ===
    ImGuiTreeNodeFlags head_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    bool head_open = ImGui::TreeNodeEx("HEAD", head_flags);
    if (head_open) {
        // base: scene-linear from RAWS
        ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (state.selection.pipe_view == PipeView::BASE) {
            base_flags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx("base", base_flags);
        if (ImGui::IsItemClicked()) {
            state.selection.pipe_view = PipeView::BASE;
            state.selection.link = -1;  // Deselect link
            selection_changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Scene-linear RGB from RAWS decoder");
        }

        // view: embedded camera preview
        ImGuiTreeNodeFlags view_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (state.selection.pipe_view == PipeView::VIEW) {
            view_flags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx("view", view_flags);
        if (ImGui::IsItemClicked()) {
            state.selection.pipe_view = PipeView::VIEW;
            state.selection.link = -1;  // Deselect link
            selection_changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Embedded camera JPEG preview");
        }

        ImGui::TreePop();
    }

    // === BODY node ===
    ImGuiTreeNodeFlags body_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
    if (state.selection.pipe_view == PipeView::BODY && state.selection.link < 0) {
        body_flags |= ImGuiTreeNodeFlags_Selected;
    }
    bool body_open = ImGui::TreeNodeEx("BODY", body_flags);

    // Clicking BODY shows body output
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        state.selection.pipe_view = PipeView::BODY;
        selection_changed = true;
    }

    if (body_open) {
        if (proj.links.empty()) {
            ImGui::TextDisabled("No links - click + to add");
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
            state.selection.pipe_view = PipeView::BODY;  // Show body output
            state.panels.link_editor = true;
        }

        // Right-align buttons: Edit, Save, X
        float btn_width = ImGui::CalcTextSize("Edit Save X").x + ImGui::GetStyle().ItemSpacing.x * 4 + ImGui::GetStyle().FramePadding.x * 6;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - btn_width + ImGui::GetCursorPosX());

        // Edit button
        if (ImGui::SmallButton("Edit")) {
            link.editing_name = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Rename link");
        }

        // Save button
        ImGui::SameLine();
        if (ImGui::SmallButton("Save")) {
            state.selection.link = l;
            open_link_save_dialog(state);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Save link as preset");
        }

        // Remove button (X)
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
        if (ImGui::SmallButton("X")) {
            ImGui::PopStyleColor(3);
            proj.links.erase(proj.links.begin() + l);
            save_pipe_json(proj);
            if (state.selection.link == l) {
                state.selection.link = -1;
            } else if (state.selection.link > l) {
                state.selection.link--;
            }
            state.needs_reprocess = true;
            ImGui::PopID();
            if (link_open) ImGui::TreePop();
            continue;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Remove link");
        }

        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                link.editing_name = true;
            }
            ImGui::Separator();
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
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                link.name = name_buf;
                link.editing_name = false;
                save_pipe_json(proj);
            }
            // Escape cancels edit
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                link.editing_name = false;
            }
            // Click outside cancels edit
            if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
                link.editing_name = false;
            }
        }

        if (link_open) {
            // Helper: check if hot dial is in this module for this link
            auto is_hot_module = [&](int mod_id) {
                return state.selection.link == l &&
                       state.selection.hot &&
                       state.selection.hot_module == mod_id;
            };

            // Geometric
            if (module_has_settings(link.geometric)) {
                if (is_hot_module(MOD_GEOMETRIC)) ImGui::SetNextItemOpen(true);
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
                if (is_hot_module(MOD_COLOR_CORRECTION)) ImGui::SetNextItemOpen(true);
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
                if (is_hot_module(MOD_TONE_MAPPING)) ImGui::SetNextItemOpen(true);
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
                if (is_hot_module(MOD_GLOBAL_COLOR)) ImGui::SetNextItemOpen(true);
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
                if (is_hot_module(MOD_SELECTIVE_COLOR)) ImGui::SetNextItemOpen(true);
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
                if (is_hot_module(MOD_DETAIL)) ImGui::SetNextItemOpen(true);
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
        }  // end links loop

        ImGui::TreePop();  // close BODY
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

    // Build tree structure from dot-separated keys
    // e.g., "camera.make" -> tree["camera"]["make"] = value
    std::map<std::string, std::map<std::string, std::string>> tree;
    std::map<std::string, std::string> ungrouped;

    for (const auto& [key, value] : state.raw_info) {
        size_t dot = key.find('.');
        if (dot != std::string::npos) {
            std::string group = key.substr(0, dot);
            std::string field = key.substr(dot + 1);
            tree[group][field] = value;
        } else {
            ungrouped[key] = value;
        }
    }

    // Render tree with collapsible nodes
    for (const auto& [group, fields] : tree) {
        if (ImGui::TreeNodeEx(group.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable(("##info_" + group).c_str(), 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [field, value] : fields) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", field.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                }

                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }

    // Render ungrouped keys (if any)
    if (!ungrouped.empty()) {
        if (ImGui::TreeNodeEx("other", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##info_other", 2,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {

                ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                for (const auto& [key, value] : ungrouped) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%s", key.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(value.c_str());
                }

                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

} // namespace desk
