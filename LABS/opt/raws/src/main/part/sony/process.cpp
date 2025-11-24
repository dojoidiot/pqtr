// process.cpp
// Sony ARW2 RAW processing pipeline
// Implements the main `process` method for the sony::Decoder class.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    // Process to scene-linear sRGB (HEAD output)
    // Pipeline: Demosaic → BLC → WB → Color Matrix → Crop
    // Output is in scene-linear sRGB (working space), ready for BODY modules
    bool Decoder::process_linear(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb)
    {
        try
        {
            // Stage 1: Demosaic (Input: CV_16UC1, Output: CV_16UC3)
            cv::UMat rgb_u16;
            if (!demosaic(bayer, rgb_u16, metadata))
            {
                std::cerr << "[Decoder::process_linear] Demosaic failed" << std::endl;
                return false;
            }

            // Stage 2: BLC (Black Level Correction) (Input: CV_16UC3, Output: CV_32FC3)
            cv::UMat rgb_normalized;
            if (!blc(rgb_u16, rgb_normalized, metadata))
            {
                std::cerr << "[Decoder::process_linear] BLC failed" << std::endl;
                return false;
            }

            // Stage 3: White Balance (Input: CV_32FC3, Output: CV_32FC3)
            cv::UMat rgb_wb;
            if (!wb_gain(rgb_normalized, rgb_wb, metadata))
            {
                std::cerr << "[Decoder::process_linear] WB_Gain failed" << std::endl;
                return false;
            }

            // Stage 4: Color Matrix (camera RGB → sRGB)
            // This is automatic - uses calibration matrix from metadata
            cv::UMat rgb_srgb;
            if (!color_matrix(rgb_wb, rgb_srgb, metadata))
            {
                std::cerr << "[Decoder::process_linear] Color Matrix failed" << std::endl;
                return false;
            }

            // Stage 5: Crop (removes optical black borders)
            // This is automatic - uses active area from metadata
            if (!crop(rgb_srgb, rgb, metadata))
            {
                std::cerr << "[Decoder::process_linear] Crop failed" << std::endl;
                return false;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Decoder::process_linear] Exception: " << e.what() << std::endl;
            return false;
        }
    }

    // Public gamma wrapper
    bool Decoder::apply_gamma(const cv::UMat &linear, cv::UMat &gamma)
    {
        return gamma_oetf(linear, gamma);
    }

    // Full pipeline with gamma
    bool Decoder::process(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb)
    {
        // Pipeline: Demosaic → BLC → WB → Gamma
        try
        {
            cv::UMat linear;
            if (!process_linear(bayer, metadata, linear))
            {
                return false;
            }

            // Stage 4: Gamma OETF (Input: CV_32FC3, Output: CV_32FC3)
            if (!gamma_oetf(linear, rgb))
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
