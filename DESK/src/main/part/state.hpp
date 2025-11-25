// state.hpp - DESK application state
// Holds all runtime state for the DESK application

#pragma once

#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace desk {

// Forward declarations
struct Project;
struct Link;
struct Module;

// Dial value (normalized 0.0-1.0)
using Dial = float;

// Module with dials
struct Module {
    std::string name;
    std::map<std::string, Dial> dials;
};

// Link (named collection of 6 modules)
struct Link {
    std::string name;
    bool editing_name = false;

    // 6 modules per link
    Module geometric;
    Module color_correction;
    Module tone_mapping;
    Module global_color;
    Module selective_color;
    Module detail;

    Link(const std::string& n = "New Link");
};

// Project (RAW file with sidecars)
struct Project {
    std::string name;              // File name without extension
    std::filesystem::path raw_path; // Full path to .ARW
    std::filesystem::path desk_path; // .desk.json
    std::filesystem::path pipe_path; // .pipe.json
    std::filesystem::path png_path;  // Output .png

    bool hidden = false;
    bool expanded = false;         // Tree node state
    std::string decoder = "sony_arw2";
    std::vector<Link> links;
    std::string tail_output;       // Output filename from tail section
};

// Application state
struct State {
    // Root folder
    std::filesystem::path root_folder;
    bool root_folder_set = false;

    // Projects
    std::vector<Project> projects;
    int selected_project = -1;
    int selected_link = -1;

    // Image texture
    unsigned int texture_id = 0;
    int texture_width = 0;
    int texture_height = 0;
    bool texture_loaded = false;

    // UI state
    bool needs_refresh = false;
    bool needs_reprocess = false;
    std::string status_message;
    std::string error_message;

    // Link Editor state
    int selected_module = 0;   // 0-5: EXP, WB, TONE, COLOR, SEL, DTL
    int selected_dial = 0;     // Index within module's dial list
};

// Module identifiers (6 golden modules - no geometric)
enum ModuleId {
    MOD_EXP = 0,
    MOD_WB,
    MOD_TONE,
    MOD_COLOR,
    MOD_SEL,
    MOD_DTL,
    MOD_COUNT
};

// Initialize default dial values for a module
void init_geometric(Module& m);
void init_color_correction(Module& m);
void init_tone_mapping(Module& m);
void init_global_color(Module& m);
void init_selective_color(Module& m);
void init_detail(Module& m);

} // namespace desk
