// theme.hpp - UI theme configuration
// Centralized styling for DESK application

#pragma once

namespace desk {

// Apply the Modern Dark theme to ImGui
// Based on AdamHarris-GamesProgrammer/Dear-ImGui-Themes
void apply_theme();

// Panel transparency level (0.0 - 1.0)
constexpr float PANEL_ALPHA = 0.85f;

// Panel dimensions (initial sizes, user can resize)
constexpr float WORKSPACE_PANEL_WIDTH = 220.0f;
constexpr float WORKSPACE_PANEL_HEIGHT = 300.0f;
constexpr float PIPE_PANEL_WIDTH = 300.0f;
constexpr float PIPE_PANEL_HEIGHT = 400.0f;
constexpr float INFO_PANEL_WIDTH = 280.0f;
constexpr float INFO_PANEL_HEIGHT = 250.0f;
constexpr float LINK_EDITOR_WIDTH = 800.0f;
constexpr float LINK_EDITOR_HEIGHT = 180.0f;
constexpr float EMBEDDED_PANEL_WIDTH = 400.0f;
constexpr float EMBEDDED_PANEL_HEIGHT = 300.0f;

// Menu bar height
constexpr float MENU_BAR_HEIGHT = 30.0f;

} // namespace desk
