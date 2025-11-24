// demosaic.cpp
// Demosaic Module Implementation
//
// Custom float32-compatible demosaicing using high-quality bilinear interpolation
// (OpenCV's cv::demosaicing only supports CV_8U/CV_16U, not CV_32FC1)

#include "demosaic.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <iostream>

namespace mods {

// Constructor with metadata
Demosaic::Demosaic(const RawMetadata& metadata)
    : metadata_(metadata)
    , has_metadata_(true)
{
}

// Default constructor
Demosaic::Demosaic()
    : has_metadata_(false)
{
    metadata_.bayer_pattern = 46;  // Default: RGGB
}

std::string Demosaic::name() const {
    return "Demosaic";
}

Params Demosaic::defaults() const {
    return {
        {"enabled", 1.0f},
        {"bayer_pattern", 46.0f}  // RGGB default
    };
}

void Demosaic::setMetadata(const RawMetadata& metadata) {
    metadata_ = metadata;
    has_metadata_ = true;
}

bool Demosaic::process(
    const cv::UMat& input,
    cv::UMat& output,
    const Params& params
) {
    // Check if enabled
    if (!isEnabled(params)) {
        // Passthrough: Convert single channel to 3-channel (no demosaicing)
        std::vector<cv::UMat> channels = {input, input, input};
        cv::merge(channels, output);
        return true;
    }

    // Validate input
    if (input.empty()) {
        std::cerr << "Demosaic: Input image is empty" << std::endl;
        return false;
    }

    if (input.type() != CV_32FC1) {
        std::cerr << "Demosaic: Input must be CV_32FC1, got type "
                  << input.type() << std::endl;
        return false;
    }

    // Get Bayer pattern (metadata or params)
    int bayer_pattern = metadata_.bayer_pattern;
    if (!has_metadata_) {
        auto it = params.find("bayer_pattern");
        if (it != params.end()) {
            bayer_pattern = static_cast<int>(it->second);
        }
    }

    // Perform demosaicing
    try {
        demosaicBayer(input, output, bayer_pattern);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Demosaic: Error during demosaicing: " << e.what() << std::endl;
        return false;
    }
}

// Custom float32 bilinear demosaicing implementation
// This is a high-quality bilinear interpolation algorithm
// optimized for GPU via OpenCV operations
void Demosaic::demosaicBayer(
    const cv::UMat& bayer,
    cv::UMat& rgb,
    int bayer_pattern
) {
    const int rows = bayer.rows;
    const int cols = bayer.cols;

    // Create output RGB image
    rgb.create(rows, cols, CV_32FC3);

    // Download to CPU for processing (TODO: GPU kernel implementation for Phase 2)
    cv::Mat bayer_cpu = bayer.getMat(cv::ACCESS_READ);
    cv::Mat rgb_cpu(rows, cols, CV_32FC3);

    // Determine channel positions based on Bayer pattern
    // Pattern codes: RGGB=46, GRBG=47, BGGR=48, GBRG=49
    int r_row = 0, r_col = 0;  // Position of R in 2x2 Bayer cell
    int b_row = 1, b_col = 1;  // Position of B in 2x2 Bayer cell

    switch (bayer_pattern) {
        case 46:  // RGGB (BayerRG)
            r_row = 0; r_col = 0;
            b_row = 1; b_col = 1;
            break;
        case 47:  // GRBG (BayerGR)
            r_row = 0; r_col = 1;
            b_row = 1; b_col = 0;
            break;
        case 48:  // BGGR (BayerBG)
            r_row = 1; r_col = 1;
            b_row = 0; b_col = 0;
            break;
        case 49:  // GBRG (BayerGB)
            r_row = 1; r_col = 0;
            b_row = 0; b_col = 1;
            break;
        default:
            std::cerr << "Demosaic: Unknown Bayer pattern " << bayer_pattern
                      << ", using RGGB default" << std::endl;
            r_row = 0; r_col = 0;
            b_row = 1; b_col = 1;
            break;
    }

    // Bilinear interpolation algorithm
    // For each pixel, interpolate missing color channels based on neighbors
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            float r = 0.0f, g = 0.0f, b = 0.0f;

            // Determine which color this pixel represents in the Bayer pattern
            int row_mod = row % 2;
            int col_mod = col % 2;

            bool is_r = (row_mod == r_row && col_mod == r_col);
            bool is_b = (row_mod == b_row && col_mod == b_col);

            float center = bayer_cpu.at<float>(row, col);

            if (is_r) {
                // Red pixel: R is known, interpolate G and B
                r = center;

                // Interpolate G (average of 4 neighbors)
                float g_sum = 0.0f;
                int g_count = 0;
                if (row > 0) { g_sum += bayer_cpu.at<float>(row-1, col); g_count++; }
                if (row < rows-1) { g_sum += bayer_cpu.at<float>(row+1, col); g_count++; }
                if (col > 0) { g_sum += bayer_cpu.at<float>(row, col-1); g_count++; }
                if (col < cols-1) { g_sum += bayer_cpu.at<float>(row, col+1); g_count++; }
                g = (g_count > 0) ? g_sum / g_count : center;

                // Interpolate B (average of 4 diagonal neighbors)
                float b_sum = 0.0f;
                int b_count = 0;
                if (row > 0 && col > 0) { b_sum += bayer_cpu.at<float>(row-1, col-1); b_count++; }
                if (row > 0 && col < cols-1) { b_sum += bayer_cpu.at<float>(row-1, col+1); b_count++; }
                if (row < rows-1 && col > 0) { b_sum += bayer_cpu.at<float>(row+1, col-1); b_count++; }
                if (row < rows-1 && col < cols-1) { b_sum += bayer_cpu.at<float>(row+1, col+1); b_count++; }
                b = (b_count > 0) ? b_sum / b_count : center;

            } else if (is_b) {
                // Blue pixel: B is known, interpolate R and G
                b = center;

                // Interpolate G (average of 4 neighbors)
                float g_sum = 0.0f;
                int g_count = 0;
                if (row > 0) { g_sum += bayer_cpu.at<float>(row-1, col); g_count++; }
                if (row < rows-1) { g_sum += bayer_cpu.at<float>(row+1, col); g_count++; }
                if (col > 0) { g_sum += bayer_cpu.at<float>(row, col-1); g_count++; }
                if (col < cols-1) { g_sum += bayer_cpu.at<float>(row, col+1); g_count++; }
                g = (g_count > 0) ? g_sum / g_count : center;

                // Interpolate R (average of 4 diagonal neighbors)
                float r_sum = 0.0f;
                int r_count = 0;
                if (row > 0 && col > 0) { r_sum += bayer_cpu.at<float>(row-1, col-1); r_count++; }
                if (row > 0 && col < cols-1) { r_sum += bayer_cpu.at<float>(row-1, col+1); r_count++; }
                if (row < rows-1 && col > 0) { r_sum += bayer_cpu.at<float>(row+1, col-1); r_count++; }
                if (row < rows-1 && col < cols-1) { r_sum += bayer_cpu.at<float>(row+1, col+1); r_count++; }
                r = (r_count > 0) ? r_sum / r_count : center;

            } else {  // is_g
                // Green pixel: G is known, interpolate R and B
                g = center;

                // Determine if we're on R row or B row
                bool on_r_row = (row_mod == r_row);

                if (on_r_row) {
                    // G pixel on R row: R left/right, B above/below
                    float r_sum = 0.0f;
                    int r_count = 0;
                    if (col > 0) { r_sum += bayer_cpu.at<float>(row, col-1); r_count++; }
                    if (col < cols-1) { r_sum += bayer_cpu.at<float>(row, col+1); r_count++; }
                    r = (r_count > 0) ? r_sum / r_count : center;

                    float b_sum = 0.0f;
                    int b_count = 0;
                    if (row > 0) { b_sum += bayer_cpu.at<float>(row-1, col); b_count++; }
                    if (row < rows-1) { b_sum += bayer_cpu.at<float>(row+1, col); b_count++; }
                    b = (b_count > 0) ? b_sum / b_count : center;

                } else {  // on_b_row
                    // G pixel on B row: B left/right, R above/below
                    float b_sum = 0.0f;
                    int b_count = 0;
                    if (col > 0) { b_sum += bayer_cpu.at<float>(row, col-1); b_count++; }
                    if (col < cols-1) { b_sum += bayer_cpu.at<float>(row, col+1); b_count++; }
                    b = (b_count > 0) ? b_sum / b_count : center;

                    float r_sum = 0.0f;
                    int r_count = 0;
                    if (row > 0) { r_sum += bayer_cpu.at<float>(row-1, col); r_count++; }
                    if (row < rows-1) { r_sum += bayer_cpu.at<float>(row+1, col); r_count++; }
                    r = (r_count > 0) ? r_sum / r_count : center;
                }
            }

            // OpenCV uses BGR order, not RGB
            rgb_cpu.at<cv::Vec3f>(row, col) = cv::Vec3f(b, g, r);
        }
    }

    // Upload result to GPU
    rgb_cpu.copyTo(rgb);
}

} // namespace mods
