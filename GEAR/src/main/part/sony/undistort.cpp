// undistort.cpp
// Lens distortion correction using Sony's embedded radial coefficients
// Part of HEAD automatic processing
//
// Algorithm based on darktable's lens.cc and stannum.io analysis:
//
// Sony stores N spline knots (N = first value in tag, typically 11 or 16).
// Knots are distributed evenly from center (r=0) to corner (r=1).
// Each knot value is scaled by 2^-14 to get the correction factor:
//
//   g(r) = 1 + param[i] * 2^-14
//
// The correction maps undistorted radius to distorted (source) radius:
//   r_distorted = r_undistorted * g(r)
//
// For barrel distortion (positive params at edge), this means:
//   - Undistorted image is LARGER than distorted source
//   - To fill output pixel at r_out, sample from r_src = r_out * g(r_out)
//
// Note: darktable uses an autoscale factor to prevent black borders.
// We apply the same concept by computing max(g) and normalizing.

#include "../sony.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace sony
{
    // Interpolate spline value at normalized radius r (0=center, 1=corner)
    // knot_count is the number of valid knots
    static float interpolate_spline(const int16_t *params, int knot_count, float r)
    {
        if (knot_count <= 0) return 0.0f;
        if (knot_count == 1) return static_cast<float>(params[0]);

        // Clamp r to [0, 1]
        if (r <= 0.0f) return static_cast<float>(params[0]);
        if (r >= 1.0f) return static_cast<float>(params[knot_count - 1]);

        // Map r to knot index: r in [0,1] -> idx in [0, knot_count-1]
        float idx = r * (knot_count - 1);
        int i0 = static_cast<int>(idx);
        int i1 = std::min(i0 + 1, knot_count - 1);
        float t = idx - i0;

        // Linear interpolation between knots
        return params[i0] * (1.0f - t) + params[i1] * t;
    }

    bool Decoder::undistort(
        const cv::UMat &input,
        cv::UMat &output,
        const RawMetadata &metadata)
    {
        if (input.empty())
        {
            std::cerr << "[Undistort] Error: Input image is empty\n";
            return false;
        }

        if (!metadata.has_distortion_params || metadata.distortion_knot_count <= 0)
        {
            // No distortion data - pass through unchanged
            input.copyTo(output);
            std::cout << "    Undistort: skipped (no params)\n";
            return true;
        }

        try
        {
            int width = input.cols;
            int height = input.rows;
            int knot_count = metadata.distortion_knot_count;

            // Center of image (optical center)
            float cx = width / 2.0f;
            float cy = height / 2.0f;

            // Maximum radius (center to corner) for normalization
            float r_max = std::sqrt(cx * cx + cy * cy);

            // Sony scale factor: 2^-14
            const float scale = 1.0f / 16384.0f;

            // Compute max g(r) at the image edges for autoscale
            // This ensures we don't get black borders after correction
            float g_max = 1.0f;
            for (int i = 0; i < knot_count; i++)
            {
                float g = 1.0f + scale * metadata.distortion_params[i];
                if (g > g_max) g_max = g;
            }

            // Build remap tables
            // For each output pixel, find where to sample from in the distorted input
            cv::Mat map_x(height, width, CV_32FC1);
            cv::Mat map_y(height, width, CV_32FC1);

            float max_displacement = 0.0f;

            for (int y = 0; y < height; y++)
            {
                float *mx = map_x.ptr<float>(y);
                float *my = map_y.ptr<float>(y);

                for (int x = 0; x < width; x++)
                {
                    // Vector from center
                    float dx = x - cx;
                    float dy = y - cy;
                    float r = std::sqrt(dx * dx + dy * dy);

                    if (r < 0.5f)
                    {
                        // At center, no displacement
                        mx[x] = static_cast<float>(x);
                        my[x] = static_cast<float>(y);
                    }
                    else
                    {
                        // Normalized radius (0 = center, 1 = corner)
                        float r_norm = r / r_max;

                        // Get correction factor at this radius
                        // g(r) = 1 + scale * spline_value
                        float spline_val = interpolate_spline(metadata.distortion_params,
                                                               knot_count, r_norm);
                        float g = 1.0f + scale * spline_val;

                        // Normalize by g_max to prevent black borders
                        // This scales the entire output so edges fit
                        float g_normalized = g / g_max;

                        // Source radius in distorted image
                        // r_src = r_out * g_normalized
                        float src_x = cx + dx * g_normalized;
                        float src_y = cy + dy * g_normalized;

                        mx[x] = src_x;
                        my[x] = src_y;

                        float disp = std::sqrt((src_x - x) * (src_x - x) + (src_y - y) * (src_y - y));
                        if (disp > max_displacement) max_displacement = disp;
                    }
                }
            }

            // Apply remap with bilinear interpolation
            cv::UMat map_x_gpu, map_y_gpu;
            map_x.copyTo(map_x_gpu);
            map_y.copyTo(map_y_gpu);

            cv::remap(input, output, map_x_gpu, map_y_gpu, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

            std::cout << "    Undistort: " << knot_count << " knots, g_max=" << g_max
                      << ", max_disp=" << max_displacement << "px"
                      << " (params[0]=" << metadata.distortion_params[0]
                      << ", params[" << knot_count-1 << "]=" << metadata.distortion_params[knot_count-1] << ")\n";

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[Undistort] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace sony
