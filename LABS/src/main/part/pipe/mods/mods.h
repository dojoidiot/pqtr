// mods.h
// Pipe modules for display-referred processing
// Minimal UMat-based implementations

#pragma once

#include <opencv2/core.hpp>

namespace pipe
{
namespace mods
{
    // Color Matrix - camera RGB → sRGB
    // Applies 3x3 color transformation matrix from camera metadata
    bool color_matrix(
        const cv::UMat &input,
        cv::UMat &output,
        const cv::Matx33f &matrix);

    // Tone Mapping - HDR → SDR compression
    // Basic filmic curve with configurable white point and contrast
    bool tone_map(
        const cv::UMat &input,
        cv::UMat &output,
        float white_point = 1.0f,
        float contrast = 1.0f);

} // namespace mods
} // namespace pipe
