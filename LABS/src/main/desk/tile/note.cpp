// note.cpp - Note tile (event history) with local state

#include "desk.hpp"

// Local state for Note tile
static struct {
    Note history[64] = {};
    int history_count = 0;
    bool scroll_to_bottom = false;
} s_note;

void render_note_tile(float x, float y, float width, float height) {
    // Sync from global note history (temporary during migration)
    s_note.history_count = g_note_history_count;
    for (int i = 0; i < s_note.history_count && i < 64; i++) {
        s_note.history[i] = g_note_history[i];
    }
    s_note.scroll_to_bottom = g_note_history_scroll;

    ImGuiWindowFlags fixed_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    if (ImGui::Begin("##note", nullptr, fixed_flags)) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Note");
        ImGui::SameLine(width - 60);
        if (ImGui::SmallButton("Clear")) {
            s_note.history_count = 0;
            g_note_history_count = 0;
        }
        ImGui::Separator();

        ImGui::BeginChild("NoteScroll", ImVec2(0, 0), false);
        for (int i = 0; i < s_note.history_count; i++) {
            Note& n = s_note.history[i];
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", n.event);
            if (n.data[0]) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", n.data);
            }
        }
        if (s_note.scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            g_note_history_scroll = false;
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
