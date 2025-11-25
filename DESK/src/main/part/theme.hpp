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
constexpr float PROJECTS_PANEL_WIDTH = 280.0f;
constexpr float PROJECTS_PANEL_HEIGHT = 400.0f;
constexpr float INFO_PANEL_WIDTH = 280.0f;
constexpr float INFO_PANEL_HEIGHT = 250.0f;
constexpr float LINK_EDITOR_WIDTH = 500.0f;
constexpr float LINK_EDITOR_HEIGHT = 220.0f;

// Menu bar height
constexpr float MENU_BAR_HEIGHT = 30.0f;

} // namespace desk
