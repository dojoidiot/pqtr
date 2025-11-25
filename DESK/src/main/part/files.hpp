// files.hpp - File operations for DESK
// Load/save desk.json and pipe.json, file dialogs

#pragma once

#include "state.hpp"
#include <string>
#include <filesystem>

namespace desk {

// Scan root folder for projects (RAW files with .desk.json)
void scan_projects(State& state);

// Load desk.json for a project
bool load_desk_json(Project& project);

// Save desk.json for a project
bool save_desk_json(const Project& project);

// Load pipe.json for a project
bool load_pipe_json(Project& project);

// Save pipe.json for a project
bool save_pipe_json(const Project& project);

// Create new project from RAW file (copies to root, creates sidecars)
bool create_project(State& state, const std::filesystem::path& raw_file);

// Render project through LABS pipe (RAW → PNG)
bool render_project(State& state, const Project& project);

// Load PNG texture into OpenGL
bool load_texture(State& state, const std::filesystem::path& png_path);

// Unload texture
void unload_texture(State& state);

// Open folder dialog (returns path or empty if cancelled)
std::filesystem::path open_folder_dialog();

// Open file dialog for RAW files (returns path or empty if cancelled)
std::filesystem::path open_raw_file_dialog();

} // namespace desk
