// view.hpp
// Internal: Display conversion utilities (linear → sRGB)
// Not a public header - used only within pipe module

#pragma once

#include <pipe.hpp>

namespace pipe::internal
{

    // Apply sRGB gamma encoding to linear scene data
    bool applyGamma(const View& linear, View& gamma);

    // Convert linear scene data to 8-bit BGR for display
    // Optionally scales down to max_dim if > 0
    View toDisplayView(const View& linear, int max_dim = 0);

} // namespace pipe::internal
