// demosaic.cpp
// Demosaic Module - Scene-Referred
// Converts Bayer pattern to RGB
//
// Input: CV_32FC1 (white-balanced Bayer, normalized [0,1+])
// Output: CV_32FC3 (linear RGB)
//
// Note: OpenCV demosaicing requires 8-bit or 16-bit input.
// We scale float to 16-bit, demosaic, then scale back.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::demosaic(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[Demosaic] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC1)
        {
            std::cerr << "[Demosaic] Error: Input must be CV_32FC1, got " << input.type() << "\n";
            return false;
        }

        try
        {
            // Map custom pattern codes to OpenCV RGB output codes
            // Pipeline uses RGB internally for consistency with image processing conventions
            int cv_pattern_code;
            switch (metadata.bayer_pattern)
            {
            case 46: // RGGB
                cv_pattern_code = cv::COLOR_BayerRG2RGB;
                break;
            case 47: // GRBG
                cv_pattern_code = cv::COLOR_BayerGR2RGB;
                break;
            case 48: // BGGR
                cv_pattern_code = cv::COLOR_BayerBG2RGB;
                break;
            case 49: // GBRG
                cv_pattern_code = cv::COLOR_BayerGB2RGB;
                break;
            default:
                std::cerr << "[Demosaic] Warning: Unknown bayer pattern "
                          << metadata.bayer_pattern << ". Defaulting to RGGB.\n";
                cv_pattern_code = cv::COLOR_BayerRG2RGB;
                break;
            }

            // OpenCV demosaicing requires integer input (8-bit or 16-bit)
            // Scale float [0,1+] to 16-bit, demosaic, scale back
            //
            // HEURISTIC: Use 65535 scale with 4x headroom mapping
            // - Input [0,1] maps to [0,16383] (normal range)
            // - Input [1,4] maps to [16383,65535] (highlight headroom)
            // - Values >4 are clipped (extreme overexposure)
            // This preserves highlight detail from linearization curve expansion
            // while staying within uint16 range for OpenCV

            cv::UMat input_u16;
            input.convertTo(input_u16, CV_16UC1, 16383.0);  // Scale: 1.0 -> 16383

            // Demosaic to RGB
            cv::UMat rgb_u16;
            cv::demosaicing(input_u16, rgb_u16, cv_pattern_code);

            // Convert back to float - don't clamp, preserve HDR headroom
            rgb_u16.convertTo(output, CV_32FC3, 1.0 / 16383.0);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Demosaic] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
