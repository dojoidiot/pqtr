// state.hpp - DESK application state
// Internal state management for the DESK application

#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace desk {

// ============================================================
// Type Aliases
// ============================================================

using Dial = float;                               // Normalized dial value (0.0 - 1.0)
using Info = std::map<std::string, std::string>;  // Metadata key-value pairs

// ============================================================
// Module - Processing unit with named dials
// ============================================================

struct Module {
    std::string name;
    std::map<std::string, Dial> dials;
};

// ============================================================
// Link - Named collection of 6 modules (45 dials total)
// ============================================================

struct Link {
    std::string name;
    bool editing_name = false;

    // 6 golden modules
    Module geometric;        // 6 dials: crop (4), zoom (1), rotation (1)
    Module color_correction; // 3 dials: exposure (1), white balance (2)
    Module tone_mapping;     // 5 dials: contrast (1), curve (2), clip (2)
    Module global_color;     // 3 dials: vibrance, saturation, density
    Module selective_color;  // 24 dials: 8 colors x 3 HSL
    Module detail;           // 4 dials: sharpen (2), denoise (2)

    explicit Link(const std::string& n = "New Link");
};

// ============================================================
// Project - RAW file with sidecars
// ============================================================

struct Project {
    std::string name;                     // File name without extension
    std::filesystem::path raw_path;       // Full path to .ARW
    std::filesystem::path desk_path;      // .desk.json
    std::filesystem::path pipe_path;      // .pipe.json
    std::filesystem::path png_path;       // Output .png

    bool hidden = false;
    bool expanded = false;
    std::string decoder = "sony_arw2";
    std::string tail_output;
    std::vector<Link> links;
};

// ============================================================
// Panel Visibility
// ============================================================

struct PanelVisibility {
    bool pipe = false;         // Pipe tree (Links/Modules/Dials) - top left
    bool info = false;         // RAW metadata - bottom left
    bool link_editor = false;  // Slider equalizer - bottom right
    bool embedded = false;     // Embedded camera preview - top right
};

// ============================================================
// Texture - OpenGL texture handle
// ============================================================

struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
    bool loaded = false;

    void reset() {
        id = 0;
        width = 0;
        height = 0;
        loaded = false;
    }
};

// ============================================================
// Selection - Current UI selection state
// ============================================================

struct Selection {
    int project = -1;       // Selected project index (-1 = none)
    int link = -1;          // Selected link index (-1 = none)
    int module = 0;         // Selected module (0-5)
    int dial = 0;           // Selected dial within module
};

// ============================================================
// State - Complete application state
// ============================================================

struct State {
    // Project folder (where projects are stored: DESK/var/)
    std::filesystem::path project_folder;
    bool project_folder_set = false;

    // RAW source folder (where RAWs are browsed from: LABS/var/pics/)
    std::filesystem::path raw_source_folder;

    // Projects
    std::vector<Project> projects;

    // Selection
    Selection selection;

    // Panel visibility
    PanelVisibility panels;

    // Image textures
    Texture texture;           // Main rendered image
    Texture embedded_texture;  // Embedded camera preview
    bool has_embedded = false; // True if RAW has embedded preview

    // RAW metadata
    Info raw_info;

    // Render settings
    int working_size = 1024;       // Max dimension for preview renders (0 = full)
    static constexpr int WORKING_SIZES[] = {512, 1024, 2048, 4096, 0};
    static constexpr int WORKING_SIZE_COUNT = 5;

    // Status
    bool needs_refresh = false;
    bool needs_reprocess = false;
    bool needs_export = false;     // Full-size export requested
    bool is_working = false;       // True while rendering
    std::string status_message;
    std::string error_message;

    // Check if a project is currently open
    bool has_project() const {
        return selection.project >= 0 &&
               selection.project < static_cast<int>(projects.size());
    }

    // Get current project (valid only if has_project() is true)
    Project& current_project() {
        return projects[selection.project];
    }

    const Project& current_project() const {
        return projects[selection.project];
    }
};

// ============================================================
// Module identifiers
// ============================================================

enum ModuleId {
    MOD_COLOR_CORRECTION = 0,
    MOD_TONE_MAPPING,
    MOD_GLOBAL_COLOR,
    MOD_SELECTIVE_COLOR,
    MOD_DETAIL,
    MOD_COUNT
};

// ============================================================
// Module initialization functions
// ============================================================

void init_geometric(Module& m);
void init_color_correction(Module& m);
void init_tone_mapping(Module& m);
void init_global_color(Module& m);
void init_selective_color(Module& m);
void init_detail(Module& m);

} // namespace desk
