// RAWS - RAW decoder library
// Decodes RAW files to scene-linear RGB
// Auto-detects format (Sony ARW, Canon CR2/CR3, Nikon NEF, etc.)

#pragma once

#include "pipe.hpp"
#include "sink.hpp"

namespace raws {

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
    Result decode(pqtr::Sink& sink);

} // namespace raws
