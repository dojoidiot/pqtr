// pipe.cpp - PIPE tile (pipe.json tree view) with local state

#include "desk.hpp"

// JSON tree rendering
namespace {

void formatNumber(char* buf, size_t size, double n) {
    if (n == (int64_t)n) {
        snprintf(buf, size, "%lld", (long long)n);
    } else {
        snprintf(buf, size, "%.6g", n);
    }
}

void renderNode(const char* key, const json::Value* value);

void renderNode(const char* key, const json::Value* value) {
    if (!value) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: null", key);
        return;
    }

    switch (value->type) {
        case json::Type::Null:
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s: null", key);
            break;

        case json::Type::Bool:
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: ", key);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f), "%s",
                value->bool_val ? "true" : "false");
            break;

        case json::Type::Number: {
            char num[32];
            formatNumber(num, sizeof(num), value->num_val);
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: ", key);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%s", num);
            break;
        }

        case json::Type::String:
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: ", key);
            ImGui::SameLine(0, 0);
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.5f, 1.0f), "\"%s\"", value->str_val.c_str());
            break;

        case json::Type::Array: {
            if (value->arr_val.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: []", key);
            } else {
                char label[128];
                snprintf(label, sizeof(label), "%s [%zu]", key, value->arr_val.size());
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                bool open = ImGui::TreeNode(label);
                ImGui::PopStyleColor();
                if (open) {
                    for (size_t i = 0; i < value->arr_val.size(); i++) {
                        char idx[16];
                        snprintf(idx, sizeof(idx), "[%zu]", i);
                        renderNode(idx, value->arr_val[i].get());
                    }
                    ImGui::TreePop();
                }
            }
            break;
        }

        case json::Type::Object: {
            if (value->obj_val.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: {}", key);
            } else {
                char label[128];
                snprintf(label, sizeof(label), "%s {%zu}", key, value->obj_val.size());
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                bool open = ImGui::TreeNode(label);
                ImGui::PopStyleColor();
                if (open) {
                    for (const auto& pair : value->obj_val) {
                        renderNode(pair.key.c_str(), pair.value.get());
                    }
                    ImGui::TreePop();
                }
            }
            break;
        }
    }
}

void renderTree(const json::Value* value) {
    if (!value) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(invalid JSON)");
        return;
    }

    switch (value->type) {
        case json::Type::Object:
            for (const auto& pair : value->obj_val) {
                renderNode(pair.key.c_str(), pair.value.get());
            }
            break;
        case json::Type::Array:
            for (size_t i = 0; i < value->arr_val.size(); i++) {
                char idx[16];
                snprintf(idx, sizeof(idx), "[%zu]", i);
                renderNode(idx, value->arr_val[i].get());
            }
            break;
        default:
            renderNode("root", value);
            break;
    }
}

} // namespace

// Local state for PIPE tile
static struct {
    char current_pipe[64] = "";
    char json_file[64] = "";
    json::Value* json_tree = nullptr;  // View only, owned by g_state
    bool json_loaded = false;
    bool json_loading = false;
} s_pipe;

void render_pipe_tile(float x, float y, float width, float height) {
    // Handle notes
    char data[256];

    if (checkNote("pipe.select", data, sizeof(data))) {
        strncpy(s_pipe.current_pipe, data, sizeof(s_pipe.current_pipe) - 1);
        s_pipe.json_file[0] = '\0';
        s_pipe.json_tree = nullptr;
        s_pipe.json_loaded = false;
    }

    if (checkNote("drop.done", data, sizeof(data))) {
        if (strcmp(s_pipe.current_pipe, data) == 0) {
            s_pipe.current_pipe[0] = '\0';
            s_pipe.json_file[0] = '\0';
            s_pipe.json_tree = nullptr;
            s_pipe.json_loaded = false;
        }
    }

    // Sync from global state (temporary during migration)
    strncpy(s_pipe.current_pipe, g_state.current_pipe, sizeof(s_pipe.current_pipe) - 1);
    s_pipe.json_loaded = g_state.json_loaded;
    s_pipe.json_loading = g_state.json_loading;
    strncpy(s_pipe.json_file, g_state.json_file, sizeof(s_pipe.json_file) - 1);
    s_pipe.json_tree = g_state.json_tree.get();

    // Auto-load pipe.json when pipe selected
    if (s_pipe.current_pipe[0] && !s_pipe.json_loaded && !s_pipe.json_loading) {
        char pipe_json[128];
        snprintf(pipe_json, sizeof(pipe_json), "%s.pipe.json", s_pipe.current_pipe);
        sendJsonRequest(s_pipe.current_pipe, pipe_json);
    }

    ImGuiWindowFlags fixed_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    if (ImGui::Begin("##pipe", nullptr, fixed_flags)) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "PIPE");
        if (s_pipe.current_pipe[0]) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.6f, 0.4f, 1.0f), " %s", s_pipe.current_pipe);
        }
        ImGui::Separator();

        if (!s_pipe.current_pipe[0]) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Select a pipe");
        } else if (s_pipe.json_loading) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loading...");
        } else if (s_pipe.json_loaded && s_pipe.json_tree) {
            ImGui::BeginChild("PipeTree", ImVec2(0, 0), false);
            renderTree(s_pipe.json_tree);
            ImGui::EndChild();
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.5f, 0.4f, 1.0f), "No pipe.json");
        }
    }
    ImGui::End();
}
