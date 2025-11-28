// process.cpp
// Sony ARW2 RAW processing pipeline
// Implements the main `process` method for the sony::Decoder class.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    // Process to scene-linear RGB (HEAD output)
    //
    // CANONICAL PIPELINE ORDER (scene-referred):
    //   1. BLC on Bayer    - subtract black level, normalize
    //   2. WB on Bayer     - apply gains before color interpolation
    //   3. Demosaic        - Bayer → RGB
    //   4. Color Matrix    - camera RGB → linear sRGB
    //   5. Undistort       - lens distortion correction
    //   6. Crop            - remove optical black borders
    //
    // Output is scene-linear sRGB, ready for BODY modules (styling/grading)
    //
    bool Decoder::process_linear(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb)
    {
        try
        {
            // Stage 1: BLC on Bayer (CV_16UC1 → CV_32FC1)
            // Subtracts black level, normalizes to [0,1+] using white level
            cv::UMat bayer_blc;
            if (!blc_bayer(bayer, bayer_blc, metadata))
            {
                std::cerr << "[process_linear] BLC failed" << std::endl;
                return false;
            }

            // Stage 2: WB on Bayer (CV_32FC1 → CV_32FC1)
            // Applies per-channel gains to Bayer pattern before demosaic
            // This is the correct place for WB - on raw sensor data
            cv::UMat bayer_wb;
            if (!wb_bayer(bayer_blc, bayer_wb, metadata))
            {
                std::cerr << "[process_linear] WB failed" << std::endl;
                return false;
            }

            // Stage 3: Demosaic (CV_32FC1 → CV_32FC3 RGB)
            // Converts Bayer pattern to RGB (not BGR)
            cv::UMat rgb_linear;
            if (!demosaic(bayer_wb, rgb_linear, metadata))
            {
                std::cerr << "[process_linear] Demosaic failed" << std::endl;
                return false;
            }

            // Stage 4: Color Matrix (CV_32FC3 → CV_32FC3)
            // Transforms camera-native RGB to linear sRGB working space
            cv::UMat rgb_srgb;
            if (!color_matrix(rgb_linear, rgb_srgb, metadata))
            {
                std::cerr << "[process_linear] Color Matrix failed" << std::endl;
                return false;
            }

            // Stage 5: Undistort (CV_32FC3 → CV_32FC3)
            // Corrects lens barrel/pincushion distortion using Sony coefficients
            cv::UMat rgb_undistort;
            if (!undistort(rgb_srgb, rgb_undistort, metadata))
            {
                std::cerr << "[process_linear] Undistort failed" << std::endl;
                return false;
            }

            // Stage 6: Crop (CV_32FC3 → CV_32FC3)
            // Removes optical black border pixels
            if (!crop(rgb_undistort, rgb, metadata))
            {
                std::cerr << "[process_linear] Crop failed" << std::endl;
                return false;
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[process_linear] Exception: " << e.what() << std::endl;
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
