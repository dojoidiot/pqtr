// process.cpp
// Sony ARW2 RAW processing pipeline
// Bayer → BLC → WB → Demosaic → Gamma → RGB

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    // Forward declarations for processing modules
    namespace blc
    {
        bool process(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
    }
    namespace wb_gain
    {
        bool process(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
    }
    namespace demosaic
    {
        bool process(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
    }
    namespace gamma_oetf
    {
        bool process(const cv::UMat &input, cv::UMat &output);
    }

    bool arw2_gold::process(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb)
    {
        // Gold pipeline: RAW → BLC → WB → Demosaic → Gamma
        // This matches the exact flow from test/main.cpp (old pipeline)

        try
        {
            // Stage 1: BLC (Black Level Correction)
            cv::UMat bayer_normalized;
            if (!blc::process(bayer, bayer_normalized, metadata))
            {
                std::cerr << "[arw2_gold::process] BLC failed" << std::endl;
                return false;
            }

            // Stage 2: White Balance
            cv::UMat bayer_wb;
            if (!wb_gain::process(bayer_normalized, bayer_wb, metadata))
            {
                std::cerr << "[arw2_gold::process] WB_Gain failed" << std::endl;
                return false;
            }

            // Stage 3: Demosaic
            cv::UMat rgb_linear;
            if (!demosaic::process(bayer_wb, rgb_linear, metadata))
            {
                std::cerr << "[arw2_gold::process] Demosaic failed" << std::endl;
                return false;
            }

            // Stage 4: Gamma OETF
            if (!gamma_oetf::process(rgb_linear, rgb))
            {
                std::cerr << "[arw2_gold::process] Gamma OETF failed" << std::endl;
                return false;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[arw2_gold::process] Exception: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace sony
