// wb_gain.cpp
// White Balance Gain Module - Gold
// Simplified implementation with metadata only

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>
#include <array>

namespace sony
{
    namespace wb_gain
    {

        static std::array<float, 4> buildWBMap(
            float wb_r,
            float wb_g,
            float wb_b,
            int bayer_pattern)
        {
            std::array<float, 4> map;

            switch (bayer_pattern)
            {
            case 46: // RGGB (BayerRG)
                map[0] = wb_r;
                map[1] = wb_g;
                map[2] = wb_g;
                map[3] = wb_b;
                break;
            case 48: // BGGR (BayerBG)
                map[0] = wb_b;
                map[1] = wb_g;
                map[2] = wb_g;
                map[3] = wb_r;
                break;
            case 47: // GRBG (BayerGR)
                map[0] = wb_g;
                map[1] = wb_r;
                map[2] = wb_b;
                map[3] = wb_g;
                break;
            case 49: // GBRG (BayerGB)
                map[0] = wb_g;
                map[1] = wb_b;
                map[2] = wb_r;
                map[3] = wb_g;
                break;
            default:
                map = {wb_g, wb_g, wb_g, wb_g};
                break;
            }

            return map;
        }

        bool process(
            const cv::UMat &input,
            cv::UMat &output,
            const RawMetadata &metadata)
        {
            if (input.empty())
            {
                std::cerr << "[WB_Gain] Error: Input image is empty\n";
                return false;
            }

            if (input.type() != CV_32FC1)
            {
                std::cerr << "[WB_Gain] Error: Input must be CV_32FC1\n";
                return false;
            }

            try
            {
                // Normalize raw WB values (use green as reference)
                float g_ref = metadata.wb_rggb[1] > 0 ? metadata.wb_rggb[1] : 1024.0f;
                float wb_r = metadata.wb_rggb[0] / g_ref;
                float wb_g = 1.0f;
                float wb_b = metadata.wb_rggb[2] / g_ref; // B is at index 2 after swap
                std::array<float, 4> wb_map = buildWBMap(wb_r, wb_g, wb_b, metadata.bayer_pattern);

                int rows = input.rows;
                int cols = input.cols;

                // Build gain map on CPU
                cv::Mat gain_map_cpu(rows, cols, CV_32FC1);
                for (int r = 0; r < rows; ++r)
                {
                    for (int c = 0; c < cols; ++c)
                    {
                        int pattern_idx = (r % 2) * 2 + (c % 2); // 0, 1, 2, or 3
                        gain_map_cpu.at<float>(r, c) = wb_map[pattern_idx];
                    }
                }

                // Upload to GPU and multiply
                cv::UMat gain_map;
                gain_map_cpu.copyTo(gain_map);
                cv::multiply(input, gain_map, output, 1.0, CV_32FC1);

                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[WB_Gain] Error: " << e.what() << "\n";
                return false;
            }
        }

    } // namespace wb_gain
} // namespace sony
