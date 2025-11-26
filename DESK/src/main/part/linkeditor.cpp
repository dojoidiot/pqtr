// linkeditor.cpp - Link editor implementation
// Graphic equalizer for editing Link dial values

#include "linkeditor.hpp"
#include "files.hpp"
#include "imgui.h"
#include <cmath>
#include <cstring>

namespace desk {

// ============================================================
// Constants
// ============================================================

// Subject names (5 modules - Geometric hidden)
static const char* SUBJECT_NAMES[] = {
    "Color Correction", "Tone Mapping", "Global Color", "Selective Colour", "Detail"
};

// Module dial names per subject
static const char* MODULE_NAMES_CC[] = {"Exposure", "Temperature", "Tint"};
static const char* MODULE_NAMES_TONE[] = {"Contrast", "Highlights", "Shadows", "Black", "White"};
static const char* MODULE_NAMES_COLOR[] = {"Vibrance", "Saturation", "Density"};
static const char* MODULE_NAMES_SEL[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Magenta"};
static const char* MODULE_NAMES_DTL[] = {"Sharp Amt", "Sharp Rad", "Denoise L", "Denoise C"};

static const char** MODULE_NAMES[] = {
    MODULE_NAMES_CC,
    MODULE_NAMES_TONE,
    MODULE_NAMES_COLOR,
    MODULE_NAMES_SEL,
    MODULE_NAMES_DTL
};

static const int MODULE_COUNTS[] = {3, 5, 3, 8, 4};
static constexpr int SUBJECT_COUNT = 5;

// Detail menus for selective color
static const char* DETAIL_SEL[] = {"Hue", "Saturation", "Luminance"};

// Dial keys for each module
static const char* DIAL_KEYS_CC[] = {"exposure", "temperature", "tint"};
static const char* DIAL_KEYS_TONE[] = {"contrast", "highlights", "shadows", "black", "white"};
static const char* DIAL_KEYS_COLOR[] = {"vibrance", "saturation", "color_density"};
static const char* DIAL_KEYS_DTL[] = {"sharpen_amount", "sharpen_radius", "denoise_luminance", "denoise_chroma"};

// Slider styling - three states: default (grey), set (white), active (blue)
static const ImU32 SLIDER_LINE_DEFAULT = IM_COL32(60, 60, 60, 255);     // Grey - at neutral
static const ImU32 SLIDER_LINE_SET = IM_COL32(120, 120, 120, 255);      // White-ish - modified
static const ImU32 SLIDER_LINE_ACTIVE = IM_COL32(80, 130, 180, 255);    // Blue - selected
static const ImU32 SLIDER_BALL_DEFAULT = IM_COL32(80, 80, 80, 255);     // Grey
static const ImU32 SLIDER_BALL_SET = IM_COL32(200, 200, 200, 255);      // White
static const ImU32 SLIDER_BALL_ACTIVE = IM_COL32(100, 180, 255, 255);   // Blue

// Total visible dials (21 = 3+5+3+3+4+3 for selective)
// For selective color we show 3 dials for the selected color
static constexpr int MAX_DIALS = 21;

// ============================================================
// Module State
// ============================================================

static int expanding = 0;      // 0=collapsed, 1=subject, 2=module, 3=detail
static int detail_sel = 0;     // Sub-dial selection for selective color
static int dragging_slider = -1;

// ============================================================
// Helper: Get default/neutral value for a dial
// ============================================================

static float get_dial_default(int subject, int dial_index, int detail = 0) {
    switch (subject) {
        case MOD_COLOR_CORRECTION:
            return 0.5f;  // All center-neutral
        case MOD_TONE_MAPPING:
            if (dial_index == 3) return 0.15f;  // black
            if (dial_index == 4) return 0.85f;  // white
            return 0.5f;  // contrast, highlights, shadows
        case MOD_GLOBAL_COLOR:
            return 0.5f;  // All center-neutral
        case MOD_SELECTIVE_COLOR:
            return 0.5f;  // All center-neutral
        case MOD_DETAIL:
            if (dial_index == 0) return 0.0f;  // sharpen_amount
            if (dial_index == 1) return 0.4f;  // sharpen_radius
            if (dial_index == 2) return 0.0f;  // denoise_luminance
            if (dial_index == 3) return 0.0f;  // denoise_chroma
            return 0.5f;
    }
    return 0.5f;
}

// ============================================================
// Helper: Check if dial is set (different from default)
// ============================================================

static bool is_dial_set(float value, float default_value) {
    return std::abs(value - default_value) > 0.001f;
}

// ============================================================
// Helper: Get dial value from Link
// ============================================================

static float get_dial_value(const Link& link, int subject, int dial_index, int detail = 0) {
    switch (subject) {
        case MOD_COLOR_CORRECTION:
            if (dial_index < 3) {
                auto it = link.color_correction.dials.find(DIAL_KEYS_CC[dial_index]);
                if (it != link.color_correction.dials.end()) return it->second;
            }
            break;
        case MOD_TONE_MAPPING:
            if (dial_index < 5) {
                auto it = link.tone_mapping.dials.find(DIAL_KEYS_TONE[dial_index]);
                if (it != link.tone_mapping.dials.end()) return it->second;
            }
            break;
        case MOD_GLOBAL_COLOR:
            if (dial_index < 3) {
                auto it = link.global_color.dials.find(DIAL_KEYS_COLOR[dial_index]);
                if (it != link.global_color.dials.end()) return it->second;
            }
            break;
        case MOD_SELECTIVE_COLOR: {
            const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
            const char* attrs[] = {"_hue", "_saturation", "_luminance"};
            if (dial_index < 8 && detail < 3) {
                std::string key = std::string(colors[dial_index]) + attrs[detail];
                auto it = link.selective_color.dials.find(key);
                if (it != link.selective_color.dials.end()) return it->second;
            }
            break;
        }
        case MOD_DETAIL:
            if (dial_index < 4) {
                auto it = link.detail.dials.find(DIAL_KEYS_DTL[dial_index]);
                if (it != link.detail.dials.end()) return it->second;
            }
            break;
    }
    return 0.5f;
}

// ============================================================
// Helper: Set dial value in Link
// ============================================================

static void set_dial_value(Link& link, int subject, int dial_index, float value, int detail = 0) {
    switch (subject) {
        case MOD_COLOR_CORRECTION:
            if (dial_index < 3) {
                link.color_correction.dials[DIAL_KEYS_CC[dial_index]] = value;
            }
            break;
        case MOD_TONE_MAPPING:
            if (dial_index < 5) {
                link.tone_mapping.dials[DIAL_KEYS_TONE[dial_index]] = value;
            }
            break;
        case MOD_GLOBAL_COLOR:
            if (dial_index < 3) {
                link.global_color.dials[DIAL_KEYS_COLOR[dial_index]] = value;
            }
            break;
        case MOD_SELECTIVE_COLOR: {
            const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
            const char* attrs[] = {"_hue", "_saturation", "_luminance"};
            if (dial_index < 8 && detail < 3) {
                std::string key = std::string(colors[dial_index]) + attrs[detail];
                link.selective_color.dials[key] = value;
            }
            break;
        }
        case MOD_DETAIL:
            if (dial_index < 4) {
                link.detail.dials[DIAL_KEYS_DTL[dial_index]] = value;
            }
            break;
    }
}

// ============================================================
// Helper: Custom vertical slider
// ============================================================

// Slider result: bit 0 = value changed, bit 1 = drag ended
static constexpr int SLIDER_CHANGED = 1;
static constexpr int SLIDER_RELEASED = 2;

// Slider result flags
static constexpr int SLIDER_CLICKED = 4;  // Slider was clicked (for selection)

static int custom_vslider(int id, float width, float height, float* value, bool active, bool is_set) {
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
    bool held = ImGui::IsItemActive();
    bool clicked = ImGui::IsItemClicked();

    int result = 0;

    // Any click on slider reports clicked (for selection)
    if (clicked) {
        result |= SLIDER_CLICKED;
        dragging_slider = id;

        // Jump to click position
        ImVec2 mouse = ImGui::GetMousePos();
        float new_value = (track_bottom - mouse.y) / track_height;
        new_value = new_value < 0.0f ? 0.0f : (new_value > 1.0f ? 1.0f : new_value);
        if (new_value != *value) {
            *value = new_value;
            result |= SLIDER_CHANGED;
        }
    }

    // Handle drag
    if (dragging_slider == id && held) {
        ImVec2 mouse = ImGui::GetMousePos();
        float new_value = (track_bottom - mouse.y) / track_height;
        new_value = new_value < 0.0f ? 0.0f : (new_value > 1.0f ? 1.0f : new_value);
        if (new_value != *value) {
            *value = new_value;
            result |= SLIDER_CHANGED;
        }
    }

    // Release drag
    if (!held && dragging_slider == id) {
        dragging_slider = -1;
        result |= SLIDER_RELEASED;
    }

    // Choose colors based on state: active (blue) > set (white) > default (grey)
    ImU32 line_col, ball_col;
    if (active) {
        line_col = SLIDER_LINE_ACTIVE;
        ball_col = SLIDER_BALL_ACTIVE;
    } else if (is_set) {
        line_col = SLIDER_LINE_SET;
        ball_col = SLIDER_BALL_SET;
    } else {
        line_col = SLIDER_LINE_DEFAULT;
        ball_col = SLIDER_BALL_DEFAULT;
    }

    // Draw track
    draw->AddLine(ImVec2(center_x, track_top), ImVec2(center_x, track_bottom), line_col, 2.0f);

    // Draw ball
    ball_y = track_bottom - (*value) * track_height;
    draw->AddCircleFilled(ImVec2(center_x, ball_y), ball_radius, ball_col);

    ImGui::PopID();
    return result;
}

// ============================================================
// Helper: Render selection box
// ============================================================

static const ImVec4 BOX_NORMAL = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
static const ImVec4 BOX_HOVER = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
static const ImVec4 BOX_SELECTED = ImVec4(0.3f, 0.5f, 0.7f, 1.0f);

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

// ============================================================
// Main Render Function
// ============================================================

bool render_module_menus(State& state) {
    // Check if we have a valid link selected
    if (!state.has_project()) {
        ImGui::TextDisabled("Select a RAW file first");
        return false;
    }

    Project& proj = state.current_project();

    if (state.selection.link < 0 || state.selection.link >= static_cast<int>(proj.links.size())) {
        ImGui::TextDisabled("Select a Link to edit");
        return false;
    }

    Link& link = proj.links[state.selection.link];
    bool changed = false;

    // Determine if we need detail menu (only for Selective Color)
    bool has_detail = (state.selection.module == MOD_SELECTIVE_COLOR);

    ImGui::Text("Module:");
    ImGui::SameLine();

    // --- Subject/Module selection ---
    if (expanding == 1) {
        for (int s = 0; s < SUBJECT_COUNT; s++) {
            ImGui::PushID(s);
            if (s > 0) ImGui::SameLine();
            if (render_box(SUBJECT_NAMES[s], state.selection.module == s)) {
                state.selection.module = s;
                state.selection.dial = 0;
                detail_sel = 0;
                expanding = 0;
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

    // --- Dial selection ---
    if (expanding == 2) {
        int mod_count = MODULE_COUNTS[state.selection.module];
        const char** mod_names = MODULE_NAMES[state.selection.module];
        for (int m = 0; m < mod_count; m++) {
            ImGui::PushID(100 + m);
            if (m > 0) ImGui::SameLine();
            if (render_box(mod_names[m], state.selection.dial == m)) {
                state.selection.dial = m;
                expanding = 0;
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

    // --- Detail selection (Selective Color only) ---
    if (has_detail) {
        ImGui::SameLine();
        ImGui::Text(">");
        ImGui::SameLine();

        if (expanding == 3) {
            for (int h = 0; h < 3; h++) {
                ImGui::PushID(300 + h);
                if (h > 0) ImGui::SameLine();
                if (render_box(DETAIL_SEL[h], detail_sel == h)) {
                    detail_sel = h;
                    expanding = 0;
                }
                ImGui::PopID();
            }
        } else {
            ImGui::PushID(2);
            if (render_box(DETAIL_SEL[detail_sel], true)) {
                expanding = 3;
            }
            ImGui::PopID();
        }
    }

    // Show current value
    float current_value = get_dial_value(link, state.selection.module, state.selection.dial, detail_sel);
    ImGui::SameLine();
    ImGui::Text("= %.2f", current_value);

    // --- Graphic Equalizer ---
    ImGui::Spacing();

    // Determine how many sliders to show based on module
    int dial_count = MODULE_COUNTS[state.selection.module];

    float avail_width = ImGui::GetContentRegionAvail().x;
    float avail_height = ImGui::GetContentRegionAvail().y;
    float slider_width = (avail_width / dial_count) - 4.0f;
    float slider_height = avail_height - 20.0f;
    if (slider_height < 50.0f) slider_height = 50.0f;
    if (slider_width < 20.0f) slider_width = 20.0f;

    // Render sliders for current module
    for (int d = 0; d < dial_count; d++) {
        bool is_active = (d == state.selection.dial);

        // Get current value and default for this dial
        float value = get_dial_value(link, state.selection.module, d, detail_sel);
        float default_value = get_dial_default(state.selection.module, d, detail_sel);
        bool dial_is_set = is_dial_set(value, default_value);

        ImGui::PushID(400 + d);

        int slider_result = custom_vslider(d, slider_width, slider_height, &value, is_active, dial_is_set);

        // Click to select this dial (make it hot)
        if (slider_result & SLIDER_CLICKED) {
            state.selection.dial = d;
        }

        if (slider_result & SLIDER_CHANGED) {
            // Value changed during drag - update link data and trigger live re-render
            set_dial_value(link, state.selection.module, d, value, detail_sel);
            state.needs_reprocess = true;
            state.is_working = true;
            changed = true;
        }

        if (slider_result & SLIDER_RELEASED) {
            // Drag ended - save the final value to disk
            save_pipe_json(proj);
        }

        // Label under slider
        ImGui::SetCursorPosX(ImGui::GetCursorPosX());
        const char** names = MODULE_NAMES[state.selection.module];
        // Truncate long names
        char short_name[8];
        strncpy(short_name, names[d], 6);
        short_name[6] = '\0';
        ImGui::TextDisabled("%s", short_name);

        ImGui::PopID();

        if (d < dial_count - 1) ImGui::SameLine(0, 4.0f);
    }

    return changed;
}

} // namespace desk
