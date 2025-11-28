// RAWS - RAW decoder library
// Decodes RAW files to camera-native RGB
// Auto-detects format (Sony ARW, Canon CR2/CR3, Nikon NEF, etc.)
//
// ARCHITECTURE: Minimal decoder, maximum metadata
//   RAWS extracts sensor data + metadata.
//   LABS applies color science (WB, matrix, undistort).
//
// This separation ensures:
//   - RAWS = sensor-specific extraction (camera-agnostic output)
//   - LABS = color science (camera-agnostic processing)
//   - Optimizer learns actual camera transform, not correction-to-decoder

#pragma once

#include "pipe.hpp"
#include "sink.hpp"
#include <opencv2/core.hpp>

namespace raws {

    // Camera color metadata for LABS to apply
    struct ColorMeta {
        // White balance multipliers (as-shot)
        // RGGB order, normalized so G=1.0
        float wb_r = 1.0f;
        float wb_g = 1.0f;
        float wb_b = 1.0f;

        // Camera RGB → sRGB color matrix
        // 3x3 matrix transforms camera-native RGB to linear sRGB
        cv::Matx33f color_matrix = cv::Matx33f::eye();

        // Lens distortion correction
        // Radial spline coefficients (Sony format: value * 2^-14 = correction factor)
        int16_t distortion_params[16] = {0};
        int distortion_knot_count = 0;
        bool has_distortion = false;
    };

    // Decoded RAW result
    struct Result {
        bool success = false;

        // Camera-native RGB data (no WB, no color matrix applied)
        pipe::View data;          // CV_32FC3, camera-native RGB
        pipe::Info dataInfo;      // Metadata (decoder, dimensions, camera, etc.)

        // Color science metadata (for LABS to apply)
        ColorMeta colorMeta;      // WB, matrix, distortion - LABS applies these

        // Embedded camera preview
        pipe::View preview;       // CV_8UC3, display-referred sRGB
        pipe::Info previewInfo;   // Preview metadata (creative style, etc.)
    };

    // Decode a RAW file
    // Auto-detects format from file signature
    // Returns camera-native RGB + metadata bundle
    Result decode(pqtr::Sink& sink);

} // namespace raws
