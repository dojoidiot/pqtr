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

// Load/save link.json (single link preset file)
bool load_link_json(Link& link, const std::filesystem::path& path);
bool save_link_json(const Link& link, const std::filesystem::path& path);

// ============================================================
// Project Operations
// ============================================================

// Create new project from RAW file (copies to root, creates sidecars)
bool create_project(State& state, const std::filesystem::path& raw_file);

// Render project through LABS pipe directly to texture (no PNG)
// size: max dimension (0 = full resolution)
bool render_to_texture(State& state, const Project& project, int size = 0);

// Export project to PNG file (full resolution)
bool export_project(State& state, const Project& project);

// Run TUNE optimizer to match camera preview, creates/updates "Base" link
bool run_tune(State& state, Project& project);

// Start TUNE in background thread (non-blocking)
void start_tune_async(State& state, Project& project);

// Check if tuning is complete and handle results (call each frame)
void poll_tune_complete(State& state);

// ============================================================
// Texture Operations
// ============================================================

// Load PNG texture into OpenGL
bool load_texture(State& state, const std::filesystem::path& png_path);

// Unload current texture
void unload_texture(State& state);

// Unload base texture (scene-linear)
void unload_base_texture(State& state);

// Load embedded preview and base texture from RAW (if available)
bool load_embedded_preview(State& state, const Project& project);

// Unload embedded texture
void unload_embedded_texture(State& state);

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

// Open link save dialog (save current link as preset)
void open_link_save_dialog(const State& state);

// Open link load dialog (load preset into current link)
void open_link_load_dialog(const State& state);

} // namespace desk
