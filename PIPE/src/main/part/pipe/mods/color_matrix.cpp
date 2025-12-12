// color_matrix.cpp
// Color Matrix Module - Applies camera RGB → sRGB color matrix
// Part of the minimal display-referred pipeline

#include <opencv2/core.hpp>
#include <iostream>

namespace pipe
{
namespace mods
{
    // Apply 3x3 color matrix to convert camera RGB to sRGB
    // Input:  CV_32FC3 in camera RGB space (linear, after WB)
    // Output: CV_32FC3 in sRGB color space (linear)
    // Matrix: 3x3 camera-to-sRGB matrix from metadata
    bool color_matrix(
        const cv::UMat &input,
        cv::UMat &output,
        const cv::Matx33f &matrix)
    {
        if (input.empty())
        {
            std::cerr << "[ColorMatrix] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[ColorMatrix] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            // Convert matrix to Mat for transform
            cv::Mat matrixMat(matrix);

            // Apply color matrix: output = input * matrix^T
            // OpenCV transform expects row vectors, matrix is applied as dst = src * M^T
            cv::UMat transformed;
            cv::transform(input, transformed, matrixMat);

            // Clamp to valid range [0, 1] - matrix can produce out-of-gamut values
            cv::max(transformed, 0.0f, output);
            cv::min(output, 1.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ColorMatrix] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
