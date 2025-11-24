// demosaic.cpp
// Demosaic Module - Gold
// Uses GPU-accelerated OpenCV demosaicing

#include "../sony.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp> // For cv::demosaicing
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

        if (input.type() != CV_16UC1)
        {
            std::cerr << "[Demosaic] Error: Input must be CV_16UC1\n";
            return false;
        }

        try
        {
            // The metadata.bayer_pattern is a custom code from the project's TIFF parsing.
            // We map it to the correct OpenCV cv::ColorConversionCodes enum for BGR output.
            int cv_pattern_code;
            switch (metadata.bayer_pattern)
            {
            case 46: // Custom code for RGGB
                cv_pattern_code = cv::COLOR_BayerRG2BGR;
                break;
            case 47: // Custom code for GRBG
                cv_pattern_code = cv::COLOR_BayerGR2BGR;
                break;
            case 48: // Custom code for BGGR
                cv_pattern_code = cv::COLOR_BayerBG2BGR;
                break;
            case 49: // Custom code for GBRG
                cv_pattern_code = cv::COLOR_BayerGB2BGR;
                break;
            default:
                std::cerr << "[Demosaic] Warning: Unknown bayer pattern "
                            << metadata.bayer_pattern << ". Defaulting to RGGB." << std::endl;
                cv_pattern_code = cv::COLOR_BayerRG2BGR; // Default to RGGB
                break;
            }

            // Use OpenCV's accelerated demosaicing function.
            // This performs bilinear interpolation by default.
            cv::demosaicing(input, output, cv_pattern_code);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Demosaic] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
