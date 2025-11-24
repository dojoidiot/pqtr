// geometric.cpp
// Geometric Module - Crop, Zoom, Rotation
// Part of Geometric module (6 dials)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace pipe
{
namespace mods
{
    // Apply geometric transformations
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 transformed linear RGB
    //
    // 6 Dials (all 0.0-1.0):
    //   crop_top:    Top edge inset (0.0 = no crop, 1.0 = 50% inset)
    //   crop_right:  Right edge inset (0.0 = no crop, 1.0 = 50% inset)
    //   crop_bottom: Bottom edge inset (0.0 = no crop, 1.0 = 50% inset)
    //   crop_left:   Left edge inset (0.0 = no crop, 1.0 = 50% inset)
    //   zoom:        Zoom factor (0.0 = 1x, 1.0 = 4x)
    //   tilt_angle:  Rotation angle (0.0 = -45°, 0.5 = 0°, 1.0 = +45°)
    //
    // Processing order: Crop → Zoom → Rotation
    bool geometric(
        const cv::UMat &input,
        cv::UMat &output,
        float crop_top_dial,
        float crop_right_dial,
        float crop_bottom_dial,
        float crop_left_dial,
        float zoom_dial,
        float tilt_angle_dial)
    {
        if (input.empty())
        {
            std::cerr << "[Geometric] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[Geometric] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Clamp dials to valid range
        crop_top_dial = std::max(0.0f, std::min(1.0f, crop_top_dial));
        crop_right_dial = std::max(0.0f, std::min(1.0f, crop_right_dial));
        crop_bottom_dial = std::max(0.0f, std::min(1.0f, crop_bottom_dial));
        crop_left_dial = std::max(0.0f, std::min(1.0f, crop_left_dial));
        zoom_dial = std::max(0.0f, std::min(1.0f, zoom_dial));
        tilt_angle_dial = std::max(0.0f, std::min(1.0f, tilt_angle_dial));

        // Convert dials to working values
        // Crop: 0.0-1.0 → 0% to 50% inset
        float crop_top = crop_top_dial * 0.5f;
        float crop_right = crop_right_dial * 0.5f;
        float crop_bottom = crop_bottom_dial * 0.5f;
        float crop_left = crop_left_dial * 0.5f;

        // Zoom: 0.0-1.0 → 1x to 4x
        float zoom = 1.0f + zoom_dial * 3.0f;

        // Rotation: 0.0-1.0 → -45° to +45°
        float tilt_angle = (tilt_angle_dial - 0.5f) * 90.0f;

        // Check if any transformation is needed
        bool needs_crop = (crop_top > 0.001f || crop_right > 0.001f ||
                          crop_bottom > 0.001f || crop_left > 0.001f);
        bool needs_zoom = (std::abs(zoom - 1.0f) > 0.001f);
        bool needs_rotation = (std::abs(tilt_angle) > 0.01f);

        if (!needs_crop && !needs_zoom && !needs_rotation)
        {
            input.copyTo(output);
            return true;
        }

        try
        {
            cv::UMat current;
            input.copyTo(current);

            int width = current.cols;
            int height = current.rows;

            // Step 1: Apply crop
            if (needs_crop)
            {
                int x = static_cast<int>(width * crop_left);
                int y = static_cast<int>(height * crop_top);
                int w = width - static_cast<int>(width * (crop_left + crop_right));
                int h = height - static_cast<int>(height * (crop_top + crop_bottom));

                // Ensure valid crop region
                w = std::max(1, w);
                h = std::max(1, h);
                x = std::min(x, width - 1);
                y = std::min(y, height - 1);
                if (x + w > width) w = width - x;
                if (y + h > height) h = height - y;

                cv::Rect crop_rect(x, y, w, h);
                cv::UMat cropped;
                current(crop_rect).copyTo(cropped);
                current = cropped;

                width = current.cols;
                height = current.rows;
            }

            // Step 2: Apply zoom (scale from center)
            if (needs_zoom)
            {
                // Calculate crop region for zoom (center crop)
                int new_w = static_cast<int>(width / zoom);
                int new_h = static_cast<int>(height / zoom);
                int x = (width - new_w) / 2;
                int y = (height - new_h) / 2;

                // Ensure valid region
                new_w = std::max(1, new_w);
                new_h = std::max(1, new_h);

                cv::Rect zoom_rect(x, y, new_w, new_h);
                cv::UMat zoomed_crop;
                current(zoom_rect).copyTo(zoomed_crop);

                // Scale back to original size
                cv::UMat zoomed;
                cv::resize(zoomed_crop, zoomed, cv::Size(width, height), 0, 0, cv::INTER_LINEAR);
                current = zoomed;
            }

            // Step 3: Apply rotation
            if (needs_rotation)
            {
                cv::Point2f center(width / 2.0f, height / 2.0f);
                cv::Mat rot_matrix = cv::getRotationMatrix2D(center, tilt_angle, 1.0);

                cv::UMat rotated;
                cv::warpAffine(current, rotated, rot_matrix, cv::Size(width, height),
                              cv::INTER_LINEAR, cv::BORDER_REPLICATE);
                current = rotated;
            }

            current.copyTo(output);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Geometric] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
