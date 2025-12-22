#include "tune.hpp"
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>

namespace
{
    struct HSV { float h, s, v; };

    // r,g,b inputs are [0,1]
    // h is [0,360], s is [0,1], v is [0,1]
    static HSV rgb_to_hsv(float r, float g, float b) {
        float max_val = std::max(r, std::max(g, b));
        float min_val = std::min(r, std::min(g, b));
        float delta = max_val - min_val;

        HSV hsv;
        hsv.v = max_val;

        if (delta < 1e-6f) {
            hsv.h = 0.0f;
            hsv.s = 0.0f;
        } else {
            hsv.s = delta / max_val;
            if (r >= max_val) {
                hsv.h = 60.0f * fmod(((g - b) / delta), 6.0f);
            } else if (g >= max_val) {
                hsv.h = 60.0f * (((b - r) / delta) + 2.0f);
            } else {
                hsv.h = 60.0f * (((r - g) / delta) + 4.0f);
            }
            if (hsv.h < 0.0f) {
                hsv.h += 360.0f;
            }
        }
        return hsv;
    }

    static void hsv_to_rgb(float h, float s, float v, float& r, float& g, float& b) {
        if (s < 1e-6f) {
            r = g = b = v;
            return;
        }

        float c = v * s;
        float x = c * (1.0f - std::abs(fmod(h / 60.0f, 2.0f) - 1.0f));
        float m = v - c;

        if (h >= 0 && h < 60) {
            r = c; g = x; b = 0;
        } else if (h >= 60 && h < 120) {
            r = x; g = c; b = 0;
        } else if (h >= 120 && h < 180) {
            r = 0; g = c; b = x;
        } else if (h >= 180 && h < 240) {
            r = 0; g = x; b = c;
        } else if (h >= 240 && h < 300) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }

        r += m; g += m; b += m;
    }

    // Helper to apply a 1D LUT with interpolation
    static float apply_curve(float val, const float* curve, int size)
    {
        float curve_pos = val * (size - 1);
        int bin0 = std::max(0, std::min(size - 1, static_cast<int>(curve_pos)));
        int bin1 = std::max(0, std::min(size - 1, bin0 + 1));
        float t = curve_pos - bin0;
        return curve[bin0] * (1.0f - t) + curve[bin1] * t;
    }

} // namespace

void tune::learn(const float* tone_rgb, int width, int height,
                   const float* ref_rgb, int ref_width, int ref_height,
                   lute::CameraLut& profile)
{
    if (width != ref_width || height != ref_height) return;

    profile.hue_sum.assign(lute::HUE_CURVE_SIZE, 0.0);
    profile.hue_count.assign(lute::HUE_CURVE_SIZE, 0);
    profile.sat_sum.assign(lute::SAT_CURVE_SIZE, 0.0);
    profile.sat_count.assign(lute::SAT_CURVE_SIZE, 0);
    profile.val_sum.assign(lute::VAL_CURVE_SIZE, 0.0);
    profile.val_count.assign(lute::VAL_CURVE_SIZE, 0);

    const size_t num_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t idx = i * 3;

        // Convert input and reference to HSV
        HSV in_hsv = rgb_to_hsv(tone_rgb[idx], tone_rgb[idx+1], tone_rgb[idx+2]);
        HSV ref_hsv = rgb_to_hsv(ref_rgb[idx], ref_rgb[idx+1], ref_rgb[idx+2]);

        // Accumulate for Hue curve
        int hue_bin = std::min(lute::HUE_CURVE_SIZE - 1, static_cast<int>(in_hsv.h));
        profile.hue_sum[hue_bin] += ref_hsv.h;
        profile.hue_count[hue_bin]++;

        // Accumulate for Saturation curve
        int sat_bin = std::min(lute::SAT_CURVE_SIZE - 1, static_cast<int>(in_hsv.s * (lute::SAT_CURVE_SIZE - 1)));
        profile.sat_sum[sat_bin] += ref_hsv.s;
        profile.sat_count[sat_bin]++;
        
        // Accumulate for Value curve
        int val_bin = std::min(lute::VAL_CURVE_SIZE - 1, static_cast<int>(in_hsv.v * (lute::VAL_CURVE_SIZE - 1)));
        profile.val_sum[val_bin] += ref_hsv.v;
        profile.val_count[val_bin]++;
    }
}

void tune::apply(const float* in_rgb, float* out_rgb, int width, int height,
                   const lute::CameraLut& profile)
{
    // Extract learned curves
    float h_curve[lute::HUE_CURVE_SIZE];
    float s_curve[lute::SAT_CURVE_SIZE];
    float v_curve[lute::VAL_CURVE_SIZE];
    profile.hue_curve(h_curve);
    profile.sat_curve(s_curve);
    profile.val_curve(v_curve);

    const size_t num_pixels = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < num_pixels; ++i)
    {
        const size_t idx = i * 3;
        
        // 1. Convert to HSV
        HSV hsv = rgb_to_hsv(in_rgb[idx], in_rgb[idx+1], in_rgb[idx+2]);
        
        // 2. Apply learned curves
        hsv.h = apply_curve(hsv.h / 360.0f, h_curve, lute::HUE_CURVE_SIZE) * 360.0f;
        hsv.s = apply_curve(hsv.s, s_curve, lute::SAT_CURVE_SIZE);
        hsv.v = apply_curve(hsv.v, v_curve, lute::VAL_CURVE_SIZE);

        // 3. Convert back to RGB
        hsv_to_rgb(hsv.h, hsv.s, hsv.v, out_rgb[idx], out_rgb[idx+1], out_rgb[idx+2]);
    }
}
