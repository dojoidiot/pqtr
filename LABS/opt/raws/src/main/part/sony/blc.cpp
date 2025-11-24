// blc.cpp
// Black Level Correction (BLC) Module - Gold
// Simplified and pragmatic implementation.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::blc(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[BLC] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_16UC3)
        {
            std::cerr << "[BLC] Error: Input must be CV_16UC3\n";
            return false;
        }

        try
        {
            // Ensure white level is valid for scaling
            if (metadata.white_level <= metadata.black_level)
            {
                // Fallback: just convert to float and clamp at black level
                input.convertTo(output, CV_32FC3);
                cv::max(output, static_cast<float>(metadata.black_level), output);
                return true;
            }

            float black = static_cast<float>(metadata.black_level);
            float white = static_cast<float>(metadata.white_level);

            // Calculate scale factor to normalize the [black, white] range to [0, 1]
            float scale = 1.0f / (white - black);

            // This one operation is equivalent to: output = (input * 1.0 - black) * scale
            input.convertTo(output, CV_32FC3, scale, -black * scale);

            // Clamp any resulting negative values (from input < black_level) to zero.
            cv::max(output, 0.0f, output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[BLC] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
