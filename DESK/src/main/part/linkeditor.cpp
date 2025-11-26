// linkeditor.cpp - Link editor implementation
// Cookie-trail navigation and graphic equalizer for editing Link dial values

#include "linkeditor.hpp"
#include "files.hpp"
#include "imgui.h"
#include <cmath>
#include <cstring>

namespace desk {

// ============================================================
// Constants
// ============================================================

// Short names for cookie menu
static const char* MODULE_SHORT[] = {
    "Geo", "CC", "Tone", "Color", "Select", "Detail"
};

// Dial names per module for cookie menu
static const char* DIAL_NAMES_GEO[] = {"Crop Top", "Crop Right", "Crop Bottom", "Crop Left", "Scale", "Tilt"};
static const char* DIAL_NAMES_CC[] = {"Exposure", "Temperature", "Tint"};
static const char* DIAL_NAMES_TONE[] = {"Contrast", "Highlights", "Shadows", "Black", "White"};
static const char* DIAL_NAMES_COLOR[] = {"Vibrance", "Saturation", "Density"};
static const char* DIAL_NAMES_SEL[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Magenta"};
static const char* DIAL_NAMES_DTL[] = {"Sharp Amt", "Sharp Rad", "Denoise L", "Denoise C"};

// Detail names for selective color
static const char* DETAIL_NAMES[] = {"Hue", "Sat", "Lum"};

// Dial counts per module
static const int DIAL_COUNTS[] = {6, 3, 5, 3, 8, 4};

// Dial keys for each module
static const char* DIAL_KEYS_GEO[] = {"crop_top", "crop_right", "crop_bottom", "crop_left", "scale", "tilt_angle"};
static const char* DIAL_KEYS_CC[] = {"exposure", "temperature", "tint"};
static const char* DIAL_KEYS_TONE[] = {"contrast", "highlights", "shadows", "black", "white"};
static const char* DIAL_KEYS_COLOR[] = {"vibrance", "saturation", "color_density"};
static const char* DIAL_KEYS_DTL[] = {"sharpen_amount", "sharpen_radius", "denoise_luminance", "denoise_chroma"};

// Cookie menu box styling
static const ImVec4 BOX_NORMAL = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
static const ImVec4 BOX_HOVER = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
static const ImVec4 BOX_SELECTED = ImVec4(0.3f, 0.5f, 0.7f, 1.0f);

// Slider styling - three states: grey (default), white (in pipe), blue (hot)
static const ImU32 SLIDER_LINE_GREY = IM_COL32(100, 100, 100, 255);   // Default value - light grey
static const ImU32 SLIDER_LINE_WHITE = IM_COL32(180, 180, 180, 255);  // In pipe (non-default)
static const ImU32 SLIDER_LINE_HOT = IM_COL32(80, 150, 220, 255);     // Hot (selected)
static const ImU32 SLIDER_BALL_GREY = IM_COL32(140, 140, 140, 255);   // Light grey ball
static const ImU32 SLIDER_BALL_WHITE = IM_COL32(240, 240, 240, 255);  // Bright white
static const ImU32 SLIDER_BALL_HOT = IM_COL32(100, 180, 255, 255);    // Blue

// ============================================================
// Module State
// ============================================================

static int dragging_slider = -1;
static int expanding = 0;  // Cookie menu state: 0=collapsed, 1=module, 2=dial, 3=detail

// Undo capture - stores value at drag start
static float drag_start_value = 0.0f;
static int drag_module = -1;
static int drag_dial = -1;
static int drag_detail = -1;

// ============================================================
// Helper: Render cookie menu box
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

// ============================================================
// Helper: Get dial name for a module/dial
// ============================================================

static const char* get_dial_name(int module, int dial) {
    switch (module) {
        case MOD_GEOMETRIC: return (dial < 6) ? DIAL_NAMES_GEO[dial] : "?";
        case MOD_COLOR_CORRECTION: return (dial < 3) ? DIAL_NAMES_CC[dial] : "?";
        case MOD_TONE_MAPPING: return (dial < 5) ? DIAL_NAMES_TONE[dial] : "?";
        case MOD_GLOBAL_COLOR: return (dial < 3) ? DIAL_NAMES_COLOR[dial] : "?";
        case MOD_SELECTIVE_COLOR: return (dial < 8) ? DIAL_NAMES_SEL[dial] : "?";
        case MOD_DETAIL: return (dial < 4) ? DIAL_NAMES_DTL[dial] : "?";
        default: return "?";
    }
}

// ============================================================
// Helper: Get dial value from Link
// ============================================================

static float get_dial_value(const Link& link, int subject, int dial_index, int detail = 0) {
    switch (subject) {
        case MOD_GEOMETRIC:
            if (dial_index < 6) {
                auto it = link.geometric.dials.find(DIAL_KEYS_GEO[dial_index]);
                if (it != link.geometric.dials.end()) return it->second;
                // Default for crop is 0, for zoom/rotation is 0.5
                if (dial_index < 4) return 0.0f;  // crop defaults
            }
            break;
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
        case MOD_GEOMETRIC:
            if (dial_index < 6) {
                link.geometric.dials[DIAL_KEYS_GEO[dial_index]] = value;
            }
            break;
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
// Helper: Check if dial is at default value
// ============================================================

static bool is_default_value(int module, int dial, float value, int detail = 0) {
    // Geometric: crop defaults to 0, scale/tilt to 0.5
    if (module == MOD_GEOMETRIC) {
        if (dial < 4) return value == 0.0f;  // crop_top/right/bottom/left
        return value == 0.5f;  // scale, tilt_angle
    }
    // Tone mapping: black/white have special defaults
    if (module == MOD_TONE_MAPPING) {
        if (dial == 3) return value == 0.15f;  // black
        if (dial == 4) return value == 0.85f;  // white
    }
    // Detail: most default to 0, sharpen_radius defaults to 0.4
    if (module == MOD_DETAIL) {
        if (dial == 1) return value == 0.4f;  // sharpen_radius
        return value == 0.0f;  // sharpen_amount, denoise_luminance, denoise_chroma
    }
    // Default is 0.5 for all other dials
    return value == 0.5f;
}

// ============================================================
// Helper: Custom vertical slider
// ============================================================

// Slider result: bit 0 = value changed, bit 1 = drag ended, bit 2 = clicked
static constexpr int SLIDER_CHANGED = 1;
static constexpr int SLIDER_RELEASED = 2;
static constexpr int SLIDER_CLICKED = 4;

// Dial state: 0 = grey (default), 1 = white (in pipe), 2 = hot (selected)
enum DialState { DIAL_GREY = 0, DIAL_WHITE = 1, DIAL_HOT = 2 };

static int custom_vslider(int id, float width, float height, float* value, DialState dial_state) {
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

    int result = 0;

    // Start drag on any click in slider area - jump to click position
    if (hovered && ImGui::IsMouseClicked(0)) {
        dragging_slider = id;
        result |= SLIDER_CLICKED;
        // Immediately update value to click position
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

    // Draw track - color based on dial state
    ImU32 line_col, ball_col;
    switch (dial_state) {
        case DIAL_HOT:
            line_col = SLIDER_LINE_HOT;
            ball_col = SLIDER_BALL_HOT;
            break;
        case DIAL_WHITE:
            line_col = SLIDER_LINE_WHITE;
            ball_col = SLIDER_BALL_WHITE;
            break;
        default:
            line_col = SLIDER_LINE_GREY;
            ball_col = SLIDER_BALL_GREY;
            break;
    }
    draw->AddLine(ImVec2(center_x, track_top), ImVec2(center_x, track_bottom), line_col, 2.0f);

    // Draw ball
    ball_y = track_bottom - (*value) * track_height;
    draw->AddCircleFilled(ImVec2(center_x, ball_y), ball_radius, ball_col);

    ImGui::PopID();
    return result;
}

// ============================================================
// Main Render Function - All 45 dials horizontal
// ============================================================

// Total dials: 6 + 3 + 5 + 3 + 24 + 4 = 45
static constexpr int TOTAL_DIALS = 45;

// Structure to map flat dial index to module/dial/detail
struct DialMapping {
    int module;
    int dial;
    int detail;  // Only used for selective color
    const char* label;
};

// Build dial mapping table
static DialMapping g_dial_map[TOTAL_DIALS];
static bool g_dial_map_init = false;

static void init_dial_map() {
    if (g_dial_map_init) return;
    int idx = 0;

    // Geometric (6)
    const char* geo_labels[] = {"CrT", "CrR", "CrB", "CrL", "Scl", "Tlt"};
    for (int d = 0; d < 6; d++) {
        g_dial_map[idx++] = {MOD_GEOMETRIC, d, -1, geo_labels[d]};
    }
    // Color Correction (3)
    const char* cc_labels[] = {"Exp", "Tmp", "Tnt"};
    for (int d = 0; d < 3; d++) {
        g_dial_map[idx++] = {MOD_COLOR_CORRECTION, d, -1, cc_labels[d]};
    }
    // Tone Mapping (5)
    const char* tone_labels[] = {"Con", "Hi", "Sha", "Blk", "Wht"};
    for (int d = 0; d < 5; d++) {
        g_dial_map[idx++] = {MOD_TONE_MAPPING, d, -1, tone_labels[d]};
    }
    // Global Color (3)
    const char* gc_labels[] = {"Vib", "Sat", "Den"};
    for (int d = 0; d < 3; d++) {
        g_dial_map[idx++] = {MOD_GLOBAL_COLOR, d, -1, gc_labels[d]};
    }
    // Selective Color (24 = 8 colors × 3 HSL)
    // Pre-allocated labels for selective color
    static const char* sel_labels[24] = {
        "RH", "RS", "RL", "OH", "OS", "OL", "YH", "YS", "YL", "GH", "GS", "GL",
        "CH", "CS", "CL", "BH", "BS", "BL", "PH", "PS", "PL", "MH", "MS", "ML"
    };
    for (int c = 0; c < 8; c++) {
        for (int h = 0; h < 3; h++) {
            g_dial_map[idx++] = {MOD_SELECTIVE_COLOR, c, h, sel_labels[c * 3 + h]};
        }
    }
    // Detail (4)
    const char* dtl_labels[] = {"ShA", "ShR", "DnL", "DnC"};
    for (int d = 0; d < 4; d++) {
        g_dial_map[idx++] = {MOD_DETAIL, d, -1, dtl_labels[d]};
    }

    g_dial_map_init = true;
}

bool render_module_menus(State& state) {
    init_dial_map();

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

    // Get current navigation state (module/dial/detail for cookie menu)
    int sel_module = state.selection.module;
    int sel_dial = state.selection.dial;
    int sel_detail = state.selection.detail;

    // Clamp to valid range
    if (sel_module < MOD_GEOMETRIC || sel_module > MOD_DETAIL) sel_module = MOD_COLOR_CORRECTION;
    if (sel_dial < 0 || sel_dial >= DIAL_COUNTS[sel_module]) sel_dial = 0;
    if (sel_detail < 0) sel_detail = 0;

    bool has_detail = (sel_module == MOD_SELECTIVE_COLOR);

    // --- Cookie menu breadcrumb navigation ---
    ImGui::Text("Select:");
    ImGui::SameLine();

    // --- Module section ---
    if (expanding == 1) {
        // Show all modules (skip Geometric - index 0)
        for (int m = MOD_COLOR_CORRECTION; m <= MOD_DETAIL; m++) {
            ImGui::PushID(m);
            if (m > MOD_COLOR_CORRECTION) ImGui::SameLine();
            if (render_box(MODULE_SHORT[m], sel_module == m)) {
                // Navigate to module (don't make hot)
                state.selection.module = m;
                state.selection.dial = 0;
                state.selection.detail = (m == MOD_SELECTIVE_COLOR) ? 0 : 0;
                expanding = 0;
                changed = true;
            }
            ImGui::PopID();
        }
    } else {
        ImGui::PushID(0);
        if (render_box(MODULE_SHORT[sel_module], true)) {
            expanding = 1;
        }
        ImGui::PopID();
    }

    ImGui::SameLine();
    ImGui::Text(">");
    ImGui::SameLine();

    // --- Dial section ---
    if (expanding == 2) {
        int dial_count = DIAL_COUNTS[sel_module];
        for (int d = 0; d < dial_count; d++) {
            ImGui::PushID(100 + d);
            if (d > 0) ImGui::SameLine();
            if (render_box(get_dial_name(sel_module, d), sel_dial == d)) {
                // Navigate to dial (don't make hot)
                state.selection.dial = d;
                expanding = 0;
                changed = true;
            }
            ImGui::PopID();
        }
    } else {
        ImGui::PushID(1);
        if (render_box(get_dial_name(sel_module, sel_dial), true)) {
            expanding = 2;
        }
        ImGui::PopID();
    }

    // --- Detail section (only for Selective Color) ---
    if (has_detail) {
        ImGui::SameLine();
        ImGui::Text(">");
        ImGui::SameLine();

        if (expanding == 3) {
            for (int h = 0; h < 3; h++) {
                ImGui::PushID(200 + h);
                if (h > 0) ImGui::SameLine();
                if (render_box(DETAIL_NAMES[h], sel_detail == h)) {
                    // Navigate to detail (don't make hot)
                    state.selection.detail = h;
                    expanding = 0;
                    changed = true;
                }
                ImGui::PopID();
            }
        } else {
            ImGui::PushID(2);
            if (render_box(DETAIL_NAMES[sel_detail], true)) {
                expanding = 3;
            }
            ImGui::PopID();
        }
    }

    // --- Show current value ---
    ImGui::SameLine();
    ImGui::Text(">");
    ImGui::SameLine();
    float current_value = get_dial_value(link, sel_module, sel_dial, sel_detail);
    ImGui::Text("%.2f", current_value);

    // --- Reset button (far right) ---
    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
    if (ImGui::SmallButton("Reset")) {
        // Clear all dials in the link
        link.geometric.dials.clear();
        link.color_correction.dials.clear();
        link.tone_mapping.dials.clear();
        link.global_color.dials.clear();
        link.selective_color.dials.clear();
        link.detail.dials.clear();

        // Re-initialize with defaults
        init_geometric(link.geometric);
        init_color_correction(link.color_correction);
        init_tone_mapping(link.tone_mapping);
        init_global_color(link.global_color);
        init_selective_color(link.selective_color);
        init_detail(link.detail);

        // Clear hot selection
        state.selection.clear_hot();

        // Save and reprocess
        save_pipe_json(proj);
        state.needs_reprocess = true;
        changed = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Reset all dials to defaults");
    }

    // --- All 45 dials as horizontal equalizer ---
    ImGui::Spacing();

    float avail_width = ImGui::GetContentRegionAvail().x;
    float avail_height = ImGui::GetContentRegionAvail().y;

    // Calculate slider dimensions to fit all 45 dials
    float spacing = 2.0f;
    float slider_width = (avail_width - (TOTAL_DIALS - 1) * spacing) / TOTAL_DIALS;
    if (slider_width < 8.0f) slider_width = 8.0f;
    float slider_height = avail_height - 16.0f;  // Leave room for labels
    if (slider_height < 40.0f) slider_height = 40.0f;

    // Render all 45 sliders
    for (int i = 0; i < TOTAL_DIALS; i++) {
        const auto& dm = g_dial_map[i];

        // Get current value
        float value = get_dial_value(link, dm.module, dm.dial, dm.detail);

        // Determine dial state
        DialState dial_state;
        if (state.selection.is_hot(dm.module, dm.dial, dm.detail)) {
            dial_state = DIAL_HOT;
        } else if (!is_default_value(dm.module, dm.dial, value, dm.detail)) {
            dial_state = DIAL_WHITE;
        } else {
            dial_state = DIAL_GREY;
        }

        ImGui::PushID(i);

        int slider_result = custom_vslider(i, slider_width, slider_height, &value, dial_state);

        // Click on slider makes it hot (this is a selection change)
        if (slider_result & SLIDER_CLICKED) {
            state.selection.set_hot(dm.module, dm.dial, dm.detail);
            // Update navigation to match hot
            state.selection.module = dm.module;
            state.selection.dial = dm.dial;
            state.selection.detail = dm.detail >= 0 ? dm.detail : 0;
            changed = true;  // Selection changed

            // Capture value for undo
            drag_start_value = get_dial_value(link, dm.module, dm.dial, dm.detail);
            drag_module = dm.module;
            drag_dial = dm.dial;
            drag_detail = dm.detail;
        }

        // Value changed during drag - just update data, don't signal selection change
        if (slider_result & SLIDER_CHANGED) {
            set_dial_value(link, dm.module, dm.dial, value, dm.detail);
            // NOT setting changed - this is just a value update, not selection
        }

        if (slider_result & SLIDER_RELEASED) {
            // Push to undo stack if value actually changed
            float final_value = get_dial_value(link, drag_module, drag_dial, drag_detail);
            if (drag_module >= 0 && final_value != drag_start_value) {
                UndoEntry entry;
                entry.project = state.selection.project;
                entry.link = state.selection.link;
                entry.module = drag_module;
                entry.dial = drag_dial;
                entry.detail = drag_detail;
                entry.old_value = drag_start_value;
                entry.new_value = final_value;
                state.push_undo(entry);
            }
            drag_module = -1;  // Clear drag state

            save_pipe_json(proj);
            state.needs_reprocess = true;  // Only reprocess on release
        }

        ImGui::PopID();

        if (i < TOTAL_DIALS - 1) ImGui::SameLine(0, spacing);
    }

    return changed;
}

// ============================================================
// Apply Undo Entry
// ============================================================

void apply_undo(State& state, const UndoEntry& entry) {
    // Validate project and link still exist
    if (entry.project < 0 || entry.project >= static_cast<int>(state.projects.size())) return;
    Project& proj = state.projects[entry.project];
    if (entry.link < 0 || entry.link >= static_cast<int>(proj.links.size())) return;
    Link& link = proj.links[entry.link];

    // Restore the old value
    set_dial_value(link, entry.module, entry.dial, entry.old_value, entry.detail);

    // Save and reprocess
    save_pipe_json(proj);
    state.needs_reprocess = true;

    // Navigate to the undone dial
    state.selection.project = entry.project;
    state.selection.link = entry.link;
    state.selection.module = entry.module;
    state.selection.dial = entry.dial;
    state.selection.detail = entry.detail >= 0 ? entry.detail : 0;
    state.selection.set_hot(entry.module, entry.dial, entry.detail);
}

} // namespace desk
