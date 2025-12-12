// blc_bayer.cpp
// Black Level Correction on Bayer data (before demosaic)
// Input: CV_16UC1 (raw Bayer)
// Output: CV_32FC1 (normalized [0,1] Bayer)

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::blc_bayer(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[BLC_Bayer] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_16UC1)
        {
            std::cerr << "[BLC_Bayer] Error: Input must be CV_16UC1, got " << input.type() << "\n";
            return false;
        }

        try
        {
            // Validate levels
            if (metadata.white_level <= metadata.black_level)
            {
                std::cerr << "[BLC_Bayer] Error: Invalid black/white levels\n";
                return false;
            }

            float black = static_cast<float>(metadata.black_level);
            float white = static_cast<float>(metadata.white_level);
            float scale = 1.0f / (white - black);

            // Convert to float and normalize: output = (input - black) / (white - black)
            // Using convertTo: output = input * scale + (-black * scale)
            input.convertTo(output, CV_32FC1, scale, -black * scale);

            // Clamp negatives to zero
            cv::max(output, 0.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[BLC_Bayer] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
