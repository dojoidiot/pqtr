// exposure.cpp
// Exposure Module - Adjusts overall brightness via EV shift
// Part of Color Correction module (1 of 3 dials)

#include <opencv2/core.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply exposure adjustment
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 exposure-adjusted linear sRGB
    //
    // Dial: exposure (0.0 - 1.0)
    //   0.0 = -4 EV (16x darker)
    //   0.5 = 0 EV (neutral)
    //   1.0 = +4 EV (16x brighter)
    //
    // Formula: EV = (dial - 0.5) * 8
    //          output = input * 2^EV
    bool exposure(
        const cv::UMat &input,
        cv::UMat &output,
        float dial)
    {
        if (input.empty())
        {
            std::cerr << "[Exposure] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[Exposure] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dial to valid range
        dial = std::max(0.0f, std::min(1.0f, dial));

        // Convert dial to EV
        float ev = (dial - 0.5f) * 8.0f;

        // Convert EV to multiplier: 2^EV
        float multiplier = std::pow(2.0f, ev);

        try
        {
            // Apply exposure multiplier
            cv::multiply(input, multiplier, output);

            // Note: We don't clamp here - allow HDR headroom
            // Clamping happens in tone mapping or output

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Exposure] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
