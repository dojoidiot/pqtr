// chat.cpp - Chat pane for tune progress
//
// Simple scrolling text log showing tune's work.

#include "chat.hpp"
#include "imgui.h"
#include <vector>
#include <mutex>

namespace desk {

// ============================================================
// State
// ============================================================

static std::vector<std::string> g_messages;
static std::mutex g_mutex;
static bool g_active = false;
static bool g_scroll_to_bottom = false;

// ============================================================
// Public Interface
// ============================================================

void chat(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_messages.push_back(message);
    g_scroll_to_bottom = true;

    // Keep last 100 messages
    if (g_messages.size() > 100) {
        g_messages.erase(g_messages.begin());
    }
}

void chat_clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_messages.clear();
}

bool is_chat_active() {
    return g_active;
}

void set_chat_active(bool active) {
    g_active = active;
    if (active) {
        chat_clear();
    }
}

void render_chat_panel() {
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Chat log area
    ImGui::BeginChild("##chat_log", ImVec2(avail.x, avail.y - 5),
                      ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (g_messages.empty()) {
            ImGui::TextDisabled("Tune progress will appear here...");
        } else {
            for (const auto& msg : g_messages) {
                // Color based on message type
                if (msg.find("Error") != std::string::npos ||
                    msg.find("Failed") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(msg.c_str());
                    ImGui::PopStyleColor();
                } else if (msg.find("Done") != std::string::npos ||
                           msg.find("Complete") != std::string::npos ||
                           msg.find("Converged") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    ImGui::TextUnformatted(msg.c_str());
                    ImGui::PopStyleColor();
                } else if (msg.find("%") != std::string::npos) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 1.0f, 1.0f));
                    ImGui::TextUnformatted(msg.c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::TextUnformatted(msg.c_str());
                }
            }
        }

        // Auto-scroll
        if (g_scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            g_scroll_to_bottom = false;
        }
    }

    ImGui::EndChild();
}

} // namespace desk
