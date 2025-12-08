// RAWS - RAW loader library
// Loads RAW files to scene-linear RGB
// Auto-detects format (Sony ARW, Canon CR2/CR3, Nikon NEF, etc.)
//
// RAWS provides:
//   load() - decode RAW to flat scene-linear
//   tune() - estimate camera LUT from flat/jpeg pairs
//
// Camera LUT becomes metadata that LABS pipe applies without knowing the camera.

#pragma once

#include "pipe.hpp"
#include "sink.hpp"

namespace raws {

    // Forward declarations
    struct Result;

    // Load options - control what processing is applied
    struct Options {
        bool undistort = true;   // Apply lens distortion correction (default: on)
    };

    // Camera LUT - 3D RGB→RGB transform estimated from flat/jpeg pairs
    // This is camera-specific metadata that LABS pipe applies generically
    // Accumulates across multiple images for robust camera profiles
    //
    // Keyed by: camera_model + creative_style + dro
    struct CameraLut {
        static constexpr int GRID_SIZE = 17;
        static constexpr int CELLS = GRID_SIZE * GRID_SIZE * GRID_SIZE;  // 4,913
        static constexpr int TOTAL = CELLS * 3;  // 14,739

        // Accumulators (internal - use lut() to get final values)
        double sum[TOTAL];      // Running sum of target RGB per cell
        int count[CELLS];       // Pixel count per cell

        bool estimated = false;

        // LUT key (what style this LUT represents)
        std::string camera_make;
        std::string camera_model;
        std::string creative_style;  // "Standard", "Vivid", etc.
        std::string dro;             // "Off", "Auto", "Lv1", etc.
        int sample_count = 0;        // Number of image pairs accumulated

        // Initialize to zero accumulators
        void reset();

        // Get finalized LUT values (computes averages, identity for empty cells)
        // Caller must provide buffer of TOTAL floats
        void lut(float* out) const;

        // Check if a cell has data
        bool hasCell(int r, int g, int b) const;

        // Coverage: fraction of cells with data (0.0 - 1.0)
        float coverage() const;

        // Count empty cells
        int emptyCells() const;

        // Generate key string for this LUT (e.g., "Sony_ILCE-7M4_Standard_DRO-Auto")
        std::string key() const;

        // Check if a Result matches this LUT's style
        bool matches(const Result& result) const;

        // Describe what's missing in human terms
        // Returns list of scene suggestions to fill gaps
        // e.g., "dark reds", "bright cyans", "saturated greens"
        std::vector<std::string> missing() const;
    };

    // Analyze which cells a flat/target pair would fill
    // Returns count of NEW cells that would be filled (cells currently empty in lut)
    // Use this to decide if an image is worth adding to training
    int analyze(const pipe::View& flat, const pipe::View& target, const CameraLut& lut);

    // Loaded RAW result
    struct Result {
        bool success = false;

        // Scene-linear RGB data
        pipe::View data;          // CV_32FC3, scene-linear sRGB
        pipe::Info dataInfo;      // Metadata (decoder, dimensions, camera, etc.)

        // Embedded camera preview (target for tuning)
        pipe::View preview;       // CV_8UC3, display-referred sRGB
        pipe::Info previewInfo;   // Preview metadata (creative style, etc.)
    };

    // Load a RAW file
    // Auto-detects format from file signature
    // Default options: full processing (undistort enabled)
    Result load(pqtr::Sink& sink, const Options& opts = Options{});

    // Backward compatibility alias
    inline Result decode(pqtr::Sink& sink, const Options& opts = Options{}) {
        return load(sink, opts);
    }

    // Tune: estimate camera LUT from loaded RAW
    // flat:   Scene-linear data from load()
    // target: Camera JPEG (preview from load(), or external reference)
    // lut:    Output - accumulated into existing LUT for averaging
    // Returns true on success
    bool tune(const pipe::View& flat, const pipe::View& target, CameraLut& lut);

} // namespace raws
