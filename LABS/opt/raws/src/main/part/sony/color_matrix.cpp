// color_matrix.cpp
// Color Matrix Module - Applies camera RGB → sRGB color matrix
// Part of HEAD automatic processing (not a dial-based module)

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::color_matrix(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
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
            // Get color matrix from metadata
            const cv::Matx33f &matrix = metadata.color_matrix;

            // Debug output
            std::cout << "    Color matrix (camera → sRGB):\n";
            std::cout << "      [" << matrix(0,0) << ", " << matrix(0,1) << ", " << matrix(0,2) << "]\n";
            std::cout << "      [" << matrix(1,0) << ", " << matrix(1,1) << ", " << matrix(1,2) << "]\n";
            std::cout << "      [" << matrix(2,0) << ", " << matrix(2,1) << ", " << matrix(2,2) << "]\n";

            // Convert matrix to Mat for transform
            cv::Mat matrixMat(matrix);

            // Apply color matrix: output = input * matrix^T
            // cv::transform applies the matrix to each pixel
            cv::UMat transformed;
            cv::transform(input, transformed, matrixMat);

            // Don't clamp - allow values outside [0,1] for HDR headroom
            // Clamping happens later in tone mapping or output
            output = transformed;

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ColorMatrix] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace sony
