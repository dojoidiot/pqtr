// wb_gain.h
// White Balance Gain (WB_Gain) Module
// Type A (Point Operation) - GPU Accelerated
//
// Applies per-channel white balance gains based on Bayer pattern position
// Input:  Bayer CV_32FC1 (float32 single channel, black-corrected)
// Output: Bayer CV_32FC1 (float32 single channel, white-balanced)
//
// Math: output_pixel(x,y) = input_pixel(x,y) × wb_coeff[bayer_channel(x,y)]
//
// Bayer Pattern Mapping (OpenCV convention):
//   RGGB (BayerRG): [0,0]=R, [0,1]=G, [1,0]=G, [1,1]=B
//   BGGR (BayerBG): [0,0]=B, [0,1]=G, [1,0]=G, [1,1]=R
//   GRBG (BayerGR): [0,0]=G, [0,1]=R, [1,0]=B, [1,1]=G
//   GBRG (BayerGB): [0,0]=G, [0,1]=B, [1,0]=R, [1,1]=G

#pragma once

#include "module.h"     // from part/
#include "sony_arw2.h"  // from part/
#include <opencv2/core.hpp>
#include <string>
#include <array>

namespace mods {

class WBGain : public Module {
public:
    // Constructor accepts metadata for WB coefficients and Bayer pattern
    explicit WBGain(const RawMetadata& metadata);

    // Default constructor (uses params["wb_r"], params["wb_g"], params["wb_b"])
    WBGain();

    // Process: Apply white balance gains per Bayer channel
    bool process(
        const cv::UMat& input,  // Bayer CV_32FC1 (from BLC)
        cv::UMat& output,       // Bayer CV_32FC1 (white-balanced)
        const Params& params
    ) override;

    std::string name() const override;

    Params defaults() const override;

    // Update metadata (allows dynamic WB coefficient changes)
    void setMetadata(const RawMetadata& metadata);

private:
    RawMetadata metadata_;
    bool has_metadata_;

    // WB coefficient array indexed by Bayer 2×2 pattern position
    // Pattern: [row%2, col%2] → {0,0}, {0,1}, {1,0}, {1,1}
    std::array<float, 4> wb_map_;

    // GPU kernel wrapper for white balance gain
    void applyWhiteBalanceGain(
        const cv::UMat& input,
        cv::UMat& output,
        int bayer_pattern,
        const std::array<float, 4>& wb_map
    );

    // Build WB coefficient map from RGB coefficients and Bayer pattern
    static std::array<float, 4> buildWBMap(
        float wb_r,
        float wb_g,
        float wb_b,
        int bayer_pattern
    );
};

} // namespace mods
