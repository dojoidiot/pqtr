// process.cpp
// Sony ARW2 RAW processing pipeline
// Implements the main `process` method for the sony::Decoder class.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::process(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb)
    {
        // Pipeline: Demosaic → BLC → WB → Gamma
        // This order ensures cv::demosaicing receives the integer data it expects.
        // Each stage is now a private static method of the Decoder class.

        try
        {
            // Stage 1: Demosaic (Input: CV_16UC1, Output: CV_16UC3)
            cv::UMat rgb_u16;
            if (!demosaic(bayer, rgb_u16, metadata))
            {
                std::cerr << "[Decoder::process] Demosaic failed" << std::endl;
                return false;
            }

            // Stage 2: BLC (Black Level Correction) (Input: CV_16UC3, Output: CV_32FC3)
            cv::UMat rgb_normalized;
            if (!blc(rgb_u16, rgb_normalized, metadata))
            {
                std::cerr << "[Decoder::process] BLC failed" << std::endl;
                return false;
            }

            // Stage 3: White Balance (Input: CV_32FC3, Output: CV_32FC3)
            cv::UMat rgb_wb;
            if (!wb_gain(rgb_normalized, rgb_wb, metadata))
            {
                std::cerr << "[Decoder::process] WB_Gain failed" << std::endl;
                return false;
            }

            // Stage 4: Gamma OETF (Input: CV_32FC3, Output: CV_32FC3)
            if (!gamma_oetf(rgb_wb, rgb))
            {
                std::cerr << "[Decoder::process] Gamma OETF failed" << std::endl;
                return false;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Decoder::process] Exception: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace sony
