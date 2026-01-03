#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * pqtr - photo quality transfer
 *
 * Labs = pipeline integration (C++ processing)
 * Vibe = parameter model (external interface)
 *
 * PIMPL - minimal interface, low cognitive load.
 */
namespace pqtr
{

// ============================================================================
// Labs - Pipeline Integration
// ============================================================================

namespace Labs
{
    // common fields
    constexpr const char *WIDTH = "width";
    constexpr const char *HEIGHT = "height";

    // Head Field Keys
    constexpr const char *NAME = "name";
    constexpr const char *BLACK = "black";
    constexpr const char *WHITE = "white";

    // Tail Field Keys
    constexpr const char *PNG = "png";
    constexpr const char *JPG = "jpg";

    // Bayer filter patterns
    constexpr uint32_t FILTERS_GBRG = 0x49494949;
    constexpr uint32_t FILTERS_RGGB = 0x94949494;

    // ------------------------------------------------------------------------
    // Tree - hierarchical metadata (PIMPL)
    // ------------------------------------------------------------------------

    class Node
    {
    public:
        virtual ~Node() = default;
    };

    class Leaf : public Node
    {
    public:
        virtual float dial() = 0;
        virtual void dial(float val) = 0;
        virtual std::string &text() = 0;
        virtual void text(std::string &val) = 0;
        virtual bool live() = 0;
    };

    class Stem : public Node
    {
    public:
        virtual Stem &next(const std::string &name) = 0;
        virtual Leaf &leaf(const std::string &name) = 0;
        virtual Node *find(const std::string &name) = 0;
        virtual size_t size() const = 0;
        virtual bool test(const std::string &name) const = 0;
        virtual void tidy() = 0;
        virtual std::vector<std::string> list() = 0;
    };

    // ------------------------------------------------------------------------
    // Flow - data container with execution state
    // ------------------------------------------------------------------------

    class Flow
    {
    public:
        virtual ~Flow() = default;

        // Image geometry
        virtual int width() = 0;
        virtual void width(int v) = 0;
        virtual int height() = 0;
        virtual void height(int v) = 0;
        virtual uint32_t filters() = 0;
        virtual void filters(uint32_t v) = 0;

        // Camera data
        virtual float exposureBias() = 0;
        virtual void exposureBias(float v) = 0;

        // Temperature output
        class Temperature
        {
        public:
            virtual ~Temperature() = default;
            virtual bool enabled() = 0;
            virtual void enabled(bool v) = 0;
            virtual float coeff(int ch) = 0;
            virtual void coeff(int ch, float v) = 0;
        };
        virtual Temperature &temperature() = 0;

        // Chroma adaptation data
        class Chroma
        {
        public:
            virtual ~Chroma() = default;
            virtual double D65(int ch) = 0;
            virtual void D65(int ch, double v) = 0;
            virtual double asShot(int ch) = 0;
            virtual void asShot(int ch, double v) = 0;
            virtual bool lateCorrection() = 0;
            virtual void lateCorrection(bool v) = 0;
        };
        virtual Chroma &chroma() = 0;

        // Color matrices (4x3 and 3x3)
        virtual float adobeXYZtoCAM(int row, int col) = 0;
        virtual void adobeXYZtoCAM(int row, int col, float v) = 0;
        virtual float d65ColorMatrix(int idx) = 0;
        virtual void d65ColorMatrix(int idx, float v) = 0;

        // Tree persistence
        virtual Stem &head() = 0;
        virtual Stem &flow() = 0;
        virtual Stem &tail() = 0;

        // Pixel data buffer
        virtual void *data() = 0;
        virtual void resize(size_t bytes) = 0;

        // Serialization
        virtual std::string json() = 0;
        virtual void read(std::string json) = 0;
    };

    // ------------------------------------------------------------------------
    // Links - processing units
    // ------------------------------------------------------------------------

    class Link {};

    class Head : public Link
    {
    public:
        virtual std::unique_ptr<Flow> load(Flow &flow, const void *bytes, size_t size) = 0;
    };

    class Tail : public Link
    {
    public:
        virtual void *save(Flow &flow) = 0;
    };

    class Step : public Link
    {
    public:
        virtual void *exec(Flow &flow) = 0;
    };

    // ------------------------------------------------------------------------
    // Pipe - fluent builder
    // ------------------------------------------------------------------------

    class Pipe
    {
    public:
        virtual ~Pipe() = default;
        virtual Pipe& head(std::unique_ptr<Head> head) = 0;
        virtual Pipe& body(std::string name, std::unique_ptr<Step> step) = 0;
        virtual Pipe& tail(std::unique_ptr<Tail> tail) = 0;
        virtual void *pump(void *data, size_t size) = 0;
    };

} // namespace Labs

// ============================================================================
// Vibe - Parameter Model
// ============================================================================

namespace Vibe
{
    // ------------------------------------------------------------------------
    // Common category types
    // ------------------------------------------------------------------------

    class Quad
    {
    public:
        virtual ~Quad() = default;
        virtual float r() = 0;
        virtual void r(float v) = 0;
        virtual float g() = 0;
        virtual void g(float v) = 0;
        virtual float b() = 0;
        virtual void b(float v) = 0;
        virtual float l() = 0;
        virtual void l(float v) = 0;
    };

    class Tonal
    {
    public:
        virtual ~Tonal() = default;
        virtual float shadows() = 0;
        virtual void shadows(float v) = 0;
        virtual float midtones() = 0;
        virtual void midtones(float v) = 0;
        virtual float highlights() = 0;
        virtual void highlights(float v) = 0;
    };

    // ------------------------------------------------------------------------
    // Module parameter types
    // ------------------------------------------------------------------------

    class Highlights
    {
    public:
        virtual ~Highlights() = default;

        virtual int mode() = 0;
        virtual void mode(int v) = 0;
        virtual float clip() = 0;
        virtual void clip(float v) = 0;
        virtual float strength() = 0;
        virtual void strength(float v) = 0;
        virtual float candidating() = 0;
        virtual void candidating(float v) = 0;
        virtual float combine() = 0;
        virtual void combine(float v) = 0;

        class Blend
        {
        public:
            virtual ~Blend() = default;
            virtual float L() = 0;
            virtual void L(float v) = 0;
            virtual float C() = 0;
            virtual void C(float v) = 0;
        };
        virtual Blend &blend() = 0;

        class Inpaint
        {
        public:
            virtual ~Inpaint() = default;
            virtual float noiseLevel() = 0;
            virtual void noiseLevel(float v) = 0;
            virtual float solidColor() = 0;
            virtual void solidColor(float v) = 0;
            virtual int iterations() = 0;
            virtual void iterations(int v) = 0;
            virtual int scales() = 0;
            virtual void scales(int v) = 0;
        };
        virtual Inpaint &inpaint() = 0;
    };

    class Demosaic
    {
    public:
        virtual ~Demosaic() = default;

        virtual int method() = 0;
        virtual void method(int v) = 0;

        class Opts
        {
        public:
            virtual ~Opts() = default;
            virtual int greenEq() = 0;
            virtual void greenEq(int v) = 0;
            virtual int colorSmoothing() = 0;
            virtual void colorSmoothing(int v) = 0;
            virtual int lmmseRefine() = 0;
            virtual void lmmseRefine(int v) = 0;
        };
        virtual Opts &opts() = 0;

        class Thrs
        {
        public:
            virtual ~Thrs() = default;
            virtual float median() = 0;
            virtual void median(float v) = 0;
            virtual float dual() = 0;
            virtual void dual(float v) = 0;
        };
        virtual Thrs &thrs() = 0;

        class Chroma
        {
        public:
            virtual ~Chroma() = default;
            virtual int enabled() = 0;
            virtual void enabled(int v) = 0;
            virtual int iter() = 0;
            virtual void iter(int v) = 0;
            virtual float radius() = 0;
            virtual void radius(float v) = 0;
            virtual float thrs() = 0;
            virtual void thrs(float v) = 0;
            virtual float boost() = 0;
            virtual void boost(float v) = 0;
            virtual float center() = 0;
            virtual void center(float v) = 0;
        };
        virtual Chroma &chroma() = 0;
    };

    class Exposure
    {
    public:
        virtual ~Exposure() = default;

        virtual int mode() = 0;
        virtual void mode(int v) = 0;
        virtual int compensateBias() = 0;
        virtual void compensateBias(int v) = 0;
        virtual float black() = 0;
        virtual void black(float v) = 0;
        virtual float ev() = 0;
        virtual void ev(float v) = 0;
    };

    class ChannelMixer
    {
    public:
        virtual ~ChannelMixer() = default;

        virtual int adaptation() = 0;
        virtual void adaptation(int v) = 0;
        virtual float p() = 0;
        virtual void p(float v) = 0;
        virtual float gamut() = 0;
        virtual void gamut(float v) = 0;
        virtual int clip() = 0;
        virtual void clip(int v) = 0;
        virtual int applyGrey() = 0;
        virtual void applyGrey(int v) = 0;

        virtual Quad &illuminant() = 0;
        virtual Quad &saturation() = 0;
        virtual Quad &lightness() = 0;
        virtual Quad &grey() = 0;

        class Mix
        {
        public:
            virtual ~Mix() = default;
            virtual Quad &red() = 0;
            virtual Quad &green() = 0;
            virtual Quad &blue() = 0;
        };
        virtual Mix &mix() = 0;
    };

    class ColorBalance
    {
    public:
        virtual ~ColorBalance() = default;

        virtual float contrast() = 0;
        virtual void contrast(float v) = 0;
        virtual float hueAngle() = 0;
        virtual void hueAngle(float v) = 0;

        class Global
        {
        public:
            virtual ~Global() = default;
            virtual float chroma() = 0;
            virtual void chroma(float v) = 0;
            virtual float vibrance() = 0;
            virtual void vibrance(float v) = 0;
            virtual float saturation() = 0;
            virtual void saturation(float v) = 0;
            virtual float brilliance() = 0;
            virtual void brilliance(float v) = 0;
        };
        virtual Global &global() = 0;

        class Weight
        {
        public:
            virtual ~Weight() = default;
            virtual float shadows() = 0;
            virtual void shadows(float v) = 0;
            virtual float midtones() = 0;
            virtual void midtones(float v) = 0;
            virtual float highlights() = 0;
            virtual void highlights(float v) = 0;
        };
        virtual Weight &weight() = 0;

        class Fulcrum
        {
        public:
            virtual ~Fulcrum() = default;
            virtual float grey() = 0;
            virtual void grey(float v) = 0;
            virtual float white() = 0;
            virtual void white(float v) = 0;
            virtual float maskGrey() = 0;
            virtual void maskGrey(float v) = 0;
        };
        virtual Fulcrum &fulcrum() = 0;

        virtual Quad &shadows() = 0;
        virtual Quad &midtones() = 0;
        virtual Quad &highlights() = 0;
        virtual float midtonesY() = 0;
        virtual void midtonesY(float v) = 0;

        virtual Quad &chroma() = 0;
        virtual Quad &saturation() = 0;
        virtual Quad &brilliance() = 0;
    };

    class Filmic
    {
    public:
        virtual ~Filmic() = default;

        class Source
        {
        public:
            virtual ~Source() = default;
            virtual float grey() = 0;
            virtual void grey(float v) = 0;
            virtual float black() = 0;
            virtual void black(float v) = 0;
            virtual float white() = 0;
            virtual void white(float v) = 0;
            virtual float dynamicRange() = 0;
            virtual void dynamicRange(float v) = 0;
        };
        virtual Source &source() = 0;

        virtual float contrast() = 0;
        virtual void contrast(float v) = 0;
        virtual float saturation() = 0;
        virtual void saturation(float v) = 0;

        class Display
        {
        public:
            virtual ~Display() = default;
            virtual float normalize() = 0;
            virtual void normalize(float v) = 0;
            virtual float outputPower() = 0;
            virtual void outputPower(float v) = 0;
        };
        virtual Display &display() = 0;

        class Sigma
        {
        public:
            virtual ~Sigma() = default;
            virtual float toe() = 0;
            virtual void toe(float v) = 0;
            virtual float shoulder() = 0;
            virtual void shoulder(float v) = 0;
        };
        virtual Sigma &sigma() = 0;
    };

    class Bilat
    {
    public:
        virtual ~Bilat() = default;

        virtual int mode() = 0;
        virtual void mode(int v) = 0;
        virtual float detail() = 0;
        virtual void detail(float v) = 0;
        virtual float midtone() = 0;
        virtual void midtone(float v) = 0;

        class Sigma
        {
        public:
            virtual ~Sigma() = default;
            virtual float r() = 0;
            virtual void r(float v) = 0;
            virtual float s() = 0;
            virtual void s(float v) = 0;
        };
        virtual Sigma &sigma() = 0;
    };

    // ------------------------------------------------------------------------
    // Vibe - complete parameter set
    // ------------------------------------------------------------------------

    class Vibe
    {
    public:
        virtual ~Vibe() = default;

        virtual Highlights &highlights() = 0;
        virtual Demosaic &demosaic() = 0;
        virtual Exposure &exposure() = 0;
        virtual ChannelMixer &channelmixer() = 0;
        virtual ColorBalance &colorbalance() = 0;
        virtual Filmic &filmic() = 0;
        virtual Bilat &bilat() = 0;

        virtual std::string json() const = 0;
        virtual void load(const std::string &json) = 0;
        virtual void reset() = 0;
    };

} // namespace Vibe

// ============================================================================
// Factories
// ============================================================================

std::unique_ptr<Labs::Pipe> pipe();
std::unique_ptr<Vibe::Vibe> vibe(Labs::Pipe &pipe);

} // namespace pqtr
