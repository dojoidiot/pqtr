// RAWS - RAW decoder library
// Decodes RAW files to scene-linear RGB
// Auto-detects format (Sony ARW, Canon CR2/CR3, Nikon NEF, etc.)
//
// RAWS is decode-only. Style estimation (curves, polynomials) belongs in LABS.

#pragma once

#include "pipe.hpp"
#include "sink.hpp"

namespace raws {

    // Decode options - control what processing is applied
    struct Options {
        bool undistort = true;   // Apply lens distortion correction (default: on)
    };

    // Decoded RAW result
    struct Result {
        bool success = false;

        // Scene-linear RGB data
        pipe::View data;          // CV_32FC3, scene-linear sRGB
        pipe::Info dataInfo;      // Metadata (decoder, dimensions, camera, etc.)

        // Embedded camera preview
        pipe::View preview;       // CV_8UC3, display-referred sRGB
        pipe::Info previewInfo;   // Preview metadata (creative style, etc.)
    };

    // Decode a RAW file
    // Auto-detects format from file signature
    // Default options: full processing (undistort enabled)
    Result decode(pqtr::Sink& sink, const Options& opts = Options{});

} // namespace raws
