#include "tone.hpp"
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
}

void tone::learn(const float* head_rgb, int width, int height,
                   const uint8_t* ref_rgb8, int ref_width, int ref_height,
                   lute::CameraLut& profile)
{
    if (width != ref_width || height != ref_height)
    {
        // For simplicity, this implementation requires inputs to be the same size.
        // The caller (e.g., the test harness) is responsible for downsampling.
        return;
    }

    profile.tone_sum.assign(lute::TONE_CURVE_SIZE, 0.0);
    profile.tone_count.assign(lute::TONE_CURVE_SIZE, 0);

    const size_t num_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t idx = i * 3;

        // Input luminance (from scene-linear head_rgb)
        float in_r = head_rgb[idx];
        float in_g = head_rgb[idx + 1];
        float in_b = head_rgb[idx + 2];
        float in_lum = linear_rgb_to_luminance(in_r, in_g, in_b);
        in_lum = std::max(0.0f, std::min(1.0f, in_lum));

        // Target luminance (from sRGB reference jpeg)
        float ref_r = srgb_to_linear(ref_rgb8[idx]);
        float ref_g = srgb_to_linear(ref_rgb8[idx + 1]);
        float ref_b = srgb_to_linear(ref_rgb8[idx + 2]);
        float ref_lum = linear_rgb_to_luminance(ref_r, ref_g, ref_b);

        // Find the appropriate bin for this input luminance
        int bin = static_cast<int>(in_lum * (lute::TONE_CURVE_SIZE - 1) + 0.5f);
        bin = std::max(0, std::min(lute::TONE_CURVE_SIZE - 1, bin));

        // Accumulate data for this bin
        profile.tone_sum[bin] += ref_lum;
        profile.tone_count[bin]++;
    }

    profile.sample_count += static_cast<int>(num_pixels);
    profile.estimated = true; // Mark that the profile has learned data
}

void tone::apply(const float* in_rgb, float* out_rgb, int width, int height,
                   const lute::CameraLut& profile)
{
    float curve[lute::TONE_CURVE_SIZE];
    profile.tone_curve(curve); // Get the finalized tone curve

    const size_t num_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t idx = i * 3;
        float r = in_rgb[idx];
        float g = in_rgb[idx + 1];
        float b = in_rgb[idx + 2];

        float in_lum = linear_rgb_to_luminance(r, g, b);
        in_lum = std::max(0.0f, std::min(1.0f, in_lum));

        // Find the position in the curve array
        float curve_pos = in_lum * (lute::TONE_CURVE_SIZE - 1);
        int bin0 = static_cast<int>(curve_pos);
        int bin1 = std::min(lute::TONE_CURVE_SIZE - 1, bin0 + 1);
        float t = curve_pos - bin0;

        // Trilinearly interpolate the target luminance from the curve
        float target_lum = curve[bin0] * (1.0f - t) + curve[bin1] * t;

        // Calculate luminance ratio and apply it to the RGB channels
        // Avoid division by zero for black pixels
        float ratio = (in_lum > 1e-6f) ? (target_lum / in_lum) : 0.0f;

        out_rgb[idx] = r * ratio;
        out_rgb[idx + 1] = g * ratio;
        out_rgb[idx + 2] = b * ratio;
    }
}
