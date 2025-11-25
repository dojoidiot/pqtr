// pipe.hpp
// Simplified HEAD/TAIL abstraction for RAW image processing
// BODY processing: use pipe::mods::* from mods.h

#pragma once

#include <sink.hpp>
#include <opencv2/core.hpp>
#include <map>
#include <string>

namespace pipe
{
    // Type aliases
    using View = cv::UMat;
    using Info = std::map<std::string, std::string>;

    // HEAD: Decoded RAW data
    // Contains scene-linear RGB and metadata
    struct Head
    {
        View view;  // CV_32FC3, scene-linear sRGB, [0,1+] range
        Info info;  // Metadata (camera, EXIF, dimensions, etc.)
    };

    // Available decoders
    namespace decoder
    {
        constexpr const char* SONY_ARW2 = "sony_arw2";
        // Future: CANON_CR3, NIKON_NEF, etc.
    }

    // HEAD: Decode RAW → scene-linear RGB
    // Input:  sink    - RAW file data
    //         decoder - Decoder name (use pipe::decoder::* constants)
    // Output: head    - Decoded view and metadata
    // Returns: true on success
    bool open(pqtr::Sink& sink, const std::string& decoder, Head& head);

    // TAIL: Apply sRGB gamma (OETF)
    // Input:  linear - Scene-linear RGB (CV_32FC3)
    // Output: output - Gamma-encoded RGB (CV_32FC3, [0,1] range)
    // Returns: true on success
    bool gamma(const View& linear, View& output);

    // TAIL: Save to PNG file
    // Input:  view - Gamma-encoded RGB (CV_32FC3)
    //         path - Output file path
    // Returns: true on success
    bool save(const View& view, const std::string& path);

    // BODY: Processing modules
    // Use pipe::mods::* functions from mods.h:
    //   - geometric()       (6 dials)
    //   - exposure()        (1 dial)
    //   - white_balance()   (2 dials)
    //   - tone_map()        (5 dials)
    //   - global_color()    (3 dials)
    //   - selective_color() (24 dials)
    //   - detail()          (4 dials)
    // Total: 45 dials

} // namespace pipe
