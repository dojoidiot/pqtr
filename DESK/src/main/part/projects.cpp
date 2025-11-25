// projects.cpp - Projects panel implementation

#include "projects.hpp"
#include "files.hpp"
#include "imgui.h"

namespace desk {

bool render_projects_panel(State& state) {
    bool selection_changed = false;

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Projects");

    // Add project button
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    if (ImGui::SmallButton("+##add_project")) {
        if (state.root_folder_set) {
            open_raw_file_dialog();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add RAW file to project");
    }

    ImGui::Separator();

    if (!state.root_folder_set) {
        ImGui::TextWrapped("No root folder selected.\n\nUse File > Select Root Folder to begin.");
        return false;
    }

    if (state.projects.empty()) {
        ImGui::TextWrapped("No projects found.\n\nClick + to add a RAW file.");
        return false;
    }

    // Project tree
    for (int p = 0; p < (int)state.projects.size(); p++) {
        Project& proj = state.projects[p];

        ImGui::PushID(p);

        // Tree node for project
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (state.selected_project == p && state.selected_link == -1) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (proj.links.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        bool node_open = ImGui::TreeNodeEx(proj.name.c_str(), flags);

        // Select project on click
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            if (state.selected_project != p) {
                state.selected_project = p;
                state.selected_link = -1;
                selection_changed = true;
            }
        }

        // Project buttons (same line)
        ImGui::SameLine(ImGui::GetWindowWidth() - 60);

        // Add link button
        if (ImGui::SmallButton("+##add_link")) {
            proj.links.push_back(Link("New Link"));
            save_pipe_json(proj);
            state.needs_reprocess = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Add Link");
        }

        ImGui::SameLine();

        // Hide project button
        if (ImGui::SmallButton("-##hide")) {
            proj.hidden = true;
            save_desk_json(proj);
            if (state.selected_project == p) {
                state.selected_project = -1;
                state.selected_link = -1;
                selection_changed = true;
            }
            // Remove from list (will be re-scanned as hidden)
            state.projects.erase(state.projects.begin() + p);
            p--;
            ImGui::PopID();
            if (node_open) ImGui::TreePop();
            continue;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Hide Project");
        }

        // Links under project
        if (node_open) {
            for (int l = 0; l < (int)proj.links.size(); l++) {
                Link& link = proj.links[l];

                ImGui::PushID(l);

                ImGuiTreeNodeFlags link_flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (state.selected_project == p && state.selected_link == l) {
                    link_flags |= ImGuiTreeNodeFlags_Selected;
                }

                // Link name (editable)
                if (link.editing_name) {
                    static char name_buf[64];
                    if (ImGui::IsWindowAppearing() || !link.editing_name) {
                        strncpy(name_buf, link.name.c_str(), sizeof(name_buf) - 1);
                    }

                    ImGui::SetNextItemWidth(120);
                    if (ImGui::InputText("##name", name_buf, sizeof(name_buf),
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                        link.name = name_buf;
                        link.editing_name = false;
                        save_pipe_json(proj);
                    }
                    if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
                        link.editing_name = false;
                    }
                } else {
                    ImGui::TreeNodeEx(link.name.c_str(), link_flags);

                    // Select link on click
                    if (ImGui::IsItemClicked()) {
                        state.selected_project = p;
                        state.selected_link = l;
                        selection_changed = true;
                    }
                }

                // Link buttons
                ImGui::SameLine(ImGui::GetWindowWidth() - 60);

                // Edit name button
                if (ImGui::SmallButton("E##edit")) {
                    link.editing_name = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Edit Name");
                }

                ImGui::SameLine();

                // Remove link button
                if (ImGui::SmallButton("-##remove")) {
                    proj.links.erase(proj.links.begin() + l);
                    save_pipe_json(proj);
                    if (state.selected_link == l) {
                        state.selected_link = -1;
                        selection_changed = true;
                    } else if (state.selected_link > l) {
                        state.selected_link--;
                    }
                    state.needs_reprocess = true;
                    l--;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Remove Link");
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    return selection_changed;
}

} // namespace desk
