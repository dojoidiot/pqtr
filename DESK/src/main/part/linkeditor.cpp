// linkeditor.cpp - Link editor implementation
// Module menus with cookie-trail navigation and graphic equalizer

#include "linkeditor.hpp"
#include "imgui.h"
#include <cmath>

namespace desk {

// ============================================================
// Constants
// ============================================================

// Subject names (5 golden modules - Geometric hidden per spec)
static const char* SUBJECT_NAMES[] = {
    "Color Correction", "Tone Mapping", "Global Color", "Selective Colour", "Detail"
};

// Module dial names per subject
static const char* MODULE_NAMES_CC[] = {"Exposure", "Temperature", "Tint"};
static const char* MODULE_NAMES_TONE[] = {"Contrast", "Highlights", "Shadows"};
static const char* MODULE_NAMES_COLOR[] = {"Vibrance", "Saturation", "Colour Density"};
static const char* MODULE_NAMES_SEL[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Magenta"};
static const char* MODULE_NAMES_DTL[] = {"Sharp Amount", "Sharp Radius", "Denoise Lum", "Denoise Chroma"};

static const char** MODULE_NAMES[] = {
    MODULE_NAMES_CC,
    MODULE_NAMES_TONE,
    MODULE_NAMES_COLOR,
    MODULE_NAMES_SEL,
    MODULE_NAMES_DTL
};

static const int MODULE_COUNTS[] = {3, 3, 3, 8, 4};
static constexpr int SUBJECT_COUNT = 5;

// Detail menus for subjects that have sub-dials
static const char* DETAIL_TONE[] = {"Black", "White"};
static const char* DETAIL_SEL[] = {"Hue", "Saturation", "Luminance"};

// Box styling
static const ImVec4 BOX_NORMAL = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
static const ImVec4 BOX_HOVER = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
static const ImVec4 BOX_SELECTED = ImVec4(0.3f, 0.5f, 0.7f, 1.0f);

// Slider styling
static const ImU32 SLIDER_LINE_INACTIVE = IM_COL32(60, 60, 60, 255);
static const ImU32 SLIDER_LINE_ACTIVE = IM_COL32(80, 130, 180, 255);
static const ImU32 SLIDER_BALL_INACTIVE = IM_COL32(100, 100, 100, 255);
static const ImU32 SLIDER_BALL_ACTIVE = IM_COL32(200, 200, 200, 255);

// Total visible dials (39 = 45 - 6 geometric)
static constexpr int DIAL_COUNT = 39;

// ============================================================
// Module State
// ============================================================

// Editor state
static int expanding = 0;   // 0=collapsed, 1=subject, 2=module, 3=detail
static int detail_sel = 0;  // Sub-dial selection
static int dragging_slider = -1;

// All dial values
static float all_dials[DIAL_COUNT] = {
    // ColorCorrection (0-2): exposure, temperature, tint
    0.5f, 0.5f, 0.5f,
    // ToneMapping (3-7): contrast, highlights, shadows, black, white
    0.5f, 0.5f, 0.5f, 0.15f, 0.85f,
    // GlobalColor (8-10): vibrance, saturation, colour_density
    0.5f, 0.5f, 0.5f,
    // SelectiveColour (11-34): 8 colors x 3 (hue, sat, lum)
    0.5f, 0.5f, 0.5f,  // red
    0.5f, 0.5f, 0.5f,  // orange
    0.5f, 0.5f, 0.5f,  // yellow
    0.5f, 0.5f, 0.5f,  // green
    0.5f, 0.5f, 0.5f,  // cyan
    0.5f, 0.5f, 0.5f,  // blue
    0.5f, 0.5f, 0.5f,  // purple
    0.5f, 0.5f, 0.5f,  // magenta
    // Detail (35-38): sharp_amount, sharp_radius, denoise_lum, denoise_chroma
    0.5f, 0.5f, 0.5f, 0.5f,
};

// ============================================================
// Helper Functions
// ============================================================

static bool render_box(const char* label, bool selected, float width = 0.0f) {
    ImVec4 color = selected ? BOX_SELECTED : BOX_NORMAL;

    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? BOX_SELECTED : BOX_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, BOX_SELECTED);

    bool clicked = (width > 0.0f)
        ? ImGui::Button(label, ImVec2(width, 0))
        : ImGui::Button(label);

    ImGui::PopStyleColor(3);
    return clicked;
}

static bool custom_vslider(int id, float width, float height, float* value, bool active) {
    ImGui::PushID(id);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    float ball_radius = 5.0f;
    float track_top = pos.y + ball_radius;
    float track_bottom = pos.y + height - ball_radius;
    float track_height = track_bottom - track_top;
    float center_x = pos.x + width * 0.5f;
    float ball_y = track_bottom - (*value) * track_height;

    ImGui::InvisibleButton("##slider", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();

    bool changed = false;

    // Start drag on ball click
    if (hovered && ImGui::IsMouseClicked(0)) {
        ImVec2 mouse = ImGui::GetMousePos();
        float dist = fabsf(mouse.y - ball_y);
        if (dist <= ball_radius + 4.0f) {
            dragging_slider = id;
        }
    }

    // Handle drag
    if (dragging_slider == id && held) {
        ImVec2 mouse = ImGui::GetMousePos();
        float new_value = (track_bottom - mouse.y) / track_height;
        new_value = new_value < 0.0f ? 0.0f : (new_value > 1.0f ? 1.0f : new_value);
        if (new_value != *value) {
            *value = new_value;
            changed = true;
        }
    }

    // Release drag
    if (!held && dragging_slider == id) {
        dragging_slider = -1;
    }

    // Draw track
    ImU32 line_col = active ? SLIDER_LINE_ACTIVE : SLIDER_LINE_INACTIVE;
    draw->AddLine(ImVec2(center_x, track_top), ImVec2(center_x, track_bottom), line_col, 2.0f);

    // Draw ball
    ImU32 ball_col = active ? SLIDER_BALL_ACTIVE : SLIDER_BALL_INACTIVE;
    ball_y = track_bottom - (*value) * track_height;
    draw->AddCircleFilled(ImVec2(center_x, ball_y), ball_radius, ball_col);

    ImGui::PopID();
    return changed;
}

// Map selection to dial index
static int get_active_dial(int subject, int module, int detail) {
    switch (subject) {
        case 0: return module;                    // ColorCorrection: 0-2
        case 1: return (module < 3) ? (3 + module) : (6 + detail);  // ToneMapping: 3-7
        case 2: return 8 + module;                // GlobalColor: 8-10
        case 3: return 11 + module * 3 + detail;  // SelectiveColour: 11-34
        case 4: return 35 + module;               // Detail: 35-38
        default: return 0;
    }
}

// Reverse mapping
static void dial_to_selection(int dial, int& subject, int& module, int& detail) {
    if (dial < 3) {
        subject = 0; module = dial; detail = 0;
    } else if (dial < 8) {
        subject = 1;
        if (dial < 6) {
            module = dial - 3; detail = 0;
        } else {
            module = 0; detail = dial - 6;
        }
    } else if (dial < 11) {
        subject = 2; module = dial - 8; detail = 0;
    } else if (dial < 35) {
        subject = 3;
        int offset = dial - 11;
        module = offset / 3;
        detail = offset % 3;
    } else {
        subject = 4; module = dial - 35; detail = 0;
    }
}

// ============================================================
// Main Render Function
// ============================================================

bool render_module_menus(State& state) {
    bool changed = false;
    bool has_detail = (state.selection.module == 1 || state.selection.module == 3);

    ImGui::Text("Select:");
    ImGui::SameLine();

    // --- Subject section ---
    if (expanding == 1) {
        for (int s = 0; s < SUBJECT_COUNT; s++) {
            ImGui::PushID(s);
            if (s > 0) ImGui::SameLine();
            if (render_box(SUBJECT_NAMES[s], state.selection.module == s)) {
                state.selection.module = s;
                state.selection.dial = 0;
                detail_sel = 0;
                expanding = 0;
                changed = true;
            }
            ImGui::PopID();
        }
    } else {
        ImGui::PushID(0);
        if (render_box(SUBJECT_NAMES[state.selection.module], true)) {
            expanding = 1;
        }
        ImGui::PopID();
    }

    ImGui::SameLine();
    ImGui::Text(">");
    ImGui::SameLine();

    // --- Module section ---
    if (expanding == 2) {
        int mod_count = MODULE_COUNTS[state.selection.module];
        const char** mod_names = MODULE_NAMES[state.selection.module];
        for (int m = 0; m < mod_count; m++) {
            ImGui::PushID(100 + m);
            if (m > 0) ImGui::SameLine();
            if (render_box(mod_names[m], state.selection.dial == m)) {
                state.selection.dial = m;
                expanding = 0;
                changed = true;
            }
            ImGui::PopID();
        }
    } else {
        ImGui::PushID(1);
        const char** mod_names = MODULE_NAMES[state.selection.module];
        if (render_box(mod_names[state.selection.dial], true)) {
            expanding = 2;
        }
        ImGui::PopID();
    }

    // --- Detail section ---
    if (has_detail) {
        ImGui::SameLine();
        ImGui::Text(">");
        ImGui::SameLine();

        if (expanding == 3) {
            if (state.selection.module == 1) {
                for (int c = 0; c < 2; c++) {
                    ImGui::PushID(200 + c);
                    if (c > 0) ImGui::SameLine();
                    if (render_box(DETAIL_TONE[c], detail_sel == c)) {
                        detail_sel = c;
                        expanding = 0;
                        changed = true;
                    }
                    ImGui::PopID();
                }
            } else {
                for (int h = 0; h < 3; h++) {
                    ImGui::PushID(300 + h);
                    if (h > 0) ImGui::SameLine();
                    if (render_box(DETAIL_SEL[h], detail_sel == h)) {
                        detail_sel = h;
                        expanding = 0;
                        changed = true;
                    }
                    ImGui::PopID();
                }
            }
        } else {
            ImGui::PushID(2);
            const char* detail_name = (state.selection.module == 1)
                ? DETAIL_TONE[detail_sel]
                : DETAIL_SEL[detail_sel];
            if (render_box(detail_name, true)) {
                expanding = 3;
            }
            ImGui::PopID();
        }
    }

    // Show current value
    int active_dial = get_active_dial(state.selection.module, state.selection.dial, detail_sel);
    ImGui::SameLine();
    ImGui::Text(">");
    ImGui::SameLine();
    ImGui::Text("%.2f", all_dials[active_dial]);

    // --- Graphic Equalizer ---
    ImGui::Spacing();

    float avail_width = ImGui::GetContentRegionAvail().x;
    float avail_height = ImGui::GetContentRegionAvail().y;
    float slider_width = (avail_width / DIAL_COUNT) - 2.0f;
    float slider_height = avail_height - 4.0f;
    if (slider_height < 50.0f) slider_height = 50.0f;

    for (int d = 0; d < DIAL_COUNT; d++) {
        bool is_active = (d == active_dial);

        if (custom_vslider(400 + d, slider_width, slider_height, &all_dials[d], is_active)) {
            changed = true;
        }

        // Click to select
        if (!is_active && ImGui::IsItemClicked() && dragging_slider == -1) {
            int new_subject, new_module, new_detail;
            dial_to_selection(d, new_subject, new_module, new_detail);
            state.selection.module = new_subject;
            state.selection.dial = new_module;
            detail_sel = new_detail;
            expanding = 0;
            changed = true;
        }

        if (d < DIAL_COUNT - 1) ImGui::SameLine(0, 2.0f);
    }

    return changed;
}

} // namespace desk
