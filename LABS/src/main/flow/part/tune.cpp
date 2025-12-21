#include "tune.hpp"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

namespace
{
    // Helper to convert sRGB [0, 255] to linear [0, 1]
    static float srgb_to_linear(uint8_t v)
    {
        float f = v / 255.0f;
        if (f <= 0.04045f)
            return f / 12.92f;
        return std::pow((f + 0.055f) / 1.055f, 2.4f);
    }
}

void tune::learn(const float* tone_rgb, int width, int height,
                   const uint8_t* ref_rgb8, int ref_width, int ref_height,
                   lute::CameraLut& profile)
{
    if (width != ref_width || height != ref_height)
    {
        return; // Caller is responsible for downsampling
    }

    profile.sum.resize(lute::CELLS * 3, 0.0);
    profile.count.resize(lute::CELLS, 0);

    const size_t num_pixels = static_cast<size_t>(width) * height;
    const float grid_scale = lute::GRID_SIZE - 1;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t idx = i * 3;

        // Input coordinates from tone-corrected rgb
        float in_r = std::max(0.0f, std::min(1.0f, tone_rgb[idx]));
        float in_g = std::max(0.0f, std::min(1.0f, tone_rgb[idx + 1]));
        float in_b = std::max(0.0f, std::min(1.0f, tone_rgb[idx + 2]));

        // Target color from reference jpeg
        float ref_r = srgb_to_linear(ref_rgb8[idx]);
        float ref_g = srgb_to_linear(ref_rgb8[idx + 1]);
        float ref_b = srgb_to_linear(ref_rgb8[idx + 2]);

        // Find cell index in 3D LUT
        int r_idx = static_cast<int>(in_r * grid_scale + 0.5f);
        int g_idx = static_cast<int>(in_g * grid_scale + 0.5f);
        int b_idx = static_cast<int>(in_b * grid_scale + 0.5f);
        int cell_idx = r_idx + g_idx * lute::GRID_SIZE + b_idx * lute::GRID_SIZE * lute::GRID_SIZE;
        cell_idx = std::max(0, std::min(lute::CELLS - 1, cell_idx));

        // Accumulate target color
        const size_t lut_idx = cell_idx * 3;
        profile.sum[lut_idx + 0] += ref_r;
        profile.sum[lut_idx + 1] += ref_g;
        profile.sum[lut_idx + 2] += ref_b;
        profile.count[cell_idx]++;
    }
}

void tune::apply(const float* in_rgb, float* out_rgb, int width, int height,
                   const lute::CameraLut& profile)
{
    std::vector<float> lut(lute::LUT_SIZE);
    profile.lut(lut.data());

    const size_t num_pixels = static_cast<size_t>(width) * height;
    const float grid_scale = lute::GRID_SIZE - 1;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t p_idx = i * 3;
        float r = std::max(0.0f, std::min(1.0f, in_rgb[p_idx]));
        float g = std::max(0.0f, std::min(1.0f, in_rgb[p_idx + 1]));
        float b = std::max(0.0f, std::min(1.0f, in_rgb[p_idx + 2]));

        // Get grid coordinates and interpolation factors
        float r_pos = r * grid_scale;
        float g_pos = g * grid_scale;
        float b_pos = b * grid_scale;

        int r0 = static_cast<int>(r_pos);
        int g0 = static_cast<int>(g_pos);
        int b0 = static_cast<int>(b_pos);

        float fr = r_pos - r0;
        float fg = g_pos - g0;
        float fb = b_pos - b0;

        int r1 = std::min(lute::GRID_SIZE - 1, r0 + 1);
        int g1 = std::min(lute::GRID_SIZE - 1, g0 + 1);
        int b1 = std::min(lute::GRID_SIZE - 1, b0 + 1);
        
        r0 = std::max(0, std::min(lute::GRID_SIZE - 1, r0));
        g0 = std::max(0, std::min(lute::GRID_SIZE - 1, g0));
        b0 = std::max(0, std::min(lute::GRID_SIZE - 1, b0));

        // Trilinear interpolation
        for (int c = 0; c < 3; ++c)
        {
            // Get values of the 8 corners of the cube in the LUT
            float v000 = lut[(r0 + g0 * lute::GRID_SIZE + b0 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v100 = lut[(r1 + g0 * lute::GRID_SIZE + b0 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v010 = lut[(r0 + g1 * lute::GRID_SIZE + b0 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v110 = lut[(r1 + g1 * lute::GRID_SIZE + b0 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v001 = lut[(r0 + g0 * lute::GRID_SIZE + b1 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v101 = lut[(r1 + g0 * lute::GRID_SIZE + b1 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v011 = lut[(r0 + g1 * lute::GRID_SIZE + b1 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];
            float v111 = lut[(r1 + g1 * lute::GRID_SIZE + b1 * lute::GRID_SIZE * lute::GRID_SIZE) * 3 + c];

            // Interpolate along r-axis
            float v00 = v000 * (1 - fr) + v100 * fr;
            float v01 = v001 * (1 - fr) + v101 * fr;
            float v10 = v010 * (1 - fr) + v110 * fr;
            float v11 = v011 * (1 - fr) + v111 * fr;

            // Interpolate along g-axis
            float v0 = v00 * (1 - fg) + v10 * fg;
            float v1 = v01 * (1 - fg) + v11 * fg;

            // Interpolate along b-axis
            out_rgb[p_idx + c] = v0 * (1 - fb) + v1 * fb;
        }
    }
}
