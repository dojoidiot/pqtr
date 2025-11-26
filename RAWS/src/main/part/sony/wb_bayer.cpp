// wb_bayer.cpp
// White Balance on Bayer data (before demosaic)
// Input: CV_32FC1 (normalized Bayer)
// Output: CV_32FC1 (white-balanced Bayer)
//
// Applies WB gains directly to Bayer pattern:
// - R pixels get R gain
// - G pixels get G gain (1.0)
// - B pixels get B gain
//
// This is the correct place for WB in scene-referred pipeline.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::wb_bayer(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[WB_Bayer] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC1)
        {
            std::cerr << "[WB_Bayer] Error: Input must be CV_32FC1, got " << input.type() << "\n";
            return false;
        }

        try
        {
            // Normalize WB gains (G = 1.0 reference)
            float g_ref = metadata.wb_rggb[1] > 0 ? static_cast<float>(metadata.wb_rggb[1]) : 1024.0f;
            float wb_r = static_cast<float>(metadata.wb_rggb[0]) / g_ref;
            float wb_g = 1.0f;
            float wb_b = static_cast<float>(metadata.wb_rggb[2]) / g_ref;

            // Create gain mask matching Bayer pattern
            // For RGGB (pattern 46):
            //   R  G
            //   G  B
            // We need to apply different gains to each 2x2 cell

            int rows = input.rows;
            int cols = input.cols;

            // Build gain matrix on CPU (small, fast)
            cv::Mat gain_pattern(2, 2, CV_32FC1);

            // Map pattern code to gain positions
            switch (metadata.bayer_pattern)
            {
            case 46: // RGGB
                gain_pattern.at<float>(0, 0) = wb_r;
                gain_pattern.at<float>(0, 1) = wb_g;
                gain_pattern.at<float>(1, 0) = wb_g;
                gain_pattern.at<float>(1, 1) = wb_b;
                break;
            case 47: // GRBG
                gain_pattern.at<float>(0, 0) = wb_g;
                gain_pattern.at<float>(0, 1) = wb_r;
                gain_pattern.at<float>(1, 0) = wb_b;
                gain_pattern.at<float>(1, 1) = wb_g;
                break;
            case 48: // BGGR
                gain_pattern.at<float>(0, 0) = wb_b;
                gain_pattern.at<float>(0, 1) = wb_g;
                gain_pattern.at<float>(1, 0) = wb_g;
                gain_pattern.at<float>(1, 1) = wb_r;
                break;
            case 49: // GBRG
                gain_pattern.at<float>(0, 0) = wb_g;
                gain_pattern.at<float>(0, 1) = wb_b;
                gain_pattern.at<float>(1, 0) = wb_r;
                gain_pattern.at<float>(1, 1) = wb_g;
                break;
            default:
                std::cerr << "[WB_Bayer] Warning: Unknown pattern " << metadata.bayer_pattern << ", using RGGB\n";
                gain_pattern.at<float>(0, 0) = wb_r;
                gain_pattern.at<float>(0, 1) = wb_g;
                gain_pattern.at<float>(1, 0) = wb_g;
                gain_pattern.at<float>(1, 1) = wb_b;
                break;
            }

            // Tile the 2x2 pattern to full image size
            cv::Mat gain_full(rows, cols, CV_32FC1);
            for (int y = 0; y < rows; y++)
            {
                for (int x = 0; x < cols; x++)
                {
                    gain_full.at<float>(y, x) = gain_pattern.at<float>(y % 2, x % 2);
                }
            }

            // Upload to GPU and multiply
            cv::UMat gain_umat;
            gain_full.copyTo(gain_umat);
            cv::multiply(input, gain_umat, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[WB_Bayer] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
