#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * This is the flow namespace.  This is designed for me to read - it must be maintained as minimal interface,
 * PIMPL.  No boilerplate, low cognitive load.
 *
 * DO NOT ADD CODE HERE.
 *
 * DO NOT CHANGE THIS COMMENT.
 */
namespace flow
{

    // ============================================================================
    // Field Keys
    // ============================================================================

    constexpr const char *NAME = "name";
    constexpr const char *WIDTH = "width";
    constexpr const char *HEIGHT = "height";
    constexpr const char *BLACK = "black";
    constexpr const char *WHITE = "white";

    // ============================================================================
    // Colorspace tracking (Rule 7: assert before each module test)
    // ============================================================================

    enum Colorspace {
        BAYER,          // Bayer mosaic (float32, before demosaic)
        LINEAR_RGB,     // Scene-referred linear RGB (float32)
        LAB,            // CIE Lab D50 (float32)
        DISPLAY_SRGB    // Display-referred sRGB (uint8 after gamma)
    };

    // Returns colorspace name for logging
    inline const char* colorspace_name(Colorspace cs) {
        switch (cs) {
            case BAYER: return "BAYER";
            case LINEAR_RGB: return "LINEAR_RGB";
            case LAB: return "LAB";
            case DISPLAY_SRGB: return "DISPLAY_SRGB";
        }
        return "UNKNOWN";
    }

    // ============================================================================
    // Tree - hierarchical metadata (PIMPL)
    // ============================================================================

    // Base class for tree nodes
    class Node
    {
    public:
        virtual ~Node() = default;
    };

    // Leaf node - holds a single value (dial or text)
    class Leaf : public Node
    {
    public:
        virtual float dial() = 0;                // get numeric value
        virtual void dial(float val) = 0;        // set numeric value
        virtual std::string &text() = 0;         // get string value
        virtual void text(std::string &val) = 0; // set string value
        virtual bool live() = 0;                 // true if value has been set
    };

    // Stem node - holds child nodes (branches and leaves)
    class Stem : public Node
    {
    public:
        virtual Stem &next(const std::string &name) = 0;      // get/create child stem
        virtual Leaf &leaf(const std::string &name) = 0;      // get/create child leaf
        virtual Node *find(const std::string &name) = 0;      // find child (null if missing)
        virtual size_t size() const = 0;                      // number of children
        virtual bool test(const std::string &name) const = 0; // check if child exists
        virtual void tidy() = 0;                              // remove empty children
        virtual std::vector<std::string> list() = 0;          // list child names
    };

    // Tree root - owns the hierarchy
    class Tree
    {
    public:
        virtual ~Tree() = default;
        virtual Stem &root() = 0;                // get root stem
        virtual std::string json() = 0;          // serialize to JSON
        virtual void read(std::string json) = 0; // deserialize from JSON
    };

    // ============================================================================
    // Flow - RAW image container
    // ============================================================================
    //
    // NOTE: Embedded JPEG preview is NOT included in Flow.
    // Sony ARW format varies by camera generation - older cameras store
    // full JPEG separately, newer ones embed JPEG+thumb in ARW.
    // Preview extraction is a separate concern outside the pipe.

    class Flow
    {
    public:
        virtual ~Flow() = default;
        virtual Tree &info() = 0;      // metadata tree
        virtual uint16_t *data() = 0;  // raw bayer data (width * height)
        virtual float *fdata() = 0;    // normalized float bayer (null until rawprepare)
        virtual float *rgb() = 0;      // RGB float data (width * height * 4, RGBX)
    };
    // ============================================================
    // Link - Processing unit
    // ============================================================
    //
    // Modules contribute Links to the pipe.
    // Links have a name and type for identification.

    class Link
    {
    public:
        virtual ~Link() = default;

        virtual std::string name() const = 0;
        // Save link as Json.  If link has links - like the pipe does - then recurse.
        virtual std::string save() = 0;
        // Load link from Json.  If link has links - like the pipe does - then recurse.
        virtual void load(const std::string& json) = 0;
    };

    // The head link; takes the raw arw file bytes, and makes the flow object for the pipe.
    class Head : public Link
    {
    public:
        virtual std::unique_ptr<Flow> decode(const uint8_t *bytes, size_t size) = 0;
    };

    // Rawprepare link; black level subtraction and normalization.
    // Input: uint16 bayer in data(), Output: float bayer in fdata()
    class Rawprepare : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
    };

    // Demosaic link; bayer interpolation to RGB.
    // Input: float bayer in fdata(), Output: float RGB in rgb()
    class Demosaic : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
    };

    // Highlights link; highlight reconstruction on Bayer mosaic.
    // Input/Output: float bayer in fdata() (in-place, before demosaic)
    class Highlights : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        virtual void setClip(float clip) = 0;
    };

    // Temperature link; white balance correction.
    // Input/Output: float RGB in rgb() (in-place)
    class Temperature : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        virtual void setCoeffs(float r, float g, float b) = 0;
    };

    // Exposure link; scene-referred brightness adjustment.
    // Input/Output: float RGB in rgb() (in-place)
    class Exposure : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        virtual void setParams(float ev, float black) = 0;
    };

    // Channelmixer link; chromatic adaptation (CAT16).
    // Input/Output: float RGB in rgb() (in-place)
    class Channelmixer : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        virtual void setParams(float x, float y, float temp) = 0;
    };

    // Channelmixerrgb link; RGB channel mixing with 3x3 matrix.
    // Input/Output: float RGB in rgb() (in-place)
    // IOP order: 28.5 (after exposure, before colorbalancergb)
    class Channelmixerrgb : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        // Set 3x3 mixing matrix (each row is [R,G,B,unused])
        virtual void setMatrix(const float red[4], const float green[4], const float blue[4]) = 0;
        // Optional adjustments
        virtual void setSaturation(const float sat[4]) = 0;
        virtual void setLightness(const float light[4]) = 0;
        virtual void setGrey(const float grey[4]) = 0;
    };

    // Colorin link; camera RGB → XYZ → Lab (D50).
    // Input/Output: float RGB in rgb() (in-place, becomes Lab)
    class Colorin : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
    };

    // Colorout link; Lab → XYZ → linear sRGB (D50 adapted).
    // Input/Output: float data in rgb() (Lab in, linear sRGB out)
    class Colorout : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
    };

    // Gamma link; sRGB transfer function only.
    // Input: linear sRGB in rgb(), Output: display sRGB
    class Gamma : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
    };

    // Sigmoid link; scene-referred tone mapping.
    // Input: linear sRGB in rgb(), Output: display sRGB (replaces gamma)
    class Sigmoid : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        virtual void setParams(float contrast, float white_target, float black_target, float hue_preservation = 100.0f) = 0;
    };

    // Filmicrgb link; scene-referred filmic tone mapping.
    // Input: linear sRGB in rgb(), Output: display sRGB
    class Filmicrgb : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        virtual void setParams(float grey, float black_ev, float white_ev,
                               float contrast, float latitude, float hardness) = 0;
    };

    // Bilat link; local contrast (local Laplacian filter).
    // Input/Output: float Lab in rgb() (in-place, modifies L channel only)
    class Bilat : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        // sigma=midtone point, shadows=shadow contrast, highlights=highlight contrast, clarity=detail
        virtual void setParams(float sigma, float shadows, float highlights, float clarity) = 0;
    };

    // Colorbalancergb link; color grading in Jzazbz/Yrg.
    // Input/Output: float RGB in rgb() (in-place)
    class Colorbalancergb : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        // Most params - for now just support chroma/saturation/vibrance
        virtual void setParams(float chroma_global, float saturation_global, float vibrance,
                               float contrast, float grey_fulcrum) = 0;
    };

    // Flip link; image orientation (rotation/mirror).
    // Input/Output: float RGB in rgb() (in-place, may change dimensions)
    class Flip : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        // orientation: -1=auto (EXIF), 0=none, 1=flip_y, 2=flip_x, 3=180°, 5=cw90°, 6=ccw90°
        virtual void setOrientation(int orientation) = 0;
    };

    // Colorequal link; per-hue color adjustments.
    // Input/Output: float RGB in rgb() (in-place)
    // Works in dt UCS 22 colorspace for perceptual uniformity.
    // 8 hue nodes: red, orange, yellow, green, cyan, blue, lavender, magenta
    class Colorequal : public Link
    {
    public:
        virtual void process(Flow &flow) = 0;
        // Set hue shift for each of 8 hues (degrees, -180 to +180)
        virtual void setHue(const float hue[8]) = 0;
        // Set saturation multiplier for each of 8 hues (0 to 2, 1=unchanged)
        virtual void setSaturation(const float sat[8]) = 0;
        // Set brightness multiplier for each of 8 hues (0 to 2, 1=unchanged)
        virtual void setBrightness(const float bright[8]) = 0;
        // Set all 24 parameters at once (for optimizer)
        virtual void setParams(const float hue[8], const float sat[8], const float bright[8]) = 0;
    };

    // ============================================================================
    // Factory & Utilities
    // ============================================================================

    // Create a Tree
    std::unique_ptr<Tree> makeTree();

    // Create a Head (Sony ARW decoder)
    std::unique_ptr<Head> makeHead();

    // Create a Rawprepare (black level subtraction)
    std::unique_ptr<Rawprepare> makeRawprepare();

    // Create a Demosaic (bayer to RGB)
    std::unique_ptr<Demosaic> makeDemosaic();

    // Create a Highlights (highlight reconstruction)
    std::unique_ptr<Highlights> makeHighlights();

    // Create a Temperature (white balance)
    std::unique_ptr<Temperature> makeTemperature();

    // Create an Exposure (scene-referred brightness)
    std::unique_ptr<Exposure> makeExposure();

    // Create a Channelmixer (CAT16 chromatic adaptation)
    std::unique_ptr<Channelmixer> makeChannelmixer();

    // Create a Channelmixerrgb (RGB channel mixing)
    std::unique_ptr<Channelmixerrgb> makeChannelmixerrgb();

    // Create a Colorin (camera RGB → Lab)
    std::unique_ptr<Colorin> makeColorin();

    // Create a Colorout (Lab → linear sRGB)
    std::unique_ptr<Colorout> makeColorout();

    // Create a Gamma (sRGB transfer function)
    std::unique_ptr<Gamma> makeGamma();

    // Create a Sigmoid (scene-referred tone mapping)
    std::unique_ptr<Sigmoid> makeSigmoid();

    // Create a Filmicrgb (filmic tone mapping)
    std::unique_ptr<Filmicrgb> makeFilmicrgb();

    // Create a Bilat (local Laplacian contrast)
    std::unique_ptr<Bilat> makeBilat();

    // Create a Colorbalancergb (color grading)
    std::unique_ptr<Colorbalancergb> makeColorbalancergb();

    // Create a Flip (orientation)
    std::unique_ptr<Flip> makeFlip();

    // Create a Colorequal (per-hue color adjustments)
    std::unique_ptr<Colorequal> makeColorequal();

    // Colorspace swaps (in-place on rgb() buffer)
    // CLEAN COPY from DT common/colorspaces_inline_conversions.h
    void swapLabToRGB(Flow& flow);  // Lab → XYZ → linear sRGB (D50)
    void swapRGBToLab(Flow& flow);  // linear sRGB → XYZ → Lab (D50)

    // Format types
    enum Swap
    {
        JPG, // JPEG compressed
        PNG, // PNG compressed
        BIN, // RGB uint8 (w*h*3 bytes)
        LIN  // RGB float linear (w*h*3 floats, scene-referred)
    };

    // Universal format conversion:
    //   from BIN/LIN: w=width, h=height
    //   from PNG/JPG: w=byte_size, h=ignored
    std::vector<uint8_t> swap(const void *data, int w, int h, Swap from, Swap into);

    // ============================================================================
    // Diff - Delta-E image comparison (like ImageMagick compare)
    // ============================================================================

    // Delta-E statistics from image comparison
    struct DiffResult
    {
        int width;
        int height;
        double mean_de;     // Mean delta-E across all pixels
        double max_de;      // Maximum delta-E
        double pct_above_1; // % of pixels with delta-E > 1 (just noticeable)
        double pct_above_2; // % of pixels with delta-E > 2 (noticeable at a glance)
        double correlation; // Pearson correlation
        std::vector<float> diff_map; // Per-pixel delta-E values (optional)
    };

    // Compare two sRGB images and compute delta-E metrics
    // img1, img2: RGB uint8 data (w*h*3 bytes each)
    // compute_map: if true, populate diff_map with per-pixel delta-E
    DiffResult diff(const uint8_t* img1, const uint8_t* img2, int width, int height, bool compute_map = false);

    // Mode for diff visualization
    enum class DiffMode
    {
        GRAYSCALE,  // Delta-E as grayscale (0=black, scale=white)
        HEATMAP,    // Delta-E as heat map (blue→green→yellow→red)
        HIGHLIGHT   // Gray base + red highlights for differences
    };

    // Generate visual diff image (RGB uint8, w*h*3 bytes)
    // scale: delta-E value that maps to max intensity (default 10)
    std::vector<uint8_t> diff_image(const uint8_t* img1, const uint8_t* img2,
                                     int width, int height, DiffMode mode = DiffMode::HIGHLIGHT, float scale = 10.0f);

    // Print diff statistics to stdout
    void print_diff_stats(const DiffResult& result);

    // Compare float32 buffers (for non-visual intermediate steps)
    // Works on RGBX (4 floats per pixel) or LabX format
    // Returns delta-E for Lab, or RMSE for RGB
    DiffResult diff_float(const float* buf1, const float* buf2, int width, int height,
                          Colorspace cs, bool compute_map = false);

}
