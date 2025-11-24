// blc.cpp
// Black Level Correction (BLC) Module Implementation
// GPU-accelerated point operation using OpenCV UMat

#include "blc.h"
#include <opencv2/core.hpp>
#include <opencv2/core/ocl.hpp>
#include <iostream>
#include <stdexcept>

namespace mods {

// Constructor with metadata
BLC::BLC(const RawMetadata& metadata)
    : metadata_(metadata), has_metadata_(true) {
}

// Default constructor
BLC::BLC()
    : metadata_(), has_metadata_(false) {
}

std::string BLC::name() const {
    return "BLC";
}

Params BLC::defaults() const {
    Params p;
    p["enabled"] = 1.0f;
    p["normalize"] = 1.0f;  // Enable normalization by default (Python-compatible mode)
    if (!has_metadata_) {
        p["black_level"] = 512.0f;  // Typical default for 14-bit sensors
        p["white_level"] = 16383.0f;  // Typical default for 14-bit sensors
    }
    return p;
}

void BLC::setMetadata(const RawMetadata& metadata) {
    metadata_ = metadata;
    has_metadata_ = true;
}

bool BLC::process(
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
        std::cerr << "[BLC] Error: Input image is empty\n";
        return false;
    }

    if (input.type() != CV_16UC1) {
        std::cerr << "[BLC] Error: Input must be CV_16UC1 (uint16 single channel)\n";
        std::cerr << "[BLC] Got type: " << input.type() << "\n";
        return false;
    }

    // Determine black level from metadata or params
    int black_level;
    int white_level;
    bool normalize;

    if (has_metadata_) {
        black_level = metadata_.black_level;
        white_level = metadata_.white_level;
    } else {
        auto it = params.find("black_level");
        if (it != params.end()) {
            black_level = static_cast<int>(it->second);
        } else {
            black_level = static_cast<int>(defaults()["black_level"]);
        }

        it = params.find("white_level");
        if (it != params.end()) {
            white_level = static_cast<int>(it->second);
        } else {
            white_level = static_cast<int>(defaults()["white_level"]);
        }
    }

    // Check if normalization is enabled
    auto it = params.find("normalize");
    if (it != params.end()) {
        normalize = (it->second > 0.5f);
    } else {
        normalize = (defaults()["normalize"] > 0.5f);
    }

    // Apply black level correction on GPU
    try {
        applyBlackLevelCorrection(input, output, black_level, white_level, normalize);
    } catch (const cv::Exception& e) {
        std::cerr << "[BLC] OpenCV error: " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[BLC] Error: " << e.what() << "\n";
        return false;
    }

    return true;
}

void BLC::applyBlackLevelCorrection(
    const cv::UMat& input,
    cv::UMat& output,
    int black_level,
    int white_level,
    bool normalize
) {
    // Convert uint16 → float32
    cv::UMat float_input;
    input.convertTo(float_input, CV_32FC1);

    // Subtract black level: temp = max(0, input - black_level)
    // Using OpenCV GPU operations (UMat stays on GPU throughout)
    cv::UMat temp;
    if (black_level > 0) {
        cv::subtract(float_input, cv::Scalar(static_cast<float>(black_level)), temp);
        cv::max(temp, 0.0f, temp);  // Clamp negatives to 0
    } else {
        // No black level correction needed
        temp = float_input;
    }

    // Normalize to [0, 1] range if enabled (Python-compatible mode)
    if (normalize && white_level > black_level) {
        float scale_factor = 1.0f / static_cast<float>(white_level - black_level);
        cv::multiply(temp, cv::Scalar(scale_factor), output);
    } else {
        // No normalization (original behavior)
        temp.copyTo(output);
    }

    // Verify output format
    if (output.type() != CV_32FC1) {
        throw std::runtime_error("BLC output type mismatch - expected CV_32FC1");
    }
}

} // namespace mods
