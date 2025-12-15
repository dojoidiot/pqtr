// vibe.hpp - PQTR Creative Style Module
//
// VIBE handles the photographer's creative adjustments - the dials they
// twist in Lightroom/Darktable to express their style.
//
// Key abstractions:
//   - Vibe: Style container (51 dials organized into modules)
//   - Dial: Individual adjustable parameter with view() and tune()
//   - Module: Group of related dials operating in same color space
//
// Modules (51 dials total):
//   - Geometric (6): crop, zoom, rotation
//   - ColorCorrection (3): exposure, white balance
//   - ToneMapping (7): contrast, highlights, shadows, pivots, clips
//   - GlobalColor (3): vibrance, saturation, density
//   - SplitTone (4): shadow/highlight color grading
//   - SelectiveColour (24): per-hue HSL adjustments
//   - Detail (4): sharpen, denoise
//
// GPU compute via WGPU (WebGPU)
//
// NOTE: Implementation pending WGPU port

#pragma once

#include "pipe.hpp"
#include <string>
#include <memory>

namespace vibe {

    // ============================================================
    // Type Aliases
    // ============================================================

    using Name = std::string;
    using Dial = float;  // 0.0-1.0 normalized parameter

    // ============================================================
    // ColourSpace - Processing domain for each module
    // ============================================================

    enum class ColourSpace {
        SPATIAL,          // Geometric operations (x,y coordinates)
        SCENE_LINEAR_RGB, // Camera-native linear RGB
        LINEAR_RGB,       // Working space (D65 white point)
        LCH,              // Perceptual color space (CIELAB cylindrical)
        SRGB              // Standard output (gamma-encoded)
    };

    // ============================================================
    // Vibe - Creative style interface
    // ============================================================
    //
    // Implementation pending WGPU port

    class Vibe {
    public:
        virtual ~Vibe() = default;

        // Style name
        virtual Name name() const = 0;

        // Apply style to image (via WGPU)
        virtual pipe::Data view(pipe::Data in) = 0;

        // Learn style from reference image
        virtual pipe::Data tune(pipe::Data in, pipe::Data reference) = 0;

        // Persistence
        virtual bool save(const Name& path) = 0;
        virtual bool load(const Name& path) = 0;
    };

    // Factory (returns nullptr until WGPU implementation)
    std::unique_ptr<Vibe> create();
    std::unique_ptr<Vibe> create(const Name& path);

} // namespace vibe
