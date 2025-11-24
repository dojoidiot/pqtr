// wb_gain.cpp
// White Balance Gain Module - Gold
// Applies WB gains to a 3-channel BGR image.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::wb_gain(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[WB_Gain] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[WB_Gain] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            // Normalize raw WB values (use green as reference)
            float g_ref = metadata.wb_rggb[1] > 0 ? static_cast<float>(metadata.wb_rggb[1]) : 1024.0f;
            float wb_r = metadata.wb_rggb[0] / g_ref;
            float wb_g = 1.0f;
            float wb_b = metadata.wb_rggb[2] / g_ref; // B is at index 2 after swap in prepare.cpp

            // Apply gains to the B, G, R channels respectively.
            // The input UMat is in BGR order, so the Scalar must be (B, G, R).
            cv::multiply(input, cv::Scalar(wb_b, wb_g, wb_r), output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[WB_Gain] Error: " << e.what() << "\n";
            return false;
        }
    }
} // namespace sony
