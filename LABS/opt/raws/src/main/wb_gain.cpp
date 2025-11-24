// wb_gain.cpp
// White Balance Gain (WB_Gain) Module Implementation
// GPU-accelerated point operation using OpenCV UMat

#include "wb_gain.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace mods {

// Constructor with metadata
WBGain::WBGain(const RawMetadata& metadata)
    : metadata_(metadata), has_metadata_(true) {
    // Normalize raw WB values (use green as reference)
    // Format: [R, G, B, G] matching rawpy.camera_whitebalance
    float g_ref = metadata.wb_rggb[1] > 0 ? metadata.wb_rggb[1] : 1024.0f;
    float wb_r = metadata.wb_rggb[0] / g_ref;  // R
    float wb_g = 1.0f;
    float wb_b = metadata.wb_rggb[2] / g_ref;  // B (index 2, not 3!)
    wb_map_ = buildWBMap(wb_r, wb_g, wb_b, metadata.bayer_pattern);
}

// Default constructor
WBGain::WBGain()
    : metadata_(), has_metadata_(false) {
    // Default: unity gains (no white balance correction)
    wb_map_ = {1.0f, 1.0f, 1.0f, 1.0f};
}

std::string WBGain::name() const {
    return "WB_Gain";
}

Params WBGain::defaults() const {
    Params p;
    p["enabled"] = 1.0f;
    if (!has_metadata_) {
        p["wb_r"] = 1.0f;
        p["wb_g"] = 1.0f;
        p["wb_b"] = 1.0f;
        p["bayer_pattern"] = static_cast<float>(cv::COLOR_BayerRG2RGB);  // Default: RGGB
    }
    return p;
}

void WBGain::setMetadata(const RawMetadata& metadata) {
    metadata_ = metadata;
    has_metadata_ = true;
    // Normalize raw WB values (use green as reference)
    // Format: [R, G, B, G] matching rawpy.camera_whitebalance
    float g_ref = metadata.wb_rggb[1] > 0 ? metadata.wb_rggb[1] : 1024.0f;
    float wb_r = metadata.wb_rggb[0] / g_ref;  // R
    float wb_g = 1.0f;
    float wb_b = metadata.wb_rggb[2] / g_ref;  // B (index 2, not 3!)
    wb_map_ = buildWBMap(wb_r, wb_g, wb_b, metadata.bayer_pattern);
}

bool WBGain::process(
    const cv::UMat& input,
    cv::UMat& output,
    const Params& params
) {
    // Check if module is enabled
    if (!isEnabled(params)) {
        input.copyTo(output);
        return true;
    }

    // Validate input format
    if (input.empty()) {
        std::cerr << "[WB_Gain] Error: Input image is empty\n";
        return false;
    }

    if (input.type() != CV_32FC1) {
        std::cerr << "[WB_Gain] Error: Input must be CV_32FC1 (float32 single channel)\n";
        std::cerr << "[WB_Gain] Got type: " << input.type() << "\n";
        return false;
    }

    // Determine WB coefficients and Bayer pattern
    std::array<float, 4> wb_map;
    int bayer_pattern;

    if (has_metadata_) {
        wb_map = wb_map_;
        bayer_pattern = metadata_.bayer_pattern;
    } else {
        // Extract from params
        auto it_r = params.find("wb_r");
        auto it_g = params.find("wb_g");
        auto it_b = params.find("wb_b");
        auto it_pattern = params.find("bayer_pattern");

        float wb_r = (it_r != params.end()) ? it_r->second : 1.0f;
        float wb_g = (it_g != params.end()) ? it_g->second : 1.0f;
        float wb_b = (it_b != params.end()) ? it_b->second : 1.0f;
        bayer_pattern = (it_pattern != params.end()) ?
            static_cast<int>(it_pattern->second) : cv::COLOR_BayerRG2RGB;

        wb_map = buildWBMap(wb_r, wb_g, wb_b, bayer_pattern);
    }

    // Apply white balance gain on GPU
    try {
        applyWhiteBalanceGain(input, output, bayer_pattern, wb_map);
    } catch (const cv::Exception& e) {
        std::cerr << "[WB_Gain] OpenCV error: " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[WB_Gain] Error: " << e.what() << "\n";
        return false;
    }

    return true;
}

void WBGain::applyWhiteBalanceGain(
    const cv::UMat& input,
    cv::UMat& output,
    int bayer_pattern,
    const std::array<float, 4>& wb_map
) {
    // Create a gain map matching input dimensions
    // Each pixel gets multiplied by the appropriate WB coefficient
    // based on its position in the Bayer 2×2 pattern

    int rows = input.rows;
    int cols = input.cols;

    // Build gain map on CPU (small overhead, clearer logic)
    cv::Mat gain_map_cpu(rows, cols, CV_32FC1);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            // Calculate position in 2×2 Bayer pattern
            int pattern_idx = (r % 2) * 2 + (c % 2);  // 0, 1, 2, or 3
            gain_map_cpu.at<float>(r, c) = wb_map[pattern_idx];
        }
    }

    // Upload gain map to GPU
    cv::UMat gain_map;
    gain_map_cpu.copyTo(gain_map);

    // Element-wise multiply on GPU: output = input × gain_map
    cv::multiply(input, gain_map, output, 1.0, CV_32FC1);

    // Verify output format
    if (output.type() != CV_32FC1) {
        throw std::runtime_error("WB_Gain output type mismatch - expected CV_32FC1");
    }
}

std::array<float, 4> WBGain::buildWBMap(
    float wb_r,
    float wb_g,
    float wb_b,
    int bayer_pattern
) {
    // Map RGB coefficients to 2×2 Bayer pattern positions
    // Pattern indices: [row%2, col%2] → 0:(0,0), 1:(0,1), 2:(1,0), 3:(1,1)

    std::array<float, 4> map;

    switch (bayer_pattern) {
        case cv::COLOR_BayerRG2RGB:  // RGGB: R at (0,0)
            map[0] = wb_r;  // (0,0) → R
            map[1] = wb_g;  // (0,1) → G
            map[2] = wb_g;  // (1,0) → G
            map[3] = wb_b;  // (1,1) → B
            break;

        case cv::COLOR_BayerBG2RGB:  // BGGR: B at (0,0)
            map[0] = wb_b;  // (0,0) → B
            map[1] = wb_g;  // (0,1) → G
            map[2] = wb_g;  // (1,0) → G
            map[3] = wb_r;  // (1,1) → R
            break;

        case cv::COLOR_BayerGR2RGB:  // GRBG: Gr at (0,0), R at (0,1)
            map[0] = wb_g;  // (0,0) → G
            map[1] = wb_r;  // (0,1) → R
            map[2] = wb_b;  // (1,0) → B
            map[3] = wb_g;  // (1,1) → G
            break;

        case cv::COLOR_BayerGB2RGB:  // GBRG: Gb at (0,0), B at (0,1)
            map[0] = wb_g;  // (0,0) → G
            map[1] = wb_b;  // (0,1) → B
            map[2] = wb_r;  // (1,0) → R
            map[3] = wb_g;  // (1,1) → G
            break;

        default:
            // Unknown pattern - use unity gains
            std::cerr << "[WB_Gain] Warning: Unknown Bayer pattern " << bayer_pattern
                      << ", using unity gains\n";
            map = {1.0f, 1.0f, 1.0f, 1.0f};
            break;
    }

    return map;
}

} // namespace mods
