// blc.cpp
// Black Level Correction (BLC) Module - Gold
// Simplified implementation with metadata only

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    namespace blc
    {

        bool process(
            const cv::UMat &input,
            cv::UMat &output,
            const RawMetadata &metadata)
        {
            if (input.empty())
            {
                std::cerr << "[BLC] Error: Input image is empty\n";
                return false;
            }

            if (input.type() != CV_16UC1)
            {
                std::cerr << "[BLC] Error: Input must be CV_16UC1\n";
                return false;
            }

            try
            {
                // Convert uint16 → float32
                cv::UMat float_input;
                input.convertTo(float_input, CV_32FC1);

                // Subtract black level
                cv::UMat temp;
                if (metadata.black_level > 0)
                {
                    cv::subtract(float_input, cv::Scalar(static_cast<float>(metadata.black_level)), temp);
                    cv::max(temp, 0.0f, temp);
                }
                else
                {
                    temp = float_input;
                }

                // Normalize to [0, 1]
                if (metadata.white_level > metadata.black_level)
                {
                    float scale_factor = 1.0f / static_cast<float>(metadata.white_level - metadata.black_level);
                    cv::multiply(temp, cv::Scalar(scale_factor), output);
                }
                else
                {
                    temp.copyTo(output);
                }

                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[BLC] Error: " << e.what() << "\n";
                return false;
            }
        }

    } // namespace blc
} // namespace sony
