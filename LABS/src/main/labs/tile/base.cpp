// base.cpp - BASE tile (file viewer) with local state

#include "desk.hpp"

// Local state for BASE tile
static struct {
    char pipes[64][64] = {};
    int pipe_count = 0;
    char current_pipe[64] = "";
    bool list_loaded = false;
    bool list_loading = false;

    char files[64][64] = {};
    int file_count = 0;
    bool files_loaded = false;
    bool files_loading = false;
} s_base;

void render_base_tile(float x, float y, float width, float height) {
    // Handle notes
    char data[256];

    if (checkNote("push.done", data, sizeof(data))) {
        strncpy(s_base.current_pipe, data, sizeof(s_base.current_pipe) - 1);
        s_base.files_loaded = false;
        s_base.list_loaded = false;
        postNote("pipe.select", data);
    }

    if (checkNote("labs.list.result", data, sizeof(data))) {
        // Parse pipe list from data (comma-separated)
        s_base.pipe_count = 0;
        s_base.list_loading = false;
        s_base.list_loaded = true;
        // Data parsing handled by BASE response
    }

    if (checkNote("drop.done", data, sizeof(data))) {
        if (strcmp(s_base.current_pipe, data) == 0) {
            s_base.current_pipe[0] = '\0';
            s_base.files_loaded = false;
        }
        s_base.list_loaded = false;
    }

    // Request list if needed (use global state for now - will migrate)
    if (!g_state.list_loaded && !g_state.list_loading) {
        sendListRequest();
        postNote("labs.list", "loading");
    }

    // Sync from global state (temporary during migration)
    s_base.pipe_count = g_state.pipe_count;
    for (int i = 0; i < s_base.pipe_count; i++) {
        strncpy(s_base.pipes[i], g_state.pipes[i], 63);
    }
    s_base.list_loaded = g_state.list_loaded;
    s_base.list_loading = g_state.list_loading;
    s_base.file_count = g_state.file_count;
    for (int i = 0; i < s_base.file_count; i++) {
        strncpy(s_base.files[i], g_state.files[i], 63);
    }
    s_base.files_loaded = g_state.files_loaded;
    s_base.files_loading = g_state.files_loading;

    ImGuiWindowFlags fixed_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    if (ImGui::Begin("##base", nullptr, fixed_flags)) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "BASE");
        ImGui::SameLine(width - 70);
        if (ImGui::SmallButton("Load RAW")) {
            openRawFilePicker();
        }
        ImGui::Separator();

        if (s_base.list_loading) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loading...");
        } else if (s_base.pipe_count == 0) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.4f, 1.0f), "Load a RAW file to begin");
        } else {
            for (int i = 0; i < s_base.pipe_count; i++) {
                bool is_selected = strcmp(s_base.pipes[i], s_base.current_pipe) == 0;

                float avail_width = ImGui::GetContentRegionAvail().x;
                float btn_width = 25.0f;
                float sel_width = avail_width - btn_width - 5.0f;

                ImGui::PushID(i);
                ImGui::SetNextItemWidth(sel_width);
                if (ImGui::Selectable(s_base.pipes[i], is_selected, 0, ImVec2(sel_width, 0))) {
                    if (!is_selected) {
                        strncpy(s_base.current_pipe, s_base.pipes[i], sizeof(s_base.current_pipe) - 1);
                        strncpy(g_state.current_pipe, s_base.pipes[i], sizeof(g_state.current_pipe) - 1);
                        s_base.files_loaded = false;
                        g_state.files_loaded = false;
                        g_state.json_file[0] = '\0';
                        g_state.json_tree.reset();
                        g_state.json_loaded = false;
                        postNote("pipe.select", s_base.current_pipe);
                    }
                }
                ImGui::SameLine(sel_width + 5.0f);
                // JPG+ button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.5f, 0.2f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.6f, 0.3f, 1.0f));
                if (ImGui::SmallButton("+")) {
                    postNote("vibe.add", s_base.pipes[i]);
                }
                ImGui::PopStyleColor(3);
                ImGui::SameLine();
                // Del button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.3f, 0.3f, 1.0f));
                if (ImGui::SmallButton("-")) {
                    sendDropRequest(s_base.pipes[i]);
                    postNote("drop.sent", s_base.pipes[i]);
                }
                ImGui::PopStyleColor(3);
                ImGui::PopID();

                if (is_selected && s_base.current_pipe[0]) {
                    if (!s_base.files_loaded && !s_base.files_loading) {
                        sendFilesRequest(s_base.current_pipe);
                    }

                    if (s_base.files_loading) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "    ...");
                    } else if (s_base.file_count > 0) {
                        for (int f = 0; f < s_base.file_count; f++) {
                            const char* fname = s_base.files[f];
                            ImGui::Indent(20.0f);
                            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", fname);
                            ImGui::Unindent(20.0f);
                        }
                    }
                }
            }
        }
    }
    ImGui::End();
}

// Create initial pipe.json after RAW upload
void createPipeJson(const char* pipe_name, const char* raw_filename) {
    // Build vibe file name: <basename>.0.jpg (embedded JPEG from RAW)
    static char vibe_file[128];
    snprintf(vibe_file, sizeof(vibe_file), "%s.0.jpg", pipe_name);

    // Use dialog name if available
    const char* vibe_name = g_state.raw_dialog_name[0] ? g_state.raw_dialog_name : pipe_name;

    // Escape find field
    static char escaped_find[512];
    size_t j = 0;
    for (size_t i = 0; g_state.raw_dialog_find[i] && j < sizeof(escaped_find) - 2; i++) {
        char c = g_state.raw_dialog_find[i];
        if (c == '"') escaped_find[j++] = '\'';
        else if (c == '\n') escaped_find[j++] = ' ';
        else if (c == '\r') { }
        else escaped_find[j++] = c;
    }
    escaped_find[j] = '\0';

    // Build pipe.json content
    static char pipe_json[2048];
    snprintf(pipe_json, sizeof(pipe_json),
        "{"
        "\"pipe\":\"1.0\","
        "\"head\":{\"file\":\"%s\"},"
        "\"body\":{\"lute\":{},\"drum\":{},\"vibe\":[]},"
        "\"tail\":[\"head\",\"lute\",\"drum\",\"vibe\",\"diff\"],"
        "\"vibe-list\":[{\"file\":\"%s\",\"name\":\"%s\",\"find\":\"%s\"}]"
        "}",
        raw_filename, vibe_file, vibe_name, escaped_find);

    // Build filename: <pipe_name>.pipe.json
    static char filename[256];
    snprintf(filename, sizeof(filename), "%s.pipe.json", pipe_name);

    // Push to BASE
    sendPushJsonRequest(pipe_name, filename, pipe_json);
}
