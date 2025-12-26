// warp.cpp - Lens distortion correction
//
// Uses Sony's embedded radial spline coefficients to correct barrel distortion.
// Algorithm based on darktable's lens.cc and stannum.io analysis.

#include "flow.hpp"
#include <cmath>
#include <vector>
#include <cstring>

namespace flow
{

    // Interpolate spline value at normalized radius r (0=center, 1=corner)
    static float interpolate_spline(const float *params, int count, float r)
    {
        if (count <= 0)
            return 0.0f;
        if (count == 1)
            return params[0];

        if (r <= 0.0f)
            return params[0];
        if (r >= 1.0f)
            return params[count - 1];

        float idx = r * (count - 1);
        int i0 = static_cast<int>(idx);
        int i1 = (i0 + 1 < count) ? i0 + 1 : count - 1;
        float t = idx - i0;

        return params[i0] * (1.0f - t) + params[i1] * t;
    }

    // Apply lens distortion correction to RGB float data
    // Operates on interleaved RGB (3 floats per pixel)
    void warp(float *rgb, int w, int h, const float *params, int count)
    {
        if (!rgb || w <= 0 || h <= 0 || !params || count <= 0)
            return;

        // Center of image
        float cx = w / 2.0f;
        float cy = h / 2.0f;

        // Max radius (center to corner)
        float r_max = std::sqrt(cx * cx + cy * cy);

        // Sony scale factor: 2^-14
        const float scale = 1.0f / 16384.0f;

        // Compute max g(r) for autoscale (prevents black borders)
        float g_max = 1.0f;
        for (int i = 0; i < count; i++)
        {
            float g = 1.0f + scale * params[i];
            if (g > g_max)
                g_max = g;
        }

        // Create output buffer
        size_t pixels = static_cast<size_t>(w) * h;
        std::vector<float> out(pixels * 3);

        // For each output pixel, sample from distorted input
        for (int y = 0; y < h; y++)
        {
            for (int x = 0; x < w; x++)
            {
                float dx = x - cx;
                float dy = y - cy;
                float r = std::sqrt(dx * dx + dy * dy);

                float src_x, src_y;
                if (r < 0.5f)
                {
                    src_x = static_cast<float>(x);
                    src_y = static_cast<float>(y);
                }
                else
                {
                    float r_norm = r / r_max;
                    float spline_val = interpolate_spline(params, count, r_norm);
                    float g = 1.0f + scale * spline_val;
                    float g_normalized = g / g_max;

                    src_x = cx + dx * g_normalized;
                    src_y = cy + dy * g_normalized;
                }

                // Bilinear sample RGB
                int x0 = static_cast<int>(src_x);
                int y0 = static_cast<int>(src_y);
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                x0 = (x0 < 0) ? 0 : (x0 >= w) ? w - 1 : x0;
                y0 = (y0 < 0) ? 0 : (y0 >= h) ? h - 1 : y0;
                x1 = (x1 < 0) ? 0 : (x1 >= w) ? w - 1 : x1;
                y1 = (y1 < 0) ? 0 : (y1 >= h) ? h - 1 : y1;

                float fx = src_x - static_cast<int>(src_x);
                float fy = src_y - static_cast<int>(src_y);
                if (fx < 0) fx = 0;
                if (fy < 0) fy = 0;

                size_t out_idx = (y * w + x) * 3;
                for (int c = 0; c < 3; c++)
                {
                    float v00 = rgb[(y0 * w + x0) * 3 + c];
                    float v10 = rgb[(y0 * w + x1) * 3 + c];
                    float v01 = rgb[(y1 * w + x0) * 3 + c];
                    float v11 = rgb[(y1 * w + x1) * 3 + c];

                    float v0 = v00 * (1.0f - fx) + v10 * fx;
                    float v1 = v01 * (1.0f - fx) + v11 * fx;
                    out[out_idx + c] = v0 * (1.0f - fy) + v1 * fy;
                }
            }
        }

        // Copy back
        std::memcpy(rgb, out.data(), pixels * 3 * sizeof(float));
    }

} // namespace flow
