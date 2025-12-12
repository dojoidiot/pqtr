// base_curve.cpp - LUTE
// Base Curve Module - Camera tone response curve (768 floats)

#include "mods.h"
#include <iostream>
#include <cmath>

namespace lute
{
namespace mods
{

bool base_curve(const View& in, View& out, Grid curve)
{
    if (in.empty() || curve == nullptr || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::base_curve] invalid input\n";
        return false;
    }

    cv::Mat cpu;
    in.copyTo(cpu);

    cv::Mat result = cpu.clone();

    for (int y = 0; y < cpu.rows; y++)
    {
        const float* in_ptr = cpu.ptr<float>(y);
        float* out_ptr = result.ptr<float>(y);

        for (int x = 0; x < cpu.cols; x++)
        {
            for (int c = 0; c < 3; c++)  // B=0, G=1, R=2
            {
                float v = std::clamp(in_ptr[x * 3 + c], 0.0f, 1.0f);

                // Linear → sRGB
                float srgb = (v <= 0.0031308f)
                    ? v * 12.92f
                    : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;

                // Per-channel LUT with interpolation
                float pos = srgb * 255.0f;
                int idx0 = static_cast<int>(pos);
                int idx1 = std::min(idx0 + 1, 255);
                float frac = pos - idx0;

                int base = c * 256;
                float out_srgb = curve[base + idx0] + frac * (curve[base + idx1] - curve[base + idx0]);

                // sRGB → linear
                out_ptr[x * 3 + c] = (out_srgb <= 0.04045f)
                    ? out_srgb / 12.92f
                    : std::pow((out_srgb + 0.055f) / 1.055f, 2.4f);
            }
        }
    }

    result.copyTo(out);
    return true;
}

void base_curve_identity(float* curve)
{
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = i / 255.0f;
}

} // namespace mods
} // namespace lute
