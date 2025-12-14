// lute.hpp - PQTR Camera Profile Module
//
// LUTE learns and applies camera-specific color transforms.
// This is the "gear manufacturer's style" - what photographers see
// on their LCD when composing shots.
//
// Camera profiles are learned from RAW + embedded JPEG pairs:
//   1. GEAR decodes RAW to scene-linear
//   2. LUTE accumulates RGB->RGB mappings into LUTs
//   3. Profile converges across multiple images
//
// Transforms (learned from camera behavior):
//   - BaseCurve: Camera tone response curve
//   - PolyColor: Polynomial color transform
//   - LutCurve: 17^3 3D LUT (full color mapping)
//   - HsvLut: HSV delta corrections
//
// GPU compute via WGPU (WebGPU)
//
// NOTE: Implementation pending WGPU port

#pragma once

#include "pipe.hpp"
#include <string>
#include <memory>

namespace lute {

    // ============================================================
    // Type Aliases
    // ============================================================

    using Name = std::string;
    using Hold = std::unique_ptr<class Lute>;

    // ============================================================
    // Transform sizes (for serialization)
    // ============================================================

    constexpr int BASE_CURVE_SIZE = 256 * 3;     // 768 floats
    constexpr int POLY_COLOR_SIZE = 10 * 3;       // 30 floats
    constexpr int LUT_CURVE_SIZE = 17 * 17 * 17 * 3; // 14739 floats
    constexpr int HSV_LUT_SIZE = 36 * 12 * 3;    // 1296 floats

    // ============================================================
    // Lute - Camera profile interface
    // ============================================================
    //
    // Implementation pending WGPU port

    class Lute {
    public:
        virtual ~Lute() = default;

        // Profile identity
        virtual Name key() const = 0;

        // Apply profile to image (via WGPU)
        virtual pipe::Data view(pipe::Data in) = 0;

        // Learn profile from RAW+preview pair
        virtual bool tune(pipe::Data flat, pipe::Data preview) = 0;

        // Persistence
        virtual bool save(const Name& path) const = 0;
        virtual bool load(const Name& path) = 0;
    };

    // Factory (returns nullptr until WGPU implementation)
    Hold create();
    Hold create(const Name& cameraModel,
                const Name& creativeStyle,
                const Name& dro);

} // namespace lute
