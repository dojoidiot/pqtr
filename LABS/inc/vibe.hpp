// vibe.hpp - PQTR Creative Style Module
//
// VIBE handles the photographer's creative adjustments - the dials they
// twist in Lightroom/Darktable to express their style.
//
// Key abstractions:
//   - Vibe: Style container (51 dials organized into modules)
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
// Plus non-dial transforms (from LUTE):
//   - BaseCurve (768 floats): camera response curve
//   - PolyColor (30 floats): polynomial coefficients
//   - LutCurve (14739 floats): 17^3 3D LUT
//   - HsvLut (1296 floats): 36x12 HSV delta LUT
//
// GPU compute via WGPU (WebGPU)
//
// Usage:
//   pipe->link(vibe::tune());  // Learn style from reference
//   pipe->link(vibe::play());  // Apply style to image

#pragma once

#include "pipe.hpp"
#include <string>
#include <memory>

namespace vibe {

    // ============================================================
    // Type Aliases
    // ============================================================

    using Name = std::string;
    using Dial = float;           // 0.0-1.0 normalized parameter
    using Grid = const float*;    // LUT/matrix data pointer

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
    // Vibe - Complete style container
    // ============================================================

    class Vibe {
    public:
        virtual ~Vibe() = default;

        // ==========================================================
        // Module 1: Geometric (6 dials) - SPATIAL
        // ==========================================================

        class Geometric {
        public:
            virtual ~Geometric() = default;
            static constexpr ColourSpace space = ColourSpace::SPATIAL;

            class Crop {
            public:
                virtual ~Crop() = default;
                virtual float top() const = 0;
                virtual void top(float value) = 0;
                virtual float right() const = 0;
                virtual void right(float value) = 0;
                virtual float bottom() const = 0;
                virtual void bottom(float value) = 0;
                virtual float left() const = 0;
                virtual void left(float value) = 0;
            };

            class Zoom {
            public:
                virtual ~Zoom() = default;
                virtual float scale() const = 0;
                virtual void scale(float value) = 0;
            };

            class Rotation {
            public:
                virtual ~Rotation() = default;
                virtual float angle() const = 0;
                virtual void angle(float value) = 0;
            };

            virtual Crop& crop() = 0;
            virtual Zoom& zoom() = 0;
            virtual Rotation& rotation() = 0;
        };

        // ==========================================================
        // Module 2: ColorCorrection (3 dials) - LINEAR_RGB
        // ==========================================================

        class ColorCorrection {
        public:
            virtual ~ColorCorrection() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;

            class Exposure {
            public:
                virtual ~Exposure() = default;
                virtual float get() const = 0;
                virtual void set(float value) = 0;
            };

            class WhiteBalance {
            public:
                virtual ~WhiteBalance() = default;
                virtual float temperature() const = 0;
                virtual void temperature(float value) = 0;
                virtual float tint() const = 0;
                virtual void tint(float value) = 0;
            };

            virtual Exposure& exposure() = 0;
            virtual WhiteBalance& whiteBalance() = 0;
        };

        // ==========================================================
        // Module 2.5: BaseCurve (768 floats) - LINEAR_RGB
        // Camera response curve from GEAR
        // ==========================================================

        class BaseCurve {
        public:
            virtual ~BaseCurve() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
            static constexpr int CURVE_LEN = 256;
            static constexpr int CURVE_CHANNELS = 3;
            static constexpr int CURVE_SIZE = CURVE_LEN * CURVE_CHANNELS;

            virtual Grid curve() const = 0;
            virtual void curve(Grid values) = 0;
            virtual void reset() = 0;
            virtual bool active() const = 0;
        };

        // ==========================================================
        // Module 2.6: PolyColor (30 floats) - LINEAR_RGB
        // Quadratic polynomial RGB->RGB transform
        // ==========================================================

        class PolyColor {
        public:
            virtual ~PolyColor() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
            static constexpr int COEFFS_PER_CHANNEL = 10;
            static constexpr int COEFFS_SIZE = COEFFS_PER_CHANNEL * 3;

            virtual Grid coeffs() const = 0;
            virtual void coeffs(Grid values) = 0;
            virtual void reset() = 0;
            virtual bool active() const = 0;
        };

        // ==========================================================
        // Module 2.7: LutCurve (14739 floats) - LINEAR_RGB
        // 17^3 3D LUT for full RGB->RGB transform capture
        // ==========================================================

        class LutCurve {
        public:
            virtual ~LutCurve() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
            static constexpr int GRID_SIZE = 17;
            static constexpr int LUT_SIZE = GRID_SIZE * GRID_SIZE * GRID_SIZE * 3;

            virtual Grid lut() const = 0;
            virtual void lut(Grid values) = 0;
            virtual void reset() = 0;
            virtual bool active() const = 0;
        };

        // ==========================================================
        // Module 2.8: HsvLut (1296 floats) - LINEAR_RGB
        // 36x12 HSV delta LUT for per-hue corrections
        // ==========================================================

        class HsvLut {
        public:
            virtual ~HsvLut() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
            static constexpr int H_BINS = 36;
            static constexpr int S_BINS = 12;
            static constexpr int LUT_SIZE = H_BINS * S_BINS * 3;

            virtual Grid lut() const = 0;
            virtual void lut(Grid values) = 0;
            virtual void reset() = 0;
            virtual bool active() const = 0;
        };

        // ==========================================================
        // Module 3: ToneMapping (7 dials) - LINEAR_RGB
        // ==========================================================

        class ToneMapping {
        public:
            virtual ~ToneMapping() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;

            class Contrast {
            public:
                virtual ~Contrast() = default;
                virtual float get() const = 0;
                virtual void set(float value) = 0;
            };

            class CurveAdjustment {
            public:
                virtual ~CurveAdjustment() = default;

                class Region {
                public:
                    virtual ~Region() = default;
                    virtual float get() const = 0;
                    virtual void set(float value) = 0;
                };

                class Pivot {
                public:
                    virtual ~Pivot() = default;
                    virtual float get() const = 0;
                    virtual void set(float value) = 0;
                };

                virtual Region& highlights() = 0;
                virtual Region& shadows() = 0;
                virtual Pivot& toePivot() = 0;
                virtual Pivot& shoulderPivot() = 0;
            };

            class ClippingPoint {
            public:
                virtual ~ClippingPoint() = default;

                class Shade {
                public:
                    virtual ~Shade() = default;
                    virtual float get() const = 0;
                    virtual void set(float value) = 0;
                };

                virtual Shade& black() = 0;
                virtual Shade& white() = 0;
            };

            virtual Contrast& contrast() = 0;
            virtual CurveAdjustment& curveAdjustment() = 0;
            virtual ClippingPoint& clippingPoint() = 0;
        };

        // ==========================================================
        // Module 4: GlobalColor (3 dials) - LCH
        // ==========================================================

        class GlobalColor {
        public:
            virtual ~GlobalColor() = default;
            static constexpr ColourSpace space = ColourSpace::LCH;

            class Vibrance {
            public:
                virtual ~Vibrance() = default;
                virtual float get() const = 0;
                virtual void set(float value) = 0;
            };

            class Saturation {
            public:
                virtual ~Saturation() = default;
                virtual float get() const = 0;
                virtual void set(float value) = 0;
            };

            class ColourDensity {
            public:
                virtual ~ColourDensity() = default;
                virtual float get() const = 0;
                virtual void set(float value) = 0;
            };

            virtual Vibrance& vibrance() = 0;
            virtual Saturation& saturation() = 0;
            virtual ColourDensity& colourDensity() = 0;
        };

        // ==========================================================
        // Module 4.5: SplitTone (4 dials) - LINEAR_RGB
        // ==========================================================

        class SplitTone {
        public:
            virtual ~SplitTone() = default;
            static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;

            class TempTint {
            public:
                virtual ~TempTint() = default;
                virtual float temperature() const = 0;
                virtual void temperature(float value) = 0;
                virtual float tint() const = 0;
                virtual void tint(float value) = 0;
            };

            virtual TempTint& shadows() = 0;
            virtual TempTint& highlights() = 0;
        };

        // ==========================================================
        // Module 5: SelectiveColour (24 dials) - LCH
        // ==========================================================

        class SelectiveColour {
        public:
            virtual ~SelectiveColour() = default;
            static constexpr ColourSpace space = ColourSpace::LCH;

            class HslAdjust {
            public:
                virtual ~HslAdjust() = default;
                virtual float hue() const = 0;
                virtual void hue(float value) = 0;
                virtual float saturation() const = 0;
                virtual void saturation(float value) = 0;
                virtual float luminance() const = 0;
                virtual void luminance(float value) = 0;
            };

            virtual HslAdjust& red() = 0;
            virtual HslAdjust& orange() = 0;
            virtual HslAdjust& yellow() = 0;
            virtual HslAdjust& green() = 0;
            virtual HslAdjust& cyan() = 0;
            virtual HslAdjust& blue() = 0;
            virtual HslAdjust& purple() = 0;
            virtual HslAdjust& magenta() = 0;
        };

        // ==========================================================
        // Module 6: Detail (4 dials) - LINEAR_RGB/LCH
        // ==========================================================

        class Detail {
        public:
            virtual ~Detail() = default;

            class Sharpen {
            public:
                virtual ~Sharpen() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
                virtual float amount() const = 0;
                virtual void amount(float value) = 0;
                virtual float radius() const = 0;
                virtual void radius(float value) = 0;
            };

            class Denoise {
            public:
                virtual ~Denoise() = default;
                static constexpr ColourSpace space = ColourSpace::LCH;

                class Channel {
                public:
                    virtual ~Channel() = default;
                    virtual float get() const = 0;
                    virtual void set(float value) = 0;
                };

                virtual Channel& luminance() = 0;
                virtual Channel& chroma() = 0;
            };

            virtual Sharpen& sharpen() = 0;
            virtual Denoise& denoise() = 0;
        };

        // ==========================================================
        // Module Access
        // ==========================================================

        virtual Geometric& geometric() = 0;
        virtual ColorCorrection& colorCorrection() = 0;
        virtual BaseCurve& baseCurve() = 0;
        virtual PolyColor& polyColor() = 0;
        virtual LutCurve& lutCurve() = 0;
        virtual HsvLut& hsvLut() = 0;
        virtual ToneMapping& toneMapping() = 0;
        virtual GlobalColor& globalColor() = 0;
        virtual SplitTone& splitTone() = 0;
        virtual SelectiveColour& selectiveColour() = 0;
        virtual Detail& detail() = 0;
    };

    // ============================================================
    // Link Factories
    // ============================================================

    pipe::Hold<pipe::Link> tune();  // Learn style from reference
    pipe::Hold<pipe::Link> play();  // Apply style to image

} // namespace vibe
