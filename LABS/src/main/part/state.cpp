// state.cpp - DESK application state implementation

#include "state.hpp"

namespace desk {

Link::Link(const std::string& n) : name(n) {
    init_geometric(geometric);
    init_color_correction(color_correction);
    init_tone_mapping(tone_mapping);
    init_global_color(global_color);
    init_selective_color(selective_color);
    init_detail(detail);
}

void init_geometric(Module& m) {
    m.name = "geometric";
    // Crop (4 dials)
    m.dials["crop_top"] = 0.0f;
    m.dials["crop_right"] = 0.0f;
    m.dials["crop_bottom"] = 0.0f;
    m.dials["crop_left"] = 0.0f;
    // Zoom (1 dial)
    m.dials["scale"] = 0.5f;  // 0.5 = 1.0x
    // Rotation (1 dial)
    m.dials["tilt_angle"] = 0.5f;  // 0.5 = 0 degrees
}

void init_color_correction(Module& m) {
    m.name = "color_correction";
    // White balance (2 dials)
    m.dials["temperature"] = 0.5f;  // 0.5 = 5500K
    m.dials["tint"] = 0.5f;         // 0.5 = 0
    // Exposure (1 dial)
    m.dials["exposure"] = 0.5f;     // 0.5 = 0 EV
}

void init_tone_mapping(Module& m) {
    m.name = "tone_mapping";
    // Contrast (1 dial)
    m.dials["contrast"] = 0.5f;
    // Curve adjustment (2 dials)
    m.dials["highlights"] = 0.5f;
    m.dials["shadows"] = 0.5f;
    // Clipping point (2 dials)
    m.dials["black"] = 0.15f;
    m.dials["white"] = 0.85f;
}

void init_global_color(Module& m) {
    m.name = "global_color";
    m.dials["vibrance"] = 0.5f;
    m.dials["saturation"] = 0.5f;
    m.dials["color_density"] = 0.5f;
}

void init_selective_color(Module& m) {
    m.name = "selective_color";
    // 8 colors x 3 dials = 24 dials
    const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
    for (const char* color : colors) {
        m.dials[std::string(color) + "_hue"] = 0.5f;
        m.dials[std::string(color) + "_saturation"] = 0.5f;
        m.dials[std::string(color) + "_luminance"] = 0.5f;
    }
}

void init_detail(Module& m) {
    m.name = "detail";
    // Sharpen (2 dials) - default to no sharpening
    m.dials["sharpen_amount"] = 0.0f;  // 0.0 = no sharpening
    m.dials["sharpen_radius"] = 0.4f;  // 1.5px when sharpening enabled
    // Denoise (2 dials) - default to no denoising
    m.dials["denoise_luminance"] = 0.0f;  // 0.0 = no denoise
    m.dials["denoise_chroma"] = 0.0f;     // 0.0 = no denoise
}

} // namespace desk
