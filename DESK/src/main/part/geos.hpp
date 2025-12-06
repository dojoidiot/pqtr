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

// ============================================================
// Tuning State (for optimizer visualization)
// ============================================================

// Set tuning mode (disables random drift, shows optimizer position)
void set_geos_tuning(bool active);
bool is_geos_tuning();

// Update optimizer progress (r=0-1 distance from target, theta=direction)
void set_geos_progress(float r, float theta, float loss);

// Get current loss for display
float get_geos_loss();

} // namespace desk
