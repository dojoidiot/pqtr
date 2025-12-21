// lute.hpp - Camera Profile Learning
//
// LUTE learns camera-specific color transforms from RAW + embedded JPEG pairs.
// Each shot improves the LUT for that camera's style.
//
// Model:
//   1. flow::Flow provides flat (scene-linear RGB after HEAD pipeline)
//   2. flow::Flow provides target (embedded JPEG from camera)
//   3. tune() accumulates flat->target mappings into 17^3 LUT
//   4. view() applies learned LUT to produce camera-style output
//
// Profile key: Camera_Model_Style (e.g., "Sony_ILCE-7M4_Standard")

#pragma once

#include <string>
#include <memory>
#include <vector>

namespace lute {

    // ============================================================
    // Constants
    // ============================================================

    // 3D color LUT
    constexpr int GRID_SIZE = 17;
    constexpr int CELLS = GRID_SIZE * GRID_SIZE * GRID_SIZE;  // 4,913
    constexpr int LUT_SIZE = CELLS * 3;  // 14,739 floats

    // 1D tone curve (applied before 3D LUT)
    constexpr int CURVE_SIZE = 256;  // 256 luminance bins

    // ============================================================
    // Core Functions
    // ============================================================

    // Forward declaration
    struct CameraLut;

    // tune() - Accumulate flat -> target mappings
    //   flat:   scene-linear RGB [0,1], w*h*3 floats
    //   target: camera JPEG RGB [0,255], w*h*3 uint8
    //   direct: if true, skip ratio adjustment (use for ACES input)
    //   Returns true on success
    bool tune(const float* flat, const uint8_t* target, int width, int height, CameraLut& lut, bool direct = false);

    // view() - Apply learned LUT with trilinear interpolation
    //   in:  scene-linear RGB [0,1], w*h*3 floats
    //   out: camera-style RGB [0,1], w*h*3 floats (must be pre-allocated)
    void view(const float* in, float* out, int width, int height, const CameraLut& lut);

    // Persistence
    bool save(const CameraLut& lut, const std::string& path);
    bool load(CameraLut& lut, const std::string& path);

    // ============================================================
    // CameraLut - 17^3 grid accumulator
    // ============================================================

    struct CameraLut {
        // 3D LUT accumulators (double for precision)
        std::vector<double> sum;       // CELLS * 3 RGB sums
        std::vector<double> prev_avg;  // for delta tracking
        std::vector<int> count;        // CELLS sample counts

        // 1D tone curve accumulators
        std::vector<double> curve_sum;   // CURVE_SIZE output luminance sums
        std::vector<int> curve_count;    // CURVE_SIZE sample counts

        // Profile identity
        std::string camera_make;
        std::string camera_model;
        std::string creative_style;

        // Convergence state
        int sample_count = 0;
        float last_delta = 1.0f;
        bool frozen = false;
        bool estimated = false;

        CameraLut();
        void reset();

        // Profile key: "Sony_ILCE-7M4_Standard"
        std::string key() const;

        // Extract LUT values (averages, identity for empty cells)
        void lut(float* out) const;

        // Extract tone curve (averages, identity for empty bins)
        void curve(float* out) const;

        // Coverage: fraction of cells with data (0.0 to 1.0)
        float coverage() const;

        // Empty cells count
        int emptyCells() const;

        // Convergence tracking
        void snapshot();           // Save current state for delta
        float computeDelta() const; // Average change since snapshot
        bool converged(float threshold = 0.001f) const;

        // Suggest scenes to photograph for better coverage
        std::vector<std::string> missing() const;
    };

} // namespace lute
