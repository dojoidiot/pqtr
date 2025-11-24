// tone_map.cpp
// Tone Mapping Module - Basic filmic tone compression
// Part of the minimal display-referred pipeline

#include <opencv2/core.hpp>
#include <iostream>

namespace pipe
{
namespace mods
{
    // Apply basic filmic tone mapping for HDR → SDR compression
    // Input:  CV_32FC3 in linear sRGB (after color matrix)
    // Output: CV_32FC3 tone-mapped, still linear (before gamma)
    //
    // Uses a simple Reinhard-style tone curve with configurable parameters:
    // - white_point: scene luminance that maps to display white (default 1.0)
    // - contrast: midtone contrast adjustment (default 1.0)
    //
    // Formula: L_out = L_in / (L_in + 1) adjusted for white point
    bool tone_map(
        const cv::UMat &input,
        cv::UMat &output,
        float white_point = 1.0f,
        float contrast = 1.0f)
    {
        if (input.empty())
        {
            std::cerr << "[ToneMap] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[ToneMap] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            // Extended Reinhard with white point:
            // L_out = L_in * (1 + L_in / W^2) / (1 + L_in)
            // where W = white_point
            //
            // This maps white_point → ~1.0 and compresses highlights

            float w2 = white_point * white_point;

            // Compute numerator: L * (1 + L/W^2) = L + L^2/W^2
            cv::UMat L_squared;
            cv::multiply(input, input, L_squared);

            cv::UMat term2;
            cv::divide(L_squared, w2, term2);

            cv::UMat numerator;
            cv::add(input, term2, numerator);

            // Compute denominator: 1 + L
            cv::UMat denominator;
            cv::add(input, 1.0f, denominator);

            // Output = numerator / denominator
            cv::divide(numerator, denominator, output);

            // Apply contrast curve if != 1.0
            // Simple power function around midpoint
            if (std::abs(contrast - 1.0f) > 0.001f)
            {
                // Lift to avoid log(0), apply contrast, lower back
                cv::UMat lifted;
                cv::add(output, 0.001f, lifted);
                cv::pow(lifted, contrast, output);
                cv::subtract(output, 0.001f, output);
                cv::max(output, 0.0f, output);
            }

            // Ensure output is in valid range
            cv::min(output, 1.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ToneMap] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
