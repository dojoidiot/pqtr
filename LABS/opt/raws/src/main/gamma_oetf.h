// gamma_oetf.h
// Module 2.1: Simple Gamma (OETF - Optical-Electro Transfer Function)
//
// Applies gamma 2.2 encoding to linear RGB for display/viewing
// Part of Step 2 "DEBUG - Linear Check" pipeline

#pragma once

#include "module.h"
#include <opencv2/core.hpp>

namespace mods {

// Simple Gamma OETF Module
// Converts scene-referred linear RGB to display-referred gamma-encoded RGB
// Uses standard gamma 2.2 (sRGB-like OETF without piecewise linear section)
class GammaOETF : public Module {
public:
    GammaOETF() = default;

    // Process: Apply gamma 2.2 encoding
    // Input: Linear RGB float32 (scene-referred)
    // Output: Gamma-encoded RGB float32 (display-referred, [0.0, 1.0])
    bool process(
        const cv::UMat& input,   // CV_32FC3 linear RGB
        cv::UMat& output,        // CV_32FC3 gamma-encoded RGB
        const Params& params
    ) override;

    std::string name() const override { return "GammaOETF"; }

    Params defaults() const override {
        return {
            {"enabled", 1.0f},
            {"gamma", 2.2f},      // Gamma exponent (1/2.2 = 0.4545)
            {"clip_max", 1.0f}    // Clip values above this threshold
        };
    }
};

} // namespace mods
