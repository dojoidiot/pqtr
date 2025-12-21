// head.cpp - HEAD tile (tune stages) with local state

#include "desk.hpp"

// Local state for HEAD tile
static struct {
    char current_pipe[64] = "";

    unsigned int head_texture = 0;
    unsigned int lute_texture = 0;
    unsigned int drum_texture = 0;
    unsigned int diff_texture = 0;
    unsigned int preview_texture = 0;

    int head_width = 0;
    int head_height = 0;
    int preview_width = 0;
    int preview_height = 0;
} s_head;

void render_head_tile(float x, float y, float width, float height) {
    // Handle notes
    char data[256];
    if (checkNote("pipe.select", data, sizeof(data))) {
        strncpy(s_head.current_pipe, data, sizeof(s_head.current_pipe) - 1);
    }

    // Sync from global state (temporary during migration)
    strncpy(s_head.current_pipe, g_state.current_pipe, sizeof(s_head.current_pipe) - 1);
    s_head.head_texture = g_state.head_texture;
    s_head.lute_texture = g_state.lute_texture;
    s_head.drum_texture = g_state.drum_texture;
    s_head.diff_texture = g_state.diff_texture;
    s_head.preview_texture = g_state.preview_texture;
    s_head.head_width = g_state.head_width;
    s_head.head_height = g_state.head_height;
    s_head.preview_width = g_state.preview_width;
    s_head.preview_height = g_state.preview_height;

    ImGuiWindowFlags fixed_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    if (ImGui::Begin("##tune", nullptr, fixed_flags)) {
        ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "Tune");
        ImGui::SameLine();

        if (s_head.current_pipe[0]) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), " - %s", s_head.current_pipe);
        }

        float pane_width = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(pane_width - 40);
        bool can_tune = s_head.current_pipe[0] != '\0';
        if (!can_tune) ImGui::BeginDisabled();
        if (ImGui::SmallButton("Tune")) {
            postNote("tune.start", s_head.current_pipe);
        }
        if (!can_tune) ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Spacing();

        float gap = 10.0f;
        float sw = (pane_width - (3 * gap)) / 4.0f;
        float sh = sw * 2.0f / 3.0f;

        auto renderStage = [&](const char* label, const char* id, ImVec4 label_color,
                               unsigned int texture, int tex_w, int tex_h, const char* placeholder) {
            ImGui::BeginGroup();
            ImGui::TextColored(label_color, "%s", label);
            ImGui::BeginChild(id, ImVec2(sw, sh), true);
            if (texture && tex_w > 0 && tex_h > 0) {
                float aspect = (float)tex_w / (float)tex_h;
                float child_width = ImGui::GetContentRegionAvail().x;
                float child_height = ImGui::GetContentRegionAvail().y;

                float img_width = child_width;
                float img_height = img_width / aspect;
                if (img_height > child_height) {
                    img_height = child_height;
                    img_width = img_height * aspect;
                }
                float offset_x = (child_width - img_width) * 0.5f;
                float offset_y = (child_height - img_height) * 0.5f;
                if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
                if (offset_y > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
                ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(img_width, img_height));
            } else if (s_head.current_pipe[0]) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", placeholder);
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "—");
            }
            ImGui::EndChild();
            ImGui::EndGroup();
        };

        unsigned int head_tex = s_head.head_texture ? s_head.head_texture : s_head.preview_texture;
        int head_w = s_head.head_texture ? s_head.head_width : s_head.preview_width;
        int head_h = s_head.head_texture ? s_head.head_height : s_head.preview_height;
        renderStage("HEAD", "stage_head", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), head_tex, head_w, head_h, "Press Tune");

        ImGui::SameLine(0, gap);
        renderStage("LUTE", "stage_lute", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), s_head.lute_texture, 0, 0, "[camera profile]");

        ImGui::SameLine(0, gap);
        renderStage("DRUM", "stage_drum", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), s_head.drum_texture, 0, 0, "[dynamic range]");

        ImGui::SameLine(0, gap);
        renderStage("DIFF", "stage_diff", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), s_head.diff_texture, 0, 0, "[diff from camera]");
    }
    ImGui::End();
}
