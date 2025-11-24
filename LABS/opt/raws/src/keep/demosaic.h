// demosaic.h
// Demosaic Module
// Type B (Spatial Operation) - GPU Accelerated
//
// Converts Bayer pattern raw data to full RGB image
// Input:  Bayer CV_32FC1 (float32 single channel, white-balanced)
// Output: RGB CV_32FC3 (float32 three channel, interpolated RGB)
//
// Algorithm: High-quality bilinear interpolation (OpenCV-compatible)
//           Custom implementation for CV_32FC1 support (OpenCV demosaicing
//           only supports CV_8U/CV_16U, not float32)
//
// Bayer Pattern Support:
//   RGGB (BayerRG/46): [0,0]=R, [0,1]=G, [1,0]=G, [1,1]=B
//   BGGR (BayerBG/48): [0,0]=B, [0,1]=G, [1,0]=G, [1,1]=R
//   GRBG (BayerGR/47): [0,0]=G, [0,1]=R, [1,0]=B, [1,1]=G
//   GBRG (BayerGB/49): [0,0]=G, [0,1]=B, [1,0]=R, [1,1]=G

#pragma once

#include "module.h"     // from part/
#include "sony_arw2.h"  // from part/
#include <opencv2/core.hpp>
#include <string>

namespace mods {

class Demosaic : public Module {
public:
    // Constructor accepts metadata for Bayer pattern
    explicit Demosaic(const RawMetadata& metadata);

    // Default constructor (uses params["bayer_pattern"])
    Demosaic();

    // Process: Convert Bayer pattern to RGB
    bool process(
        const cv::UMat& input,  // Bayer CV_32FC1 (from WB_Gain)
        cv::UMat& output,       // RGB CV_32FC3 (interpolated)
        const Params& params
    ) override;

    std::string name() const override;

    Params defaults() const override;

    // Update metadata (allows dynamic Bayer pattern changes)
    void setMetadata(const RawMetadata& metadata);

private:
    RawMetadata metadata_;
    bool has_metadata_;

    // Demosaic implementation (custom float32-compatible bilinear interpolation)
    // Uses OpenCV UMat for GPU acceleration where possible
    void demosaicBayer(
        const cv::UMat& bayer,   // CV_32FC1 input
        cv::UMat& rgb,           // CV_32FC3 output
        int bayer_pattern
    );
};

} // namespace mods
