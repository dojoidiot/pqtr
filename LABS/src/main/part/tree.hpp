// tree.hpp - ImGui tree view for JSON values
// Renders json::Value as expandable tree nodes

#pragma once
#include "json.hpp"
#include "imgui.h"
#include <cstdio>

namespace tree {

// Format number without trailing zeros
inline void formatNumber(char* buf, size_t size, double n) {
    if (n == (int64_t)n) {
        snprintf(buf, size, "%lld", (long long)n);
    } else {
        snprintf(buf, size, "%.6g", n);
    }
}

// Render a JSON value as ImGui tree nodes
// key: display name (can be index for arrays)
// value: the JSON value to render
inline void render(const char* key, const json::Value* value) {
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
            char label[128];
            snprintf(label, sizeof(label), "%s [%zu]", key, value->arr_val.size());

            if (value->arr_val.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: []", key);
            } else if (ImGui::TreeNode(label)) {
                for (size_t i = 0; i < value->arr_val.size(); i++) {
                    char idx[16];
                    snprintf(idx, sizeof(idx), "[%zu]", i);
                    render(idx, value->arr_val[i].get());
                }
                ImGui::TreePop();
            }
            break;
        }

        case json::Type::Object: {
            char label[128];
            snprintf(label, sizeof(label), "%s {%zu}", key, value->obj_val.size());

            if (value->obj_val.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s: {}", key);
            } else if (ImGui::TreeNode(label)) {
                for (const auto& pair : value->obj_val) {
                    render(pair.key.c_str(), pair.value.get());
                }
                ImGui::TreePop();
            }
            break;
        }
    }
}

// Render root JSON value (no key)
inline void render(const json::Value* value) {
    if (!value) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(invalid JSON)");
        return;
    }

    switch (value->type) {
        case json::Type::Object:
            for (const auto& pair : value->obj_val) {
                render(pair.key.c_str(), pair.value.get());
            }
            break;

        case json::Type::Array:
            for (size_t i = 0; i < value->arr_val.size(); i++) {
                char idx[16];
                snprintf(idx, sizeof(idx), "[%zu]", i);
                render(idx, value->arr_val[i].get());
            }
            break;

        default:
            render("root", value);
            break;
    }
}

} // namespace tree
