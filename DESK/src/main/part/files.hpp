// files.hpp - File operations for DESK
// Load/save desk.json and pipe.json, file dialogs, texture management

#pragma once

#include "state.hpp"
#include <filesystem>

namespace desk {

// ============================================================
// Project Discovery
// ============================================================

// Scan root folder for projects (RAW files with .desk.json)
void scan_projects(State& state);

// ============================================================
// Sidecar File Operations
// ============================================================

// Load/save desk.json (DESK project settings)
bool load_desk_json(Project& project);
bool save_desk_json(const Project& project);

// Load/save pipe.json (LABS pipe configuration)
bool load_pipe_json(Project& project);
bool save_pipe_json(const Project& project);

// ============================================================
// Project Operations
// ============================================================

// Create new project from RAW file (copies to root, creates sidecars)
bool create_project(State& state, const std::filesystem::path& raw_file);

// Render project through LABS pipe (RAW -> PNG)
bool render_project(State& state, const Project& project);

// ============================================================
// Texture Operations
// ============================================================

// Load PNG texture into OpenGL
bool load_texture(State& state, const std::filesystem::path& png_path);

// Unload current texture
void unload_texture(State& state);

// ============================================================
// RAW Metadata
// ============================================================

// Load RAW metadata info from pipe head
bool load_raw_info(State& state, const Project& project);

// ============================================================
// File Dialogs
// ============================================================

// Open folder selection dialog
void open_folder_dialog();

// Open RAW file selection dialog (starts in raw_source_folder)
void open_raw_file_dialog(const State& state);

} // namespace desk
