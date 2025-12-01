// pipe.hpp
// Public API for the three-stage pipe: HEAD (decode) → BODY (process) → TAIL (save)
//
// User apps include only this header and link against labs.so.
// Implementation details are hidden (PIMPL pattern).

#pragma once

#include <hold.hpp>
#include <sink.hpp>
#include <string>
#include <map>
#include <memory>
#include <opencv2/core.hpp>

namespace pipe
{
    // ============================================================
    // Type Aliases
    // ============================================================

    using Info = std::map<std::string, std::string>;  // Metadata (EXIF, camera, etc.)
    using View = cv::UMat;                            // GPU-accelerated image matrix
    using Name = std::string;                         // Link identifier

    // ============================================================
    // Forward Declarations
    // ============================================================

    class Data;
    class Head;
    class Body;
    class Tail;

    // ============================================================
    // Data - Combines image view with metadata
    // ============================================================

    class Data
    {
    public:
        virtual ~Data() = default;
        virtual Info info() = 0;
        virtual View view() = 0;
    };

    // ============================================================
    // BODY - Processing pipeline with 6 golden modules (51 dials)
    // Style dials: 45 (optimized by geos)
    // Geometric dials: 6 (user composition, not optimized)
    // ============================================================

    class Body
    {
    public:
        virtual ~Body() = default;

        // ColourSpace contract for Tasks
        enum class ColourSpace
        {
            SPATIAL,          // Geometric operations (x,y coordinates)
            SCENE_LINEAR_RGB, // Camera-native linear RGB
            LINEAR_RGB,       // Working space (D65 white point)
            LCH,              // Perceptual color space (CIELAB cylindrical)
            SRGB              // Standard output (gamma-encoded)
        };

        // Base interface for all processing units
        class Task
        {
        public:
            virtual ~Task() = default;
            virtual View run(View view) = 0;
        };

        // --------------------------------------------------------
        // Link - A named collection of the 6 golden modules
        // --------------------------------------------------------

        class Link : public Task
        {
        public:
            virtual ~Link() = default;

            // Module 1: Geometric (6 dials) - SPATIAL
            class Geometric
            {
            public:
                virtual ~Geometric() = default;
                static constexpr ColourSpace space = ColourSpace::SPATIAL;

                class Crop : public Task
                {
                public:
                    virtual ~Crop() = default;
                    virtual float crop_top() = 0;
                    virtual void crop_top(float value) = 0;
                    virtual float crop_right() = 0;
                    virtual void crop_right(float value) = 0;
                    virtual float crop_bottom() = 0;
                    virtual void crop_bottom(float value) = 0;
                    virtual float crop_left() = 0;
                    virtual void crop_left(float value) = 0;
                };

                class Zoom : public Task
                {
                public:
                    virtual ~Zoom() = default;
                    virtual float scale() = 0;
                    virtual void scale(float value) = 0;
                };

                class Rotation : public Task
                {
                public:
                    virtual ~Rotation() = default;
                    virtual float tiltAngle() = 0;
                    virtual void tiltAngle(float value) = 0;
                };

                virtual Crop& crop() = 0;
                virtual Zoom& zoom() = 0;
                virtual Rotation& rotation() = 0;
            };

            // Module 2: Color Correction (3 dials) - LINEAR_RGB
            class ColorCorrection
            {
            public:
                virtual ~ColorCorrection() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;

                class Exposure : public Task
                {
                public:
                    virtual ~Exposure() = default;
                    virtual float get() = 0;
                    virtual void set(float value) = 0;
                };

                class WhiteBalance : public Task
                {
                public:
                    virtual ~WhiteBalance() = default;
                    virtual float temperature() = 0;
                    virtual void temperature(float value) = 0;
                    virtual float tint() = 0;
                    virtual void tint(float value) = 0;
                };

                virtual Exposure& exposure() = 0;
                virtual WhiteBalance& whiteBalance() = 0;
            };

            // Module 2.5: Base Curve (from RAW decoder) - LINEAR_RGB
            // NOT a dial-based module - curve derived by RAWS from RAW→preview
            // Applied early to bridge flat RAW → camera JPEG appearance gap
            class BaseCurve
            {
            public:
                virtual ~BaseCurve() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
                static constexpr int CURVE_LEN = 256;
                static constexpr int CURVE_CHANNELS = 3;
                static constexpr int CURVE_SIZE = CURVE_LEN * CURVE_CHANNELS;  // 768

                // Get the current curve (768 floats: B, G, R channels)
                virtual const float* curve() const = 0;

                // Set curve directly (called with head->baseCurve())
                virtual void setCurve(const float* values) = 0;

                // Reset to identity (no effect)
                virtual void reset() = 0;

                // Check if curve is non-identity
                virtual bool isActive() const = 0;
            };

            // Module 2.6: Polynomial Color (Camera Math) - LINEAR_RGB
            // NOT a dial-based module - coefficients derived by RAWS from RAW→preview
            // Captures camera's global RGB→RGB transform via quadratic polynomial
            class PolyColor
            {
            public:
                virtual ~PolyColor() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
                static constexpr int COEFFS_PER_CHANNEL = 10;
                static constexpr int COEFFS_SIZE = COEFFS_PER_CHANNEL * 3;  // 30

                // Get the current coefficients (30 floats: R, G, B channels)
                virtual const float* coeffs() const = 0;

                // Set coefficients directly (called with head->polyCoeffs())
                virtual void setCoeffs(const float* values) = 0;

                // Reset to identity (no effect)
                virtual void reset() = 0;

                // Check if coefficients are non-identity
                virtual bool isActive() const = 0;
            };

            // Module 2.7: 3D LUT (full RGB→RGB transform capture) - LINEAR_RGB
            // This is NOT a dial-based module - it stores a 3D LUT array
            // Used by tune to capture any color transform from base to target
            // 17³ grid = 4,913 cells × 3 channels = 14,739 parameters
            // Higher resolution captures finer hue distinctions (especially foliage greens)
            class LutCurve
            {
            public:
                virtual ~LutCurve() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
                static constexpr int GRID_SIZE = 17;  // 17³ = 4,913 cells
                static constexpr int LUT_SIZE = GRID_SIZE * GRID_SIZE * GRID_SIZE * 3;

                // Get/set the 3D LUT array (GRID_SIZE³ × 3 floats)
                virtual const float* lut() const = 0;
                virtual void setLut(const float* values) = 0;

                // Estimate 3D LUT from base image to target image
                // Returns true if estimation succeeded
                virtual bool estimate(View base, View target) = 0;

                // Reset to identity (no-op transform)
                virtual void reset() = 0;

                // Check if LUT is non-identity (has been estimated)
                virtual bool isEstimated() const = 0;
            };

            // Module 3: Tone Mapping (7 dials) - LINEAR_RGB
            class ToneMapping
            {
            public:
                virtual ~ToneMapping() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;

                class Contrast : public Task
                {
                public:
                    virtual ~Contrast() = default;
                    virtual float get() = 0;
                    virtual void set(float value) = 0;
                };

                class CurveAdjustment
                {
                public:
                    virtual ~CurveAdjustment() = default;

                    class Region : public Task
                    {
                    public:
                        virtual ~Region() = default;
                        virtual float get() = 0;
                        virtual void set(float value) = 0;
                    };

                    class Pivot : public Task
                    {
                    public:
                        virtual ~Pivot() = default;
                        virtual float get() = 0;
                        virtual void set(float value) = 0;
                    };

                    virtual Region& highlights() = 0;
                    virtual Region& shadows() = 0;
                    virtual Pivot& toePivot() = 0;
                    virtual Pivot& shoulderPivot() = 0;
                };

                class ClippingPoint
                {
                public:
                    virtual ~ClippingPoint() = default;

                    class Shade : public Task
                    {
                    public:
                        virtual ~Shade() = default;
                        virtual float get() = 0;
                        virtual void set(float value) = 0;
                    };

                    virtual Shade& black() = 0;
                    virtual Shade& white() = 0;
                };

                virtual Contrast& contrast() = 0;
                virtual CurveAdjustment& curveAdjustment() = 0;
                virtual ClippingPoint& clippingPoint() = 0;
            };

            // Module 4: Global Color (3 dials) - LCH
            class GlobalColor
            {
            public:
                virtual ~GlobalColor() = default;
                static constexpr ColourSpace space = ColourSpace::LCH;

                class Vibrance : public Task
                {
                public:
                    virtual ~Vibrance() = default;
                    virtual float get() = 0;
                    virtual void set(float value) = 0;
                };

                class Saturation : public Task
                {
                public:
                    virtual ~Saturation() = default;
                    virtual float get() = 0;
                    virtual void set(float value) = 0;
                };

                class ColourDensity : public Task
                {
                public:
                    virtual ~ColourDensity() = default;
                    virtual float get() = 0;
                    virtual void set(float value) = 0;
                };

                virtual Vibrance& vibrance() = 0;
                virtual Saturation& saturation() = 0;
                virtual ColourDensity& colourDensity() = 0;
            };

            // Module 4.5: Split Toning (4 dials) - LINEAR_RGB
            // Shadow/highlight color grading - different color casts for darks vs lights
            class SplitTone
            {
            public:
                virtual ~SplitTone() = default;
                static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;

                class TempTint : public Task
                {
                public:
                    virtual ~TempTint() = default;
                    virtual float temperature() = 0;  // 0=cool, 0.5=neutral, 1=warm
                    virtual void temperature(float value) = 0;
                    virtual float tint() = 0;         // 0=green, 0.5=neutral, 1=magenta
                    virtual void tint(float value) = 0;
                };

                virtual TempTint& shadows() = 0;
                virtual TempTint& highlights() = 0;
            };

            // Module 5: Selective Colour (24 dials) - LCH
            class SelectiveColour
            {
            public:
                virtual ~SelectiveColour() = default;
                static constexpr ColourSpace space = ColourSpace::LCH;

                class HslAdjust : public Task
                {
                public:
                    virtual ~HslAdjust() = default;
                    virtual float hue() = 0;
                    virtual void hue(float value) = 0;
                    virtual float saturation() = 0;
                    virtual void saturation(float value) = 0;
                    virtual float luminance() = 0;
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

            // Module 6: Detail (4 dials) - LINEAR_RGB/LCH
            class Detail
            {
            public:
                virtual ~Detail() = default;

                class Sharpen : public Task
                {
                public:
                    virtual ~Sharpen() = default;
                    static constexpr ColourSpace space = ColourSpace::LINEAR_RGB;
                    virtual float amount() = 0;
                    virtual void amount(float value) = 0;
                    virtual float radius() = 0;
                    virtual void radius(float value) = 0;
                };

                class Denoise
                {
                public:
                    virtual ~Denoise() = default;
                    static constexpr ColourSpace space = ColourSpace::LCH;

                    class Channel : public Task
                    {
                    public:
                        virtual ~Channel() = default;
                        virtual float get() = 0;
                        virtual void set(float value) = 0;
                    };

                    virtual Channel& luminance() = 0;
                    virtual Channel& chroma() = 0;
                };

                virtual Sharpen& sharpen() = 0;
                virtual Denoise& denoise() = 0;
            };

            // Link interface
            virtual Name name() = 0;
            virtual Geometric& geometric() = 0;
            virtual ColorCorrection& colorCorrection() = 0;
            virtual BaseCurve& baseCurve() = 0;
            virtual PolyColor& polyColor() = 0;
            virtual LutCurve& lutCurve() = 0;
            virtual ToneMapping& toneMapping() = 0;
            virtual GlobalColor& globalColor() = 0;
            virtual SplitTone& splitTone() = 0;
            virtual SelectiveColour& selectiveColour() = 0;
            virtual Detail& detail() = 0;
        };

        // --------------------------------------------------------
        // Link management
        // --------------------------------------------------------

        // Create a new link with the given name
        virtual Link& add(Name name) = 0;

        // Retrieve an existing link by name
        virtual Link& get(Name name) = 0;

        // Iterator for traversing links
        class Iterator
        {
        public:
            virtual ~Iterator() = default;
            virtual Link& current() = 0;
            virtual bool next() = 0;
            virtual void reset() = 0;
        };

        virtual Iterator& links() = 0;

        // Current state of image data and metadata (scene-linear RGB)
        virtual Data& data() = 0;

        // Run pipeline and return display-ready image
        // Processes all links, applies gamma encoding
        // Returns 8-bit BGR suitable for GUI display
        // max_dim: 0 = working size, >0 = scale to fit (can be smaller than working)
        virtual View view(int max_dim = 0) = 0;

        // Continue to tail for save operations (builder pattern)
        virtual Tail& tail() = 0;
    };

    // ============================================================
    // TAIL - Export using Head's full-resolution data
    // ============================================================

    class Tail
    {
    public:
        virtual ~Tail() = default;

        // Save to PNG file using Head's full-res data
        // Scales to max_dim BEFORE processing, then runs pipeline, applies gamma
        // max_dim: 0 = full resolution, >0 = scale to fit before processing
        virtual bool save(const std::string& path, int max_dim = 0) = 0;

        // Return display-ready image using Head's full-res data
        // Scales to max_dim BEFORE processing, then runs pipeline, applies gamma
        // Returns 8-bit BGR
        // max_dim: 0 = full resolution, >0 = scale to fit before processing
        virtual View view(int max_dim = 0) = 0;
    };

    // ============================================================
    // HEAD - Decodes RAW data from sink
    // ============================================================

    class Head
    {
    public:
        virtual ~Head() = default;

        // Access decoded image data and metadata for the pipeline.
        virtual Data& data() = 0;

        // Access the embedded camera made view image.
        virtual Data& view() = 0;

        // Base curve derived from RAW→preview comparison
        // Per-channel RGB: 768 floats [B0..B255, G0..G255, R0..R255]
        // Returns nullptr if no curve available
        static constexpr int CURVE_LEN = 256;
        static constexpr int CURVE_CHANNELS = 3;
        static constexpr int CURVE_SIZE = CURVE_LEN * CURVE_CHANNELS;  // 768
        virtual const float* baseCurve() const = 0;
        virtual bool hasBaseCurve() const = 0;

        // Note: Polynomial coefficients (Camera Math) are available in data().info()["poly_coeffs"]
        // as comma-separated floats if estimated during decode. Use parsePolyCoeffs() helper.

        // Continue to body processing (builder pattern)
        // working_size: 0 = full resolution, >0 = scale decoded data for faster preview
        virtual Body& body(int working_size = 0) = 0;
    };

    // ============================================================
    // Pipe - Entry point for the processing system
    // ============================================================

    class Pipe
    {
    public:
        virtual ~Pipe() = default;

        // Open RAW data (decoder auto-detected from file signature)
        // Returns Head for access to decoded data, then chain to Body, then Tail
        virtual pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) = 0;
    };

    // Factory function to create a Pipe instance
    pqtr::Hold<Pipe> make();

} // namespace pipe
