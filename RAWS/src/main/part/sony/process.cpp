// process.cpp
// Sony ARW2 RAW processing pipeline
// Implements the main `process` method for the sony::Decoder class.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    // Process to camera-native RGB (HEAD output)
    //
    // MINIMAL PIPELINE (sensor data extraction only):
    //   1. BLC on Bayer    - subtract black level, normalize
    //   2. Demosaic        - Bayer → RGB
    //   3. Crop            - remove optical black borders
    //
    // DEFERRED TO LABS:
    //   - WB (white balance)     → passed as metadata, applied by LABS
    //   - Color Matrix           → passed as metadata, applied by LABS
    //   - Undistort              → passed as metadata, applied by LABS
    //
    // Output is camera-native RGB (no WB, no color matrix).
    // LABS applies WB + matrix using metadata, or optimizer learns transform.
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

            // WB SKIPPED - deferred to LABS
            // Metadata contains wb_rggb for LABS to apply

            // Stage 2: Demosaic (CV_32FC1 → CV_32FC3 RGB)
            // Converts Bayer pattern to RGB (not BGR)
            // Note: Without WB, colors will have strong green cast - this is expected
            cv::UMat rgb_native;
            if (!demosaic(bayer_blc, rgb_native, metadata))
            {
                std::cerr << "[process_linear] Demosaic failed" << std::endl;
                return false;
            }

            // COLOR MATRIX SKIPPED - deferred to LABS
            // Metadata contains color_matrix for LABS to apply

            // UNDISTORT SKIPPED - deferred to LABS
            // Metadata contains distortion_params for LABS to apply

            // Stage 3: Crop (CV_32FC3 → CV_32FC3)
            // Removes optical black border pixels
            if (!crop(rgb_native, rgb, metadata))
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
