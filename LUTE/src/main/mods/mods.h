// mods.h - LUTE internal module functions
//
// Camera profile transforms (4 types):
//   - BaseCurve: Per-channel tone response (768 floats)
//   - PolyColor: Polynomial color transform (30 floats)
//   - LutCurve: 17³ 3D LUT for full color mapping (14739 floats)
//   - HsvLut: HSV delta corrections (1296 floats)

#pragma once

#include <opencv2/core.hpp>

namespace lute
{
    using View = cv::UMat;
    using Grid = const float*;

namespace mods
{
    //--------------------------------------------------------------------------
    // BaseCurve - Per-channel tone response (768 floats: 3 x 256)
    //--------------------------------------------------------------------------

    static constexpr int CURVE_LEN = 256;
    static constexpr int CURVE_CHANNELS = 3;
    static constexpr int CURVE_SIZE = CURVE_LEN * CURVE_CHANNELS;

    bool base_curve(const View& in, View& out, Grid curve);
    void base_curve_identity(float* curve);

    //--------------------------------------------------------------------------
    // PolyColor - Polynomial color transform (30 floats: 3 x 10)
    //--------------------------------------------------------------------------

    static constexpr int POLY_COEFFS = 10;
    static constexpr int POLY_TOTAL = 30;

    bool poly_color(const View& in, View& out, Grid coeffs);
    bool estimate_poly_color(const View& base, const View& target, float* coeffs, int samples = 50000);
    void identity_poly_color(float* coeffs);

    //--------------------------------------------------------------------------
    // LutCurve - Per-channel curve (lut_size x 3)
    //--------------------------------------------------------------------------

    static constexpr int LUT_GRID_SIZE = 17;
    static constexpr int LUT_CURVE_SIZE = LUT_GRID_SIZE * 3;

    bool lut_curve(const View& in, View& out, Grid lut, int lut_size);
    bool estimate_lut(const View& base, const View& target, float* lut, int lut_size);

    //--------------------------------------------------------------------------
    // HsvLut - HSV delta table (36 hue x 12 sat x 3 delta = 1296)
    //--------------------------------------------------------------------------

    static constexpr int HSV_H_BINS = 36;
    static constexpr int HSV_S_BINS = 12;
    static constexpr int HSV_LUT_SIZE = HSV_H_BINS * HSV_S_BINS * 3;

    bool hsv_lut_apply(const View& in, View& out, Grid lut);
    bool hsv_lut_estimate(const View& base, const View& target, float* lut);
    void hsv_lut_identity(float* lut);

} // namespace mods
} // namespace lute
