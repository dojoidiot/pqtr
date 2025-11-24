// mods.h
// Pipe modules for display-referred processing
// Minimal UMat-based implementations
//
// Module order: Color Correction → Tone Mapping → Global Color → Selective Color → Detail
// All modules operate on CV_32FC3 scene-linear sRGB unless noted

#pragma once

#include <opencv2/core.hpp>

namespace pipe
{
namespace mods
{
    //--------------------------------------------------------------------------
    // Color Correction (3 dials)
    //--------------------------------------------------------------------------

    // Exposure - brightness adjustment via EV shift
    // Dial: 0.0-1.0, default 0.5 (neutral)
    // Maps to: -4 EV to +4 EV
    bool exposure(
        const cv::UMat &input,
        cv::UMat &output,
        float dial);

    // White Balance - color temperature and tint
    // temperature: 0.0-1.0, default 0.5 (maps to 2000K-10000K)
    // tint: 0.0-1.0, default 0.5 (maps to -100 to +100)
    bool white_balance(
        const cv::UMat &input,
        cv::UMat &output,
        float temperature,
        float tint);

    //--------------------------------------------------------------------------
    // Tone Mapping (5 dials)
    //--------------------------------------------------------------------------

    // Tone Mapping - HDR → SDR compression
    // Basic filmic curve with configurable white point and contrast
    bool tone_map(
        const cv::UMat &input,
        cv::UMat &output,
        float white_point = 1.0f,
        float contrast = 1.0f);

    //--------------------------------------------------------------------------
    // Utility (automatic, no dials)
    //--------------------------------------------------------------------------

    // Color Matrix - camera RGB → sRGB (now in decoder, kept for reference)
    bool color_matrix(
        const cv::UMat &input,
        cv::UMat &output,
        const cv::Matx33f &matrix);

} // namespace mods
} // namespace pipe
