// sony_arw2.h
// Sony .ARW custom decoder using link-based I/O
// Outputs Bayer data + metadata for pipeline

#pragma once

#include "module.h"
#include "link.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace mods {

// Metadata structure for RAW file information
struct RawMetadata {
    // Image dimensions
    int width;
    int height;

    // RAW processing metadata
    int black_level;
    int white_level;
    uint16_t wb_rggb[4];      // Raw WB multipliers from camera [R, G, G, B]
    int bayer_pattern;        // OpenCV Bayer pattern code
    cv::Matx33f color_matrix; // Camera RGB → sRGB matrix
    int orientation;          // EXIF orientation (1-8)

    // Camera identification
    std::string camera_make;
    std::string camera_model;

    // EXIF shooting parameters
    float iso;                // ISO speed (e.g., 100, 400, 3200)
    float shutter_speed;      // Shutter speed in seconds (e.g., 1/250 = 0.004)
    float aperture;           // F-number (e.g., 2.8, 5.6, 11.0)
    float focal_length;       // Focal length in mm (e.g., 50.0, 85.0)
    std::string lens_model;   // Lens description
};

class RawLoader : public Module {
public:
    // Module interface (deprecated - use decode() directly)
    bool process(
        const cv::UMat& input,  // Unused
        cv::UMat& output,       // Bayer data (uint16, single channel)
        const Params& params
    ) override;

    std::string name() const override;

    Params defaults() const override;

    // NEW: Link-based decoder interface (clean for pipe integration)
    bool decode(pqtr::Link& source, cv::UMat& output, RawMetadata& metadata);

    // Get metadata from last load operation
    const RawMetadata& metadata() const { return metadata_; }

    // OLD: Deprecated file path interface (for backward compatibility)
    void setFilePath(const std::string& path) { file_path_ = path; }

private:
    std::string file_path_;  // Deprecated - only for old process() interface
    RawMetadata metadata_;

    // Internal parsing helpers
    bool parseTIFF(pqtr::Link& source);
    bool parseIFD(pqtr::Link& source, size_t offset, bool is_makernote = false);
    bool decompressARW2(pqtr::Link& source, size_t strip_offset, size_t strip_bytes,
                        int width, int height, cv::UMat& output);

    // Linearization curve application
    void applyLinearizationCurve(cv::UMat& data, const std::vector<uint16_t>& curve);

    // Helper functions (adapted from archived dataprepare.cpp)
    static int detect_bayer_pattern(void* raw_ptr);
    static cv::Matx33f extract_color_matrix(void* raw_ptr);
    static int extract_orientation(void* raw_ptr);
};

} // namespace mods
