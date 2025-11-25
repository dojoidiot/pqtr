// linkeditor.hpp - Link editor component
// Module menus (bottom left) + dial control (bottom right)

#pragma once

#include "state.hpp"

namespace desk {

// Render module/dial selection menus (bottom left panel)
// Returns true if selection changed
bool render_module_menus(State& state);

// Render dial control area (bottom right panel)
// Returns true if dial value changed
bool render_dial_control(State& state);

} // namespace desk
