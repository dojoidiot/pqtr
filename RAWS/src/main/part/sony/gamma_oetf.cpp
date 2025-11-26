// gamma_oetf.cpp
// Gamma OETF Module - Gold
// Applies sRGB gamma curve using GPU-accelerated functions

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::gamma_oetf(
        const cv::UMat &input,
        cv::UMat &output)
    {
        if (input.empty())
        {
            std::cerr << "[GammaOETF] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[GammaOETF] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            // Ensure input data is clipped to the expected [0, 1] range.
            cv::UMat clipped;
            cv::min(input, 1.0f, clipped);
            cv::max(clipped, 0.0f, clipped);

            // sRGB OETF constants
            const float threshold = 0.0031308f;
            const float a = 12.92f;
            const float b = 1.055f;
            const float c = 0.055f;
            const float gamma_inv = 1.0f / 2.4f;

            // Create a mask for values in the linear portion of the curve
            cv::UMat linear_mask;
            cv::compare(clipped, threshold, linear_mask, cv::CMP_LE);

            // Calculate the linear part: val * a
            cv::UMat linear_part;
            cv::multiply(clipped, a, linear_part);

            // Calculate the gamma part: b * val^(1/2.4) - c
            cv::UMat gamma_part;
            cv::pow(clipped, gamma_inv, gamma_part);
            cv::multiply(gamma_part, b, gamma_part);
            cv::subtract(gamma_part, c, gamma_part);

            // Combine the two parts using the mask
            // Where mask is non-zero (true), copy from linear_part.
            // Where mask is zero (false), copy from gamma_part.
            gamma_part.copyTo(output);
            linear_part.copyTo(output, linear_mask);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[GammaOETF] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
