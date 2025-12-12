// color_matrix.cpp
// Color Matrix Module - Applies camera RGB → linear sRGB color matrix
// Part of HEAD automatic processing
//
// Input: CV_32FC3 RGB (camera native, white-balanced)
// Output: CV_32FC3 RGB (linear sRGB working space)
//
// The matrix transforms from camera-specific RGB primaries to standard
// sRGB primaries. This is a pure colorimetric transform - WB is already
// applied in wb_bayer stage.

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
            // Get color matrix from metadata (camera RGB → sRGB)
            const cv::Matx33f &matrix = metadata.color_matrix;

            // cv::transform applies matrix to each pixel: out = matrix * in
            // Matrix is 3x3, applied to RGB channels
            cv::Mat matrixMat(matrix);
            cv::transform(input, output, matrixMat);

            // Don't clamp - allow values outside [0,1] for HDR headroom
            // Scene-referred data may have highlights > 1.0
            // Clamping happens in TAIL (output stage)

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[ColorMatrix] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace sony
