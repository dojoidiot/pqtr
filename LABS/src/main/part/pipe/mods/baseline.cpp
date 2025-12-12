// baseline.cpp
// Generic camera baseline processing for HEAD
// Applies darktable-equivalent scene-referred defaults to any camera's output
//
// These transforms are camera-agnostic - they work on scene-linear RGB from any decoder.
// Camera-specific processing (WB, color matrix, etc.) happens in GEAR.

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace pipe
{
namespace mods
{

// ============================================================
// Highlight Recovery (Inpaint Opposed)
// ============================================================
//
// Reconstructs clipped channels using the ratio of unclipped channels.
// Based on darktable's "inpaint opposed" algorithm.
//
// When one or two channels clip (hit white_level), we can recover them
// by looking at the ratio of the unclipped channels and extrapolating.
//
// Example: If R clips but G and B don't, we estimate R from:
//   R_recovered = R_max * (average of G,B ratios from nearby unclipped pixels)

bool highlight_recovery(
    const cv::UMat& input,
    cv::UMat& output,
    float clip_threshold)  // Threshold above which pixels are considered clipped (0-1)
{
    if (input.empty() || input.type() != CV_32FC3)
    {
        std::cerr << "[Highlight Recovery] Error: Invalid input\n";
        return false;
    }

    try
    {
        cv::Mat cpu;
        input.copyTo(cpu);

        const int rows = cpu.rows;
        const int cols = cpu.cols;

        // First pass: identify clipped pixels and compute local ratios
        for (int y = 0; y < rows; y++)
        {
            float* ptr = cpu.ptr<float>(y);
            for (int x = 0; x < cols; x++)
            {
                float& b = ptr[x * 3 + 0];
                float& g = ptr[x * 3 + 1];
                float& r = ptr[x * 3 + 2];

                // Count clipped channels
                int clipped = 0;
                if (r >= clip_threshold) clipped++;
                if (g >= clip_threshold) clipped++;
                if (b >= clip_threshold) clipped++;

                if (clipped == 0) continue;  // Nothing clipped
                if (clipped == 3) continue;  // All clipped - can't recover

                // One or two channels clipped - recover using unclipped ratio
                float max_unclipped = 0.0f;
                float sum_unclipped = 0.0f;
                int count_unclipped = 0;

                if (r < clip_threshold) { max_unclipped = std::max(max_unclipped, r); sum_unclipped += r; count_unclipped++; }
                if (g < clip_threshold) { max_unclipped = std::max(max_unclipped, g); sum_unclipped += g; count_unclipped++; }
                if (b < clip_threshold) { max_unclipped = std::max(max_unclipped, b); sum_unclipped += b; count_unclipped++; }

                if (count_unclipped == 0 || max_unclipped < 0.01f) continue;

                // Estimate the "true" brightness from unclipped channels
                float avg_unclipped = sum_unclipped / count_unclipped;

                // Scale factor: how much brighter should clipped channels be?
                // Use the ratio of clipped value to threshold as a guide
                float scale = 1.0f;
                if (clipped == 1)
                {
                    // One channel clipped: estimate from the other two
                    // Assume the clipped channel should maintain similar ratio
                    scale = clip_threshold / max_unclipped;
                }
                else
                {
                    // Two channels clipped: more conservative recovery
                    scale = clip_threshold / avg_unclipped;
                }

                // Recover clipped channels
                if (r >= clip_threshold) r = std::min(r * scale, clip_threshold * 1.5f);
                if (g >= clip_threshold) g = std::min(g * scale, clip_threshold * 1.5f);
                if (b >= clip_threshold) b = std::min(b * scale, clip_threshold * 1.5f);
            }
        }

        cpu.copyTo(output);
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Highlight Recovery] Error: " << e.what() << "\n";
        return false;
    }
}

// ============================================================
// Generic Camera Baseline
// ============================================================
//
// Applies all baseline transforms in correct order:
// 1. Highlight recovery (before exposure to preserve headroom)
// 2. Exposure boost (+0.7 EV scene-referred default)
//
// This produces a "looks good" starting point from any camera's
// scene-linear output. The optimizer then finds the style delta.

bool baseline(
    const cv::UMat& input,
    cv::UMat& output,
    float exposure_ev,      // Exposure boost in EV (default +0.7)
    float highlight_clip)   // Highlight clip threshold (default 0.95)
{
    if (input.empty() || input.type() != CV_32FC3)
    {
        std::cerr << "[Baseline] Error: Invalid input\n";
        return false;
    }

    try
    {
        cv::UMat temp;

        // Step 1: Highlight recovery (before exposure to preserve headroom)
        if (!highlight_recovery(input, temp, highlight_clip))
        {
            input.copyTo(temp);  // Fallback: use original
        }

        // Step 2: Exposure boost
        float multiplier = std::pow(2.0f, exposure_ev);
        cv::multiply(temp, multiplier, output);

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Baseline] Error: " << e.what() << "\n";
        return false;
    }
}

// Convenience: apply with darktable scene-referred defaults
bool baseline_default(const cv::UMat& input, cv::UMat& output)
{
    return baseline(input, output,
                    0.7f,   // +0.7 EV exposure (darktable default)
                    0.95f); // 95% clip threshold for highlight recovery
}

} // namespace mods
} // namespace pipe
