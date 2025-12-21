#include "loud.hpp"
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

    // Helper to calculate luminance from linear RGB
    static float linear_rgb_to_luminance(float r, float g, float b)
    {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }

    // Bilinear downsample for float RGB to match reference dimensions
    static std::vector<float> downsample(const float *src, int src_w, int src_h, int dst_w, int dst_h)
    {
        std::vector<float> dst(static_cast<size_t>(dst_w) * dst_h * 3);
        float scale_x = static_cast<float>(src_w) / dst_w;
        float scale_y = static_cast<float>(src_h) / dst_h;

        for (int y = 0; y < dst_h; y++)
        {
            for (int x = 0; x < dst_w; x++)
            {
                float sx = (x + 0.5f) * scale_x - 0.5f;
                float sy = (y + 0.5f) * scale_y - 0.5f;
                int x0 = std::max(0, static_cast<int>(sx));
                int x1 = std::min(src_w - 1, x0 + 1);
                int y0 = std::max(0, static_cast<int>(sy));
                int y1 = std::min(src_h - 1, y0 + 1);
                float fx = sx - x0;
                float fy = sy - y0;

                size_t i00 = (static_cast<size_t>(y0) * src_w + x0) * 3;
                size_t i10 = (static_cast<size_t>(y0) * src_w + x1) * 3;
                size_t i01 = (static_cast<size_t>(y1) * src_w + x0) * 3;
                size_t i11 = (static_cast<size_t>(y1) * src_w + x1) * 3;
                size_t di = (static_cast<size_t>(y) * dst_w + x) * 3;

                for (int c = 0; c < 3; c++)
                {
                    float v0 = src[i00 + c] * (1 - fx) + src[i10 + c] * fx;
                    float v1 = src[i01 + c] * (1 - fx) + src[i11 + c] * fx;
                    dst[di + c] = v0 * (1 - fy) + v1 * fy;
                }
            }
        }
        return dst;
    }

} // namespace

loud::Params loud::learn(const float* in_rgb, int width, int height,
                                 const uint8_t* ref_rgb8, int ref_width, int ref_height)
{
    Params params;
    if (!in_rgb || !ref_rgb8 || width <= 0 || height <= 0 || ref_width <= 0 || ref_height <= 0)
    {
        return params;
    }

    // Downsample input to match reference dimensions
    std::vector<float> in_ds = downsample(in_rgb, width, height, ref_width, ref_height);
    
    const size_t num_pixels = static_cast<size_t>(ref_width) * ref_height;
    double sum_in_lum = 0.0;
    double sum_ref_lum = 0.0;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t idx = i * 3;

        // Input luminance
        sum_in_lum += linear_rgb_to_luminance(in_ds[idx], in_ds[idx + 1], in_ds[idx + 2]);

        // Reference luminance
        float ref_r = srgb_to_linear(ref_rgb8[idx]);
        float ref_g = srgb_to_linear(ref_rgb8[idx + 1]);
        float ref_b = srgb_to_linear(ref_rgb8[idx + 2]);
        sum_ref_lum += linear_rgb_to_luminance(ref_r, ref_g, ref_b);
    }

    double avg_in_lum = sum_in_lum / num_pixels;
    double avg_ref_lum = sum_ref_lum / num_pixels;

    if (avg_in_lum > 1e-6)
    {
        params.correction = static_cast<float>(avg_ref_lum / avg_in_lum);
    }

    return params;
}

void loud::apply(float* rgb, int width, int height, const Params& params)
{
    if (params.correction == 1.0f)
    {
        return; // No change needed
    }

    const size_t num_pixels = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < num_pixels * 3; ++i)
    {
        rgb[i] *= params.correction;
    }
}
