// desk.cpp - Desktop screen layout and dialogs

#include "desk.hpp"
#include <cstring>
#include <cstdio>

void render_desktop_screen() {
    ImGuiIO& io = ImGui::GetIO();

    // Load layout on first render (tune defaults to off)
    if (!g_state.layout_loaded) {
        g_state.show_labs_panel = true;
        g_state.show_note_pane = true;
        g_state.show_tune_pane = loadLayoutTune() != 0;
        g_state.layout_loaded = true;
    }

    bool prev_tune = g_state.show_tune_pane;

    // Menu bar
    float menu_height = 0;
    if (ImGui::BeginMainMenuBar()) {
        menu_height = ImGui::GetWindowHeight();

        if (ImGui::SmallButton("Logout")) {
            invalidateRequests();
            g_state.screen = Screen::Login;
            g_state.jwt[0] = '\0';
            g_state.email[0] = '\0';
            g_state.otp[0] = '\0';
            g_state.itag[0] = '\0';
            g_state.list_loaded = false;
            g_state.pipe_count = 0;
            g_state.raws_name[0] = '\0';
            clearAuth();
        }

        ImGui::SameLine(0, 20);
        ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.3f, 1.0f), "|");
        ImGui::SameLine(0, 20);

        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "View:");
        ImGui::SameLine(0, 10);

        // Tune toggle
        if (g_state.show_tune_pane) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::SmallButton("[x] Tune")) g_state.show_tune_pane = false;
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::SmallButton("[ ] Tune")) g_state.show_tune_pane = true;
            ImGui::PopStyleColor();
        }

        // User info
        float user_width = ImGui::CalcTextSize(g_state.itag).x + 20;
        ImGui::SetCursorPosX(io.DisplaySize.x - user_width);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", g_state.itag);

        ImGui::EndMainMenuBar();
    }

    // Save layout if tune changed
    if (g_state.show_tune_pane != prev_tune) {
        saveLayout(1, 1, g_state.show_tune_pane ? 1 : 0);
    }

    // Process notes
    if (checkNote("labs.open")) {
        g_state.list_loaded = false;
        if (!g_state.list_loading) {
            sendListRequest();
            postNote("labs.list", "loading");
        }
    }

    if (checkNote("tune.start")) {
        if (!g_state.tune_running && g_state.current_pipe[0]) {
            startTunePipeline();
        }
    }

    if (checkNote("raws.load")) {
        g_state.list_loaded = false;
        g_state.files_loaded = false;
        char base_name[256];
        strncpy(base_name, g_state.raws_name, sizeof(base_name) - 1);
        char* dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';
        strncpy(g_state.current_pipe, base_name, sizeof(g_state.current_pipe) - 1);
        postNote("pipe.select", base_name);
    }

    if (!g_state.list_loaded && !g_state.list_loading) {
        sendListRequest();
        postNote("labs.list", "loading");
    }

    // Layout constants
    float left_width = 280.0f;
    float content_top = menu_height;
    float content_height = io.DisplaySize.y - content_top;
    float labs_height = content_height * 0.40f;
    float pipe_height = content_height * 0.35f;
    float note_height = content_height * 0.25f;

    // Render tiles (left column: BASE, PIPE, NOTE)
    render_base_tile(0, content_top, left_width, labs_height);
    render_pipe_tile(0, content_top + labs_height, left_width, pipe_height);
    render_note_tile(0, content_top + labs_height + pipe_height, left_width, note_height);

    if (g_state.show_tune_pane) {
        float tune_x = left_width;
        float tune_width = io.DisplaySize.x - left_width;
        render_head_tile(tune_x, content_top, tune_width, content_height);
    }

    // Task view dialog (top 1/3, fixed size)
    if (g_state.show_task_view) {
        bool all_done = g_state.current_task < 0;
        bool has_error = false;
        for (int i = 0; i < g_state.task_count; i++) {
            if (g_state.tasks[i].status == TaskStatus::Error) {
                has_error = true;
                break;
            }
        }

        if (all_done && !has_error) {
            g_state.show_task_view = false;
        } else {
            ImGui::SetNextWindowSize(ImVec2(350, 200));
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.33f),
                ImGuiCond_Always, ImVec2(0.5f, 0.5f));

            if (ImGui::Begin("Task View", nullptr,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
                for (int i = 0; i < g_state.task_count; i++) {
                    Task& task = g_state.tasks[i];

                    ImVec4 color;
                    const char* icon;
                    switch (task.status) {
                        case TaskStatus::Pending:  color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); icon = "[ ]"; break;
                        case TaskStatus::Running:  color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f); icon = "[~]"; break;
                        case TaskStatus::Done:     color = ImVec4(0.4f, 0.9f, 0.4f, 1.0f); icon = "[+]"; break;
                        case TaskStatus::Error:    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); icon = "[!]"; break;
                    }

                    ImGui::TextColored(color, "%s %s", icon, task.name);
                    if (task.message[0]) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "- %s", task.message);
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (has_error) {
                    if (ImGui::Button("Shut", ImVec2(80, 0))) {
                        g_state.show_task_view = false;
                    }
                }
            }
            ImGui::End();
        }
    }

    // RAW upload dialog (top 1/3, fixed size)
    if (g_state.show_raw_dialog) {
        ImGui::SetNextWindowSize(ImVec2(400, 270));
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.33f),
            ImGuiCond_Always, ImVec2(0.5f, 0.5f));

        if (ImGui::Begin("New RAW", &g_state.show_raw_dialog,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar)) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "File: %s", g_state.pending_raw_filename);
            ImGui::Spacing();

            ImGui::Text("Name");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##name", g_state.raw_dialog_name, sizeof(g_state.raw_dialog_name));

            ImGui::Spacing();
            ImGui::Text("Find (optional)");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextMultiline("##find", g_state.raw_dialog_find, sizeof(g_state.raw_dialog_find),
                ImVec2(-1, 80));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool name_empty = g_state.raw_dialog_name[0] == '\0';
            if (name_empty) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Save", ImVec2(80, 0))) {
                confirmRawUpload();
            }
            if (name_empty) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                cancelRawUpload();
            }
        }
        ImGui::End();
    }
}
