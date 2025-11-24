// gamma_oetf.cpp
// Implementation of Simple Gamma OETF module

#include "gamma_oetf.h"
#include <opencv2/core.hpp>
#include <iostream>
#include <cmath>

namespace mods {

bool GammaOETF::process(
    const cv::UMat& input,
    cv::UMat& output,
    const Params& params
) {
    // Check if module is enabled
    if (!isEnabled(params)) {
        input.copyTo(output);
        return true;
    }

    // Validate input
    if (input.empty()) {
        std::cerr << "GammaOETF: Empty input image" << std::endl;
        return false;
    }

    if (input.type() != CV_32FC3) {
        std::cerr << "GammaOETF: Expected CV_32FC3, got type " << input.type() << std::endl;
        return false;
    }

    // Get parameters
    float clip_max = 1.0f;

    auto it_clip = params.find("clip_max");
    if (it_clip != params.end()) {
        clip_max = it_clip->second;
    }

    // Apply proper sRGB OETF (Opto-Electronic Transfer Function)
    // This is the standard sRGB gamma curve with piecewise linear portion near black

    // Step 1: Clip values above threshold
    cv::UMat clipped;
    cv::min(input, clip_max, clipped);

    // Step 2: Clamp negative values to zero (linear RGB can have small negatives at gamut boundary)
    cv::max(clipped, 0.0f, clipped);

    // Step 3: Apply sRGB OETF
    // For values <= 0.0031308: output = input * 12.92
    // For values > 0.0031308: output = 1.055 * input^(1/2.4) - 0.055

    // Download to CPU for piecewise operation (GPU doesn't support efficient branching per pixel)
    cv::Mat cpu_input = clipped.getMat(cv::ACCESS_READ);
    cv::Mat cpu_output(cpu_input.size(), cpu_input.type());

    const float threshold = 0.0031308f;
    const float a = 12.92f;
    const float b = 1.055f;
    const float c = 0.055f;
    const float gamma_inv = 1.0f / 2.4f;

    // Apply sRGB curve pixel by pixel
    for (int y = 0; y < cpu_input.rows; y++) {
        const float* in_row = cpu_input.ptr<float>(y);
        float* out_row = cpu_output.ptr<float>(y);

        for (int x = 0; x < cpu_input.cols * 3; x++) {
            float val = in_row[x];
            if (val <= threshold) {
                out_row[x] = val * a;
            } else {
                out_row[x] = b * std::pow(val, gamma_inv) - c;
            }
        }
    }

    // Upload back to GPU
    output = cpu_output.getUMat(cv::ACCESS_READ);

    return true;
}

} // namespace mods
