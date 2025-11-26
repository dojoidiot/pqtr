// projects.hpp - Workspace and Pipe panel components

#pragma once

#include "state.hpp"

namespace desk {

// Render the workspace panel (list of RAW files)
// Returns true if selection changed
bool render_workspace_panel(State& state);

// Render the pipe panel (tree of Links/Modules/Dials for selected RAW)
// Returns true if selection changed
bool render_pipe_panel(State& state);

// Render RAW metadata info panel (scrolling table)
void render_info_panel(State& state);

} // namespace desk
