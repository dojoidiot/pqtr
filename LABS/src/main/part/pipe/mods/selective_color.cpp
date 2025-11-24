// selective_color.cpp
// Selective Color Module - HSL adjustments for 8 color bands
// Part of Selective Color module (24 dials)

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <array>

namespace pipe
{
namespace mods
{
    // Color band definitions (center hue in degrees)
    // RED=0, ORANGE=45, YELLOW=90, GREEN=150, CYAN=195, BLUE=240, PURPLE=285, MAGENTA=315
    static const std::array<float, 8> HUE_CENTERS = {0.0f, 45.0f, 90.0f, 150.0f, 195.0f, 240.0f, 285.0f, 315.0f};
    static const float HUE_RANGE = 45.0f;  // ±45° from center (with overlap)

    // Compute weight for a pixel based on its hue distance from target
    // Uses cosine falloff for smooth blending
    static float hue_weight(float pixel_hue, float target_center)
    {
        // Normalize hues to 0-360
        while (pixel_hue < 0) pixel_hue += 360.0f;
        while (pixel_hue >= 360) pixel_hue -= 360.0f;
        while (target_center < 0) target_center += 360.0f;
        while (target_center >= 360) target_center -= 360.0f;

        // Calculate angular distance (handle wrap-around)
        float diff = std::abs(pixel_hue - target_center);
        if (diff > 180.0f) diff = 360.0f - diff;

        // Cosine falloff within range
        if (diff > HUE_RANGE) return 0.0f;
        return 0.5f * (1.0f + std::cos(M_PI * diff / HUE_RANGE));
    }

    // Apply selective color adjustments
    // Input:  CV_32FC3 scene-linear sRGB
    // Output: CV_32FC3 adjusted linear RGB
    //
    // 24 Dials (8 colors × 3 HSL each, all 0.0-1.0, default 0.5):
    //   For each color band (red, orange, yellow, green, cyan, blue, purple, magenta):
    //     hue:        Shift hue (0.5 = 0° neutral, maps to -30° to +30°)
    //     saturation: Adjust saturation (0.5 = 0 neutral, maps to -1 to +1)
    //     luminance:  Adjust luminance (0.5 = 0 neutral, maps to -1 to +1)
    //
    // Algorithm:
    //   1. Convert RGB to HSL
    //   2. For each pixel, compute weight for each of 8 color bands
    //   3. Apply weighted HSL adjustments
    //   4. Convert HSL back to RGB
    bool selective_color(
        const cv::UMat &input,
        cv::UMat &output,
        const float hue_dials[8],
        const float sat_dials[8],
        const float lum_dials[8])
    {
        if (input.empty())
        {
            std::cerr << "[SelectiveColor] Error: Input image is empty\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[SelectiveColor] Error: Input must be CV_32FC3\n";
            return false;
        }

        // Convert dials to adjustment values
        std::array<float, 8> hue_adj, sat_adj, lum_adj;
        bool any_active = false;
        for (int i = 0; i < 8; i++)
        {
            // Hue: -30° to +30° (dial 0.5 = 0)
            hue_adj[i] = (std::max(0.0f, std::min(1.0f, hue_dials[i])) - 0.5f) * 60.0f;
            // Saturation: -1 to +1 (dial 0.5 = 0)
            sat_adj[i] = (std::max(0.0f, std::min(1.0f, sat_dials[i])) - 0.5f) * 2.0f;
            // Luminance: -1 to +1 (dial 0.5 = 0)
            lum_adj[i] = (std::max(0.0f, std::min(1.0f, lum_dials[i])) - 0.5f) * 2.0f;

            if (std::abs(hue_adj[i]) > 0.1f || std::abs(sat_adj[i]) > 0.01f || std::abs(lum_adj[i]) > 0.01f)
                any_active = true;
        }

        // Early exit if all neutral
        if (!any_active)
        {
            input.copyTo(output);
            return true;
        }

        try
        {
            // Work on CPU for per-pixel hue-based logic
            cv::Mat cpu_input;
            input.copyTo(cpu_input);

            // Apply gamma for HSL conversion (input is linear)
            cv::Mat gamma_rgb;
            cv::Mat clamped;
            cv::max(cpu_input, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

            // Convert to 8-bit for cvtColor
            cv::Mat rgb8;
            gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);

            // Convert RGB to HLS (OpenCV uses HLS not HSL)
            cv::Mat hls;
            cv::cvtColor(rgb8, hls, cv::COLOR_RGB2HLS);

            // Convert HLS to float for processing
            cv::Mat hls_float;
            hls.convertTo(hls_float, CV_32FC3);

            // Process each pixel
            for (int y = 0; y < hls_float.rows; y++)
            {
                cv::Vec3f* row = hls_float.ptr<cv::Vec3f>(y);
                for (int x = 0; x < hls_float.cols; x++)
                {
                    float h = row[x][0] * 2.0f;  // OpenCV H is 0-180, convert to 0-360
                    float l = row[x][1] / 255.0f;  // L is 0-255, normalize
                    float s = row[x][2] / 255.0f;  // S is 0-255, normalize

                    // Accumulate weighted adjustments from all bands
                    float total_hue_adj = 0.0f;
                    float total_sat_adj = 0.0f;
                    float total_lum_adj = 0.0f;
                    float total_weight = 0.0f;

                    for (int band = 0; band < 8; band++)
                    {
                        float w = hue_weight(h, HUE_CENTERS[band]);
                        if (w > 0.001f)
                        {
                            total_hue_adj += w * hue_adj[band];
                            total_sat_adj += w * sat_adj[band];
                            total_lum_adj += w * lum_adj[band];
                            total_weight += w;
                        }
                    }

                    // Apply adjustments if any band matched
                    if (total_weight > 0.001f)
                    {
                        // Normalize by weight for overlapping regions
                        float norm = 1.0f / total_weight;
                        total_hue_adj *= norm;
                        total_sat_adj *= norm;
                        total_lum_adj *= norm;

                        // Apply hue shift
                        h = h + total_hue_adj;
                        while (h < 0) h += 360.0f;
                        while (h >= 360) h -= 360.0f;

                        // Apply saturation adjustment (multiplicative for boost, additive for reduction)
                        if (total_sat_adj > 0)
                            s = s + (1.0f - s) * total_sat_adj;  // Boost towards 1
                        else
                            s = s * (1.0f + total_sat_adj);  // Reduce towards 0

                        // Apply luminance adjustment
                        if (total_lum_adj > 0)
                            l = l + (1.0f - l) * total_lum_adj * 0.5f;  // Boost towards 1
                        else
                            l = l * (1.0f + total_lum_adj * 0.5f);  // Reduce towards 0
                    }

                    // Clamp and store back
                    row[x][0] = std::max(0.0f, std::min(180.0f, h / 2.0f));  // Back to 0-180
                    row[x][1] = std::max(0.0f, std::min(255.0f, l * 255.0f));
                    row[x][2] = std::max(0.0f, std::min(255.0f, s * 255.0f));
                }
            }

            // Convert back to 8-bit
            cv::Mat hls_out;
            hls_float.convertTo(hls_out, CV_8UC3);

            // Convert HLS back to RGB
            cv::Mat rgb8_out;
            cv::cvtColor(hls_out, rgb8_out, cv::COLOR_HLS2RGB);

            // Convert to float and remove gamma
            cv::Mat gamma_out;
            rgb8_out.convertTo(gamma_out, CV_32FC3, 1.0/255.0);

            cv::Mat linear_out;
            cv::pow(gamma_out, 2.2f, linear_out);

            // Upload to UMat
            linear_out.copyTo(output);

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[SelectiveColor] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe
