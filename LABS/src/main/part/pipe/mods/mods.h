// mods.h
// Pipe modules for display-referred processing
// Minimal UMat-based implementations
//
// Module order: Geometric → Color Correction → Tone Mapping → Global Color → Selective Color → Detail
// All modules operate on CV_32FC3 scene-linear sRGB unless noted

#pragma once

#include <opencv2/core.hpp>

namespace pipe
{
namespace mods
{
    //--------------------------------------------------------------------------
    // Geometric (6 dials)
    //--------------------------------------------------------------------------

    // Geometric transformations - crop, zoom, rotation
    // All dials: 0.0-1.0
    //   crop_top/right/bottom/left: Edge inset (0.0 = none, 1.0 = 50%)
    //   zoom: Zoom factor (0.0 = 1x, 1.0 = 4x)
    //   tilt_angle: Rotation (0.0 = -45°, 0.5 = 0°, 1.0 = +45°)
    bool geometric(
        const cv::UMat &input,
        cv::UMat &output,
        float crop_top = 0.0f,
        float crop_right = 0.0f,
        float crop_bottom = 0.0f,
        float crop_left = 0.0f,
        float zoom = 0.0f,
        float tilt_angle = 0.5f);

    //--------------------------------------------------------------------------
    // Color Correction (3 dials)
    //--------------------------------------------------------------------------

    // Exposure - brightness adjustment via EV shift
    // Dial: 0.0-1.0, default 0.5 (neutral)
    // Maps to: -4 EV to +4 EV
    bool exposure(
        const cv::UMat &input,
        cv::UMat &output,
        float dial);

    // White Balance - color temperature and tint
    // temperature: 0.0-1.0, default 0.5 (maps to 2000K-10000K)
    // tint: 0.0-1.0, default 0.5 (maps to -100 to +100)
    bool white_balance(
        const cv::UMat &input,
        cv::UMat &output,
        float temperature,
        float tint);

    //--------------------------------------------------------------------------
    // Base Curve (from RAWS decoder, no dials)
    //--------------------------------------------------------------------------

    // Apply base curve derived by RAWS from RAW→preview comparison
    // curve: 256 float values mapping gamma-space input to output [0-1]
    bool base_curve(
        const cv::UMat &input,
        cv::UMat &output,
        const float* curve);

    // Generate identity curve (no change)
    void base_curve_identity(float* curve);

    //--------------------------------------------------------------------------
    // Tone Mapping (7 dials)
    //--------------------------------------------------------------------------

    // Tone Mapping - HDR → SDR compression
    // Filmic curve with 7 dials for complete control
    //
    // All dials: 0.0-1.0, default 0.5 (neutral)
    //   contrast:       Global curve contrast (0.5→1.0 neutral)
    //   highlights:     Shoulder adjustment (0.5→0 neutral)
    //   shadows:        Toe adjustment (0.5→0 neutral)
    //   toe_pivot:      Where shadow region ends (0.5→0.3 luminance)
    //   shoulder_pivot: Where highlight region begins (0.5→0.7 luminance)
    //   white_point:    Scene white level (0.5→bypass)
    //   black_point:    Scene black level (0.5→0 neutral)
    bool tone_map(
        const cv::UMat &input,
        cv::UMat &output,
        float contrast = 0.5f,
        float highlights = 0.5f,
        float shadows = 0.5f,
        float toe_pivot = 0.5f,
        float shoulder_pivot = 0.5f,
        float white_point = 0.5f,
        float black_point = 0.5f);

    //--------------------------------------------------------------------------
    // Global Color (3 dials)
    //--------------------------------------------------------------------------

    // Global Color - vibrance, saturation, color density
    // Operates in Lab color space for perceptual uniformity
    //
    // All dials: 0.0-1.0, default 0.5 (neutral)
    //   vibrance:      Smart saturation with skin protection (0.5→0 neutral)
    //   saturation:    Global saturation multiplier (0.5→1.0 neutral)
    //   color_density: Color volume/intensity (0.5→1.0 neutral)
    bool global_color(
        const cv::UMat &input,
        cv::UMat &output,
        float vibrance = 0.5f,
        float saturation = 0.5f,
        float color_density = 0.5f);

    //--------------------------------------------------------------------------
    // Selective Color (24 dials)
    //--------------------------------------------------------------------------

    // Selective Color - HSL adjustments for 8 color bands
    // Each band has 3 dials: hue, saturation, luminance
    // Bands: red, orange, yellow, green, cyan, blue, purple, magenta
    //
    // All dials: 0.0-1.0, default 0.5 (neutral)
    //   hue[i]:        Hue shift for band i (0.5→0° neutral, range -30° to +30°)
    //   saturation[i]: Saturation adjust (0.5→0 neutral, range -1 to +1)
    //   luminance[i]:  Luminance adjust (0.5→0 neutral, range -1 to +1)
    //
    // Band indices: 0=red, 1=orange, 2=yellow, 3=green, 4=cyan, 5=blue, 6=purple, 7=magenta
    bool selective_color(
        const cv::UMat &input,
        cv::UMat &output,
        const float hue_dials[8],
        const float sat_dials[8],
        const float lum_dials[8]);

    //--------------------------------------------------------------------------
    // Detail + Output (4 dials)
    //--------------------------------------------------------------------------

    // Detail - sharpen and denoise
    // All dials: 0.0-1.0
    //   sharpen_amount: Sharpening strength (0.0 = none, 0.6 default, 1.0 = 2.0x)
    //   sharpen_radius: Sharpening radius (0.0 = 0.5px, 0.4 default, 1.0 = 3px)
    //   denoise_luma: Luminance denoise (0.0 = none, 0.3 default, 1.0 = max)
    //   denoise_chroma: Chroma denoise (0.0 = none, 0.5 default, 1.0 = max)
    bool detail(
        const cv::UMat &input,
        cv::UMat &output,
        float sharpen_amount = 0.6f,
        float sharpen_radius = 0.4f,
        float denoise_luma = 0.3f,
        float denoise_chroma = 0.5f);

    //--------------------------------------------------------------------------
    // Utility (automatic, no dials)
    //--------------------------------------------------------------------------

    // Color Matrix - camera RGB → sRGB (now in decoder, kept for reference)
    bool color_matrix(
        const cv::UMat &input,
        cv::UMat &output,
        const cv::Matx33f &matrix);

    //--------------------------------------------------------------------------
    // LUT-based Luminance Curve (for vibe transfer)
    //--------------------------------------------------------------------------

    // Apply a 1D LUT to remap luminance
    // lut: array of normalized (0-1) output values for input range 0-255
    // lut_size: number of entries in the LUT (typically 32)
    bool lut_curve(
        const cv::UMat &input,
        cv::UMat &output,
        const float* lut,
        int lut_size);

    // Estimate the luminance transfer curve from base to target
    // Fills lut with the mapping that transforms base luminance to target luminance
    // Used as a pre-pass before dial optimization
    bool estimate_lut(
        const cv::UMat& base,
        const cv::UMat& target,
        float* lut,
        int lut_size);

    //--------------------------------------------------------------------------
    // 3D LUT (for full color transform capture)
    //--------------------------------------------------------------------------

    // Apply a 3D LUT with trilinear interpolation
    // Maps input RGB tuples to output RGB tuples
    // lut: array of grid_size³ × 3 floats (R,G,B for each cell)
    // grid_size: dimension of cube (9 for social media quality)
    bool lut3d_apply(
        const cv::UMat& input,
        cv::UMat& output,
        const float* lut,
        int grid_size);

    // Estimate 3D LUT from base to target (deterministic, one-shot)
    // Bins all pixels by input RGB, averages target RGB per cell
    bool lut3d_estimate(
        const cv::UMat& base,
        const cv::UMat& target,
        float* lut,
        int grid_size);

    //--------------------------------------------------------------------------
    // Split Toning (shadow/highlight color grading) (4 dials)
    //--------------------------------------------------------------------------

    // Apply split toning - different color casts for shadows vs highlights
    // All dials: 0.0-1.0, default 0.5 (neutral)
    //   shadow_temp:    Shadow temperature (0=cool/blue, 0.5=neutral, 1=warm/yellow)
    //   shadow_tint:    Shadow tint (0=green, 0.5=neutral, 1=magenta)
    //   highlight_temp: Highlight temperature (same scale)
    //   highlight_tint: Highlight tint (same scale)
    bool split_tone(
        const cv::UMat& input,
        cv::UMat& output,
        float shadow_temp = 0.5f,
        float shadow_tint = 0.5f,
        float highlight_temp = 0.5f,
        float highlight_tint = 0.5f);

} // namespace mods
} // namespace pipe
