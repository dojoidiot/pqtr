// GEAR - Camera gear library
// Handles RAW decoding, metadata extraction, and scene-linear normalization
// Auto-detects format (Sony ARW, Canon CR2/CR3, Nikon NEF, etc.)
//
// API (pipe::Link):
//   gear::read() - Link contributor for pipe
//   Input:  Page = raw file buffer, Info = { "size": buffer_size }
//   Output: Page = Bayer buffer, Info = camera metadata
//
// GPU compute via WGPU (WebGPU)

#pragma once

#include "pipe.hpp"
#include <vector>
#include <cstdint>

namespace gear {

    // ============================================================
    // pipe::Link API
    // ============================================================

    // Read RAW file into pipeline
    pipe::Hold<pipe::Link> read();

    // Sony decoder direct call (returns pipe::Data)
    namespace sony {
        // Bayer buffer - raw sensor data + embedded preview
        struct BayerBuffer {
            std::vector<uint16_t> data;
            int width;
            int height;
            int black_level;
            int white_level;

            // Embedded preview (RGB 8-bit)
            std::vector<uint8_t> preview;
            int preview_width;
            int preview_height;
        };

        // Decode Sony ARW file
        // Returns: Page = BayerBuffer*, Info = camera metadata
        pipe::Data decode(const char* raw_data, size_t raw_size);
    }

} // namespace gear
