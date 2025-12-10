// baseline.cpp - VIBE
// Generic camera baseline processing
// Applies darktable-equivalent scene-referred defaults to any camera's output

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace vibe
{
namespace mods
{

// Highlight Recovery (Inpaint Opposed)
// Reconstructs clipped channels using the ratio of unclipped channels
bool highlight_recovery(const View& in, View& out, float clip)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::highlight_recovery] invalid input\n";
        return false;
    }

    cv::Mat cpu;
    in.copyTo(cpu);

    const int rows = cpu.rows;
    const int cols = cpu.cols;

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
            if (r >= clip) clipped++;
            if (g >= clip) clipped++;
            if (b >= clip) clipped++;

            if (clipped == 0 || clipped == 3) continue;

            // One or two channels clipped - recover using unclipped ratio
            float max_unclipped = 0.0f;
            float sum_unclipped = 0.0f;
            int count_unclipped = 0;

            if (r < clip) { max_unclipped = std::max(max_unclipped, r); sum_unclipped += r; count_unclipped++; }
            if (g < clip) { max_unclipped = std::max(max_unclipped, g); sum_unclipped += g; count_unclipped++; }
            if (b < clip) { max_unclipped = std::max(max_unclipped, b); sum_unclipped += b; count_unclipped++; }

            if (count_unclipped == 0 || max_unclipped < 0.01f) continue;

            float avg_unclipped = sum_unclipped / count_unclipped;
            float scale = (clipped == 1) ? (clip / max_unclipped) : (clip / avg_unclipped);

            if (r >= clip) r = std::min(r * scale, clip * 1.5f);
            if (g >= clip) g = std::min(g * scale, clip * 1.5f);
            if (b >= clip) b = std::min(b * scale, clip * 1.5f);
        }
    }

    cpu.copyTo(out);
    return true;
}

// Full baseline: highlight recovery + exposure boost
bool baseline(const View& in, View& out, float ev, float clip)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::baseline] invalid input\n";
        return false;
    }

    View temp;
    if (!highlight_recovery(in, temp, clip))
        in.copyTo(temp);

    float mult = std::pow(2.0f, ev);
    cv::multiply(temp, mult, out);
    return true;
}

// Convenience: darktable defaults (+0.7 EV, 95% clip)
bool baseline_default(const View& in, View& out)
{
    return baseline(in, out, 0.7f, 0.95f);
}

} // namespace mods
} // namespace vibe
