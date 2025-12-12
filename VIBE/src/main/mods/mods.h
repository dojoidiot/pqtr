// mods.h - VIBE internal module functions
//
// All image transforms live here. Each mod has:
//   - CV implementation (OpenCV UMat)
//   - DAWN implementation (WGSL shaders) [future]
//
// Dial mods (45 total):
//   geometric (6), exposure (1), white_balance (2), tone_map (7),
//   global_color (3), selective_color (24), split_tone (4), detail (4)
//
// Meta mods (no dials, metadata-driven):
//   baseline, sigmoid, color_matrix, base_curve, poly_color,
//   lut_curve, lut3d, hsv_lut, local_tone

#pragma once

#include <opencv2/core.hpp>

namespace vibe
{
    // Type aliases
    using View = cv::UMat;
    using Dial = float;           // 0.0-1.0 normalized parameter
    using Grid = const float*;    // LUT/matrix data pointer

namespace mods
{
    //--------------------------------------------------------------------------
    // Generic Camera Baseline (no dials)
    //--------------------------------------------------------------------------

    bool highlight_recovery(const View& in, View& out, float clip = 0.95f);
    bool baseline(const View& in, View& out, float ev = 0.7f, float clip = 0.95f);
    bool baseline_default(const View& in, View& out);

    //--------------------------------------------------------------------------
    // Geometric (6 dials)
    //--------------------------------------------------------------------------

    bool geometric(const View& in, View& out,
        Dial crop_top = 0.0f,
        Dial crop_right = 0.0f,
        Dial crop_bottom = 0.0f,
        Dial crop_left = 0.0f,
        Dial zoom = 0.0f,
        Dial tilt = 0.5f);

    //--------------------------------------------------------------------------
    // Color Correction (3 dials)
    //--------------------------------------------------------------------------

    bool exposure(const View& in, View& out, Dial dial);
    bool exposure_ev(const View& in, View& out, float ev);  // Direct EV, not a dial
    bool white_balance(const View& in, View& out, Dial temperature, Dial tint);

    //--------------------------------------------------------------------------
    // Base Curve (from GEAR, no dials)
    //--------------------------------------------------------------------------

    bool base_curve(const View& in, View& out, Grid curve);
    void base_curve_identity(float* curve);

    //--------------------------------------------------------------------------
    // Sigmoid (scene-referred tone mapping, no dials)
    //--------------------------------------------------------------------------

    bool sigmoid(const View& in, View& out,
        float contrast = 1.5f,
        float skewness = 0.0f,
        float white_target = 1.0f,
        float black_target = 0.000152f);

    bool sigmoid_default(const View& in, View& out);

    //--------------------------------------------------------------------------
    // Tone Mapping (7 dials)
    //--------------------------------------------------------------------------

    bool tone_map(const View& in, View& out,
        Dial contrast = 0.5f,
        Dial highlights = 0.5f,
        Dial shadows = 0.5f,
        Dial toe_pivot = 0.5f,
        Dial shoulder_pivot = 0.5f,
        Dial white_point = 0.5f,
        Dial black_point = 0.5f);

    //--------------------------------------------------------------------------
    // Global Color (3 dials)
    //--------------------------------------------------------------------------

    bool global_color(const View& in, View& out,
        Dial vibrance = 0.5f,
        Dial saturation = 0.5f,
        Dial color_density = 0.5f);

    //--------------------------------------------------------------------------
    // Selective Color (24 dials: 8 bands x 3 HSL)
    //--------------------------------------------------------------------------

    bool selective_color(const View& in, View& out,
        const Dial hue[8],
        const Dial sat[8],
        const Dial lum[8]);

    //--------------------------------------------------------------------------
    // Split Tone (4 dials)
    //--------------------------------------------------------------------------

    bool split_tone(const View& in, View& out,
        Dial shadow_temp = 0.5f,
        Dial shadow_tint = 0.5f,
        Dial highlight_temp = 0.5f,
        Dial highlight_tint = 0.5f);

    //--------------------------------------------------------------------------
    // Detail (4 dials)
    //--------------------------------------------------------------------------

    bool detail(const View& in, View& out,
        Dial sharpen_amount = 0.6f,
        Dial sharpen_radius = 0.4f,
        Dial denoise_luma = 0.3f,
        Dial denoise_chroma = 0.5f);

    //--------------------------------------------------------------------------
    // Utility (no dials)
    //--------------------------------------------------------------------------

    bool color_matrix(const View& in, View& out, const cv::Matx33f& matrix);

    //--------------------------------------------------------------------------
    // LUT-based transforms (no dials, estimated from pairs)
    //--------------------------------------------------------------------------

    // 1D luminance LUT
    bool lut_curve(const View& in, View& out, Grid lut, int lut_size);
    bool estimate_lut(const View& base, const View& target, float* lut, int lut_size);

    // HSV LUT (36 hue x 12 sat x 3 delta)
    static constexpr int HSV_H_BINS = 36;
    static constexpr int HSV_S_BINS = 12;
    static constexpr int HSV_LUT_SIZE = HSV_H_BINS * HSV_S_BINS * 3;  // 1296

    bool hsv_lut_apply(const View& in, View& out, Grid lut);
    bool hsv_lut_estimate(const View& base, const View& target, float* lut);
    void hsv_lut_identity(float* lut);

    // 3D LUT (grid^3 x 3)
    bool lut3d_apply(const View& in, View& out, Grid lut, int grid_size);
    bool lut3d_estimate(const View& base, const View& target, float* lut, int grid_size);

    //--------------------------------------------------------------------------
    // Polynomial Color (no dials, 30 coeffs)
    //--------------------------------------------------------------------------

    static constexpr int POLY_COEFFS = 10;   // Per channel
    static constexpr int POLY_TOTAL = 30;    // 3 channels

    bool poly_color(const View& in, View& out, Grid coeffs);
    bool estimate_poly_color(const View& base, const View& target, float* coeffs, int samples = 50000);
    void identity_poly_color(float* coeffs);

    //--------------------------------------------------------------------------
    // Local Tone (no dials, Iridix-style)
    //--------------------------------------------------------------------------

    bool local_tone(const View& in, View& out,
        float strength = 0.5f,
        float delta = 0.02f,
        float window_scale = 0.1f);

    bool estimate_local_tone(const View& base, const View& target,
        float& strength, float& delta, float& window_scale);

} // namespace mods
} // namespace vibe
