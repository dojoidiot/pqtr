// blc.h
// Black Level Correction (BLC) Module
// Type A (Point Operation) - GPU Accelerated
//
// Converts RAW Bayer data from integer to float and subtracts sensor black level
// Input:  Bayer CV_16UC1 (uint16 single channel)
// Output: Bayer CV_32FC1 (float32 single channel, black-corrected)
//
// Math: output_pixel = max(0.0, float(input_pixel) - black_level)

#pragma once

#include "module.h"
#include "sony_arw2.h"
#include <opencv2/core.hpp>
#include <string>

namespace mods {

class BLC : public Module {
public:
    // Constructor accepts metadata for black level extraction
    explicit BLC(const RawMetadata& metadata);

    // Default constructor (uses params["black_level"] if metadata not available)
    BLC();

    // Process: Convert uint16 Bayer → float32 Bayer with black subtraction
    bool process(
        const cv::UMat& input,  // Bayer CV_16UC1
        cv::UMat& output,       // Bayer CV_32FC1 (black-corrected)
        const Params& params
    ) override;

    std::string name() const override;

    Params defaults() const override;

    // Update metadata (allows dynamic black level changes)
    void setMetadata(const RawMetadata& metadata);

private:
    RawMetadata metadata_;
    bool has_metadata_;

    // GPU kernel wrapper for black level correction and normalization
    void applyBlackLevelCorrection(
        const cv::UMat& input,
        cv::UMat& output,
        int black_level,
        int white_level,
        bool normalize
    );
};

} // namespace mods
