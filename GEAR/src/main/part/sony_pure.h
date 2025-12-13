// sony_pure.h
// OpenCV-free Sony RAW decoder types and interface
// For WASM/WebGPU pipeline - no external dependencies

#pragma once

#include <sink.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace sony {
namespace pure {

// Bayer buffer - raw sensor data
struct BayerBuffer {
    std::vector<uint16_t> data;
    int width;
    int height;
};

// Preview buffer - decoded JPEG (RGB, 8-bit)
struct PreviewBuffer {
    std::vector<uint8_t> data;  // RGB interleaved
    int width;
    int height;
};

// RAW metadata - all camera info without OpenCV types
struct Metadata {
    // Image dimensions (sensor size from RAW)
    int width;
    int height;

    // Active area crop (removes optical black borders)
    int crop_left;
    int crop_top;
    int crop_width;   // Final image width after crop
    int crop_height;  // Final image height after crop

    // RAW processing metadata
    int black_level;
    int white_level;
    uint16_t wb_rggb[4];      // Raw WB multipliers from camera [R, G, G, B]
    int bayer_pattern;        // Bayer pattern code: 46=RGGB, 47=GRBG, 48=BGGR, 49=GBRG
    float color_matrix[9];    // Camera RGB -> sRGB matrix (row-major, 3x3)
    int orientation;          // EXIF orientation (1-8)

    // Camera identification
    std::string camera_make;
    std::string camera_model;

    // EXIF shooting parameters
    float iso;              // ISO speed (e.g., 100, 400, 3200)
    float shutter_speed;    // Shutter speed in seconds (e.g., 1/250 = 0.004)
    float aperture;         // F-number (e.g., 2.8, 5.6, 11.0)
    float focal_length;     // Focal length in mm (e.g., 50.0, 85.0)
    std::string lens_model; // Lens description

    // Preview metadata
    int preview_width;
    int preview_height;

    // Camera rendering settings (what produced the preview)
    std::string creative_style;  // "Standard", "Vivid", "Portrait", etc.
    std::string dro;             // "Off", "Auto", "Lv1"-"Lv5"
    int contrast;                // -3 to +3
    int saturation;              // -3 to +3
    int sharpness;               // -3 to +3

    // Lens correction parameters
    int16_t distortion_params[16];
    int distortion_knot_count;
    bool has_distortion_params;
};

// Decode result
struct Result {
    bool success;
    BayerBuffer bayer;
    Metadata metadata;
    PreviewBuffer preview;
    std::string error;
};

// Pure decoder - no OpenCV dependencies
// Returns Bayer buffer + metadata + preview (all in simple buffers)
Result decode(pqtr::Sink& sink);

// Internal helpers (shared with OpenCV version)
namespace internal {
    enum TIFFType {
        TYPE_BYTE = 1,
        TYPE_ASCII = 2,
        TYPE_SHORT = 3,
        TYPE_LONG = 4,
        TYPE_RATIONAL = 5
    };

    struct IFDEntry {
        uint16_t tag;
        uint16_t type;
        uint32_t count;
        uint32_t value_offset;
    };

    uint16_t read_u16(const uint8_t* data);
    uint32_t read_u32(const uint8_t* data);
    float read_rational(const std::vector<uint8_t>& file_data, uint32_t offset);
    IFDEntry parse_ifd_entry(const uint8_t* data);
    uint32_t get_entry_value(const IFDEntry& entry, const std::vector<uint8_t>& file_data);
    std::string get_entry_string(const IFDEntry& entry, const std::vector<uint8_t>& file_data);
    bool decompress_arw2(const uint8_t* compressed_data, size_t compressed_size,
                         uint16_t* output, int width, int height);
}

} // namespace pure
} // namespace sony
