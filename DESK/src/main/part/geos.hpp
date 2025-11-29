// geos.hpp - GeoS Geodesic Dome visualization
// Displays a rotating geodesic dome with 17 colored triangles representing dial values

#pragma once

namespace desk {

// Initialize OpenGL resources for dome rendering (call once at startup)
void init_geos();

// Cleanup OpenGL resources (call at shutdown)
void cleanup_geos();

// Render the GeoS dome panel
// Returns true if panel wants to stay open
void render_geos_panel();

// Randomize all 17 dial values
void randomize_geos_dials();

// Get/set individual dial value (0-16)
float get_geos_dial(int index);
void set_geos_dial(int index, float value);

} // namespace desk
