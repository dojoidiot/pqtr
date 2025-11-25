// projects.hpp - Projects panel component
// Left floating panel showing project tree with links

#pragma once

#include "state.hpp"

namespace desk {

// Render the projects panel (tree view of RAW files and links)
// Returns true if selection changed
bool render_projects_panel(State& state);

// Render RAW metadata info panel (scrolling table)
void render_info_panel(State& state);

} // namespace desk
