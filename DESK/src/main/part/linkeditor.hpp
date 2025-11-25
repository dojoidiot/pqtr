// linkeditor.hpp - Link editor component
// Floating panel for module/dial selection and editing

#pragma once

#include "state.hpp"

namespace desk {

// Render module/dial selection menus and equalizer
// Returns true if any dial value changed
bool render_module_menus(State& state);

} // namespace desk
