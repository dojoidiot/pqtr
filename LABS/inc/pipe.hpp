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
    // BODY - Processing pipeline with 6 golden modules (45 dials)
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

            // Module 3: Tone Mapping (5 dials) - LINEAR_RGB
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

                    virtual Region& highlights() = 0;
                    virtual Region& shadows() = 0;
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
            virtual ToneMapping& toneMapping() = 0;
            virtual GlobalColor& globalColor() = 0;
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

        // Current state of image data and metadata
        virtual Data& data() = 0;

        // Continue to tail (builder pattern)
        virtual Tail& tail() = 0;
    };

    // ============================================================
    // TAIL - Finalizes processing and outputs PNG
    // ============================================================

    class Tail
    {
    public:
        virtual ~Tail() = default;

        // Save to PNG file (gamma encoding applied internally)
        virtual bool save(const std::string& path) = 0;
    };

    // ============================================================
    // HEAD - Decodes RAW data from sink
    // ============================================================

    class Head
    {
    public:
        virtual ~Head() = default;

        // Access decoded image data and metadata
        virtual Data& data() = 0;

        // Continue to body processing (builder pattern)
        virtual Body& body() = 0;
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
