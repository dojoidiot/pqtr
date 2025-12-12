// crop.cpp
// Crop Module - Removes optical black borders to get active area
// Part of HEAD automatic processing

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    bool Decoder::crop(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[Crop] Error: Input image is empty\n";
            return false;
        }

        // Validate crop parameters
        if (metadata.crop_left < 0 || metadata.crop_top < 0 ||
            metadata.crop_width <= 0 || metadata.crop_height <= 0)
        {
            std::cerr << "[Crop] Error: Invalid crop parameters\n";
            return false;
        }

        int right = metadata.crop_left + metadata.crop_width;
        int bottom = metadata.crop_top + metadata.crop_height;

        if (right > input.cols || bottom > input.rows)
        {
            std::cerr << "[Crop] Error: Crop region exceeds image bounds\n";
            std::cerr << "  Input: " << input.cols << "x" << input.rows << "\n";
            std::cerr << "  Crop: " << metadata.crop_left << "," << metadata.crop_top
                      << " to " << right << "," << bottom << "\n";
            return false;
        }

        try
        {
            // Define crop region
            cv::Rect roi(metadata.crop_left, metadata.crop_top,
                         metadata.crop_width, metadata.crop_height);

            // Crop the image (creates a view, then clone to get owned data)
            output = input(roi).clone();

            std::cout << "    Crop: " << input.cols << "x" << input.rows
                      << " → " << output.cols << "x" << output.rows
                      << " (removed " << metadata.crop_left << "L, "
                      << metadata.crop_top << "T, "
                      << (input.cols - right) << "R, "
                      << (input.rows - bottom) << "B)\n";

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Crop] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace sony
