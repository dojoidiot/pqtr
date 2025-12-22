// lute.hpp - Camera Profile Learning
//
// LUTE learns camera-specific color transforms from RAW + embedded JPEG pairs.
//
// Model:
//   - TONE: 1D Tone Curve for Luminance
//   - TUNE: Three 1D curves for Hue, Saturation, and Value
// This provides a more structured, "expert-like" model for color correction.

#pragma once

#include <string>
#include <memory>
#include <vector>

namespace lute {

    // ============================================================
    // Constants
    // ============================================================

    constexpr int TONE_CURVE_SIZE = 256;
    constexpr int HUE_CURVE_SIZE = 360; // One bin per degree
    constexpr int SAT_CURVE_SIZE = 256;
    constexpr int VAL_CURVE_SIZE = 256;

    // ============================================================
    // Core Functions
    // ============================================================

    // Forward declaration
    struct CameraLut;

    // tune() - Accumulate flat -> target mappings
    //   (This function is now a high-level wrapper, logic is in plugins)
    bool tune(const float* flat, const uint8_t* target, int width, int height, CameraLut& lut, bool direct = false);

    // view() - Apply learned LUTs
    //   (This function is now a high-level wrapper, logic is in plugins)
    void view(const float* in, float* out, int width, int height, const CameraLut& lut);

    // Persistence
    bool save(const CameraLut& lut, const std::string& path);
    bool load(CameraLut& lut, const std::string& path);

    // ============================================================
    // CameraLut - Accumulator for learned curves
    // ============================================================

    struct CameraLut {
        // 1D tone curve (luminance)
        std::vector<double> tone_sum;
        std::vector<int> tone_count;

        // 1D hue curve
        std::vector<double> hue_sum;
        std::vector<int> hue_count;

        // 1D saturation curve
        std::vector<double> sat_sum;
        std::vector<int> sat_count;
        
        // 1D value curve
        std::vector<double> val_sum;
        std::vector<int> val_count;

        // Profile identity
        std::string camera_make;
        std::string camera_model;
        std::string creative_style;

        // State
        int sample_count = 0;
        bool frozen = false;
        bool estimated = false;

        CameraLut();
        void reset();

        // Profile key: "Sony_ILCE-7M4_Standard"
        std::string key() const;

        // Extract curve values (averages, identity for empty bins)
        void tone_curve(float* out) const;
        void hue_curve(float* out) const;
        void sat_curve(float* out) const;
        void val_curve(float* out) const;
    };

} // namespace lute
