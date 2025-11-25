// linkeditor.hpp - Link editor component
// Right panel for editing 6 modules with 45 dials

#pragma once

#include "state.hpp"

namespace desk {

// Render the link editor panel
// Returns true if any dial value changed
bool render_link_editor(State& state);

} // namespace desk
