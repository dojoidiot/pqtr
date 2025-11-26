// pipe.cpp
// PIMPL implementation of pipe.hpp
// HEAD→BODY→TAIL builder pattern with 6 golden modules
//
// Architecture: Modules only run if activated by setting a dial value.
// - Each dial setter activates its parent module
// - LinkImpl::run() only processes active modules
// - No dials set = passthrough (zero processing)

#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <vector>
#include <stdexcept>

// RAW decoder (from RAWS library)
#include "raws.hpp"

// Internal mods (not exposed in public API)
#include "mods/mods.h"

namespace pipe
{

// ============================================================
// Gamma Encoding (linear → sRGB display)
// ============================================================

static bool applyGamma(const cv::UMat& linear, cv::UMat& gamma)
{
    cv::UMat clamped;
    cv::max(linear, 0.0f, clamped);

    cv::UMat lowMask, highMask;
    cv::compare(clamped, 0.0031308f, lowMask, cv::CMP_LE);
    cv::compare(clamped, 0.0031308f, highMask, cv::CMP_GT);

    cv::UMat lowPart, highPart;
    cv::multiply(clamped, 12.92f, lowPart);

    cv::UMat temp;
    cv::pow(clamped, 1.0f / 2.4f, temp);
    cv::multiply(temp, 1.055f, temp);
    cv::subtract(temp, 0.055f, highPart);

    gamma.create(linear.size(), linear.type());
    lowPart.copyTo(gamma, lowMask);
    highPart.copyTo(gamma, highMask);

    return true;
}

// Convert linear scene data to 8-bit BGR for display
static View toDisplayView(const View& linear, int max_dim = 0)
{
    View scaled = linear;

    // Scale if requested
    if (max_dim > 0)
    {
        float scale = (float)max_dim / std::max(linear.cols, linear.rows);
        if (scale < 1.0f)
        {
            View small;
            cv::resize(linear, small, cv::Size(), scale, scale, cv::INTER_AREA);
            scaled = small;
        }
    }

    cv::UMat gamma;
    applyGamma(scaled, gamma);

    cv::UMat out8;
    gamma.convertTo(out8, CV_8UC3, 255.0);

    return out8;
}

// ============================================================
// DataImpl
// ============================================================

class DataImpl : public Data
{
    Info m_info;
    View m_view;

public:
    DataImpl() = default;
    DataImpl(Info info, View view) : m_info(std::move(info)), m_view(std::move(view)) {}

    Info info() override { return m_info; }
    View view() override { return m_view; }

    void setView(View view) { m_view = std::move(view); }
};

// ============================================================
// Geometric Module
// ============================================================

class CropImpl : public Body::Link::Geometric::Crop
{
    bool& m_active;
    float m_top = 0.0f, m_right = 0.0f, m_bottom = 0.0f, m_left = 0.0f;
public:
    CropImpl(bool& active) : m_active(active) {}
    float crop_top() override { return m_top; }
    void crop_top(float v) override { m_top = v; m_active = true; }
    float crop_right() override { return m_right; }
    void crop_right(float v) override { m_right = v; m_active = true; }
    float crop_bottom() override { return m_bottom; }
    void crop_bottom(float v) override { m_bottom = v; m_active = true; }
    float crop_left() override { return m_left; }
    void crop_left(float v) override { m_left = v; m_active = true; }
    View run(View view) override { return view; }
};

class ZoomImpl : public Body::Link::Geometric::Zoom
{
    bool& m_active;
    float m_scale = 0.0f;
public:
    ZoomImpl(bool& active) : m_active(active) {}
    float scale() override { return m_scale; }
    void scale(float v) override { m_scale = v; m_active = true; }
    View run(View view) override { return view; }
};

class RotationImpl : public Body::Link::Geometric::Rotation
{
    bool& m_active;
    float m_tilt = 0.5f;
public:
    RotationImpl(bool& active) : m_active(active) {}
    float tiltAngle() override { return m_tilt; }
    void tiltAngle(float v) override { m_tilt = v; m_active = true; }
    View run(View view) override { return view; }
};

class GeometricImpl : public Body::Link::Geometric
{
    bool m_active = false;
    CropImpl m_crop{m_active};
    ZoomImpl m_zoom{m_active};
    RotationImpl m_rotation{m_active};
public:
    Crop& crop() override { return m_crop; }
    Zoom& zoom() override { return m_zoom; }
    Rotation& rotation() override { return m_rotation; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        View output;
        if (!mods::geometric(view, output,
                m_crop.crop_top(), m_crop.crop_right(),
                m_crop.crop_bottom(), m_crop.crop_left(),
                m_zoom.scale(), m_rotation.tiltAngle()))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// Color Correction Module
// ============================================================

class ExposureImpl : public Body::Link::ColorCorrection::Exposure
{
    bool& m_active;
    float m_value = 0.5f;
public:
    ExposureImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class WhiteBalanceImpl : public Body::Link::ColorCorrection::WhiteBalance
{
    bool& m_active;
    float m_temp = 0.5f, m_tint = 0.5f;
public:
    WhiteBalanceImpl(bool& active) : m_active(active) {}
    float temperature() override { return m_temp; }
    void temperature(float v) override { m_temp = v; m_active = true; }
    float tint() override { return m_tint; }
    void tint(float v) override { m_tint = v; m_active = true; }
    View run(View view) override { return view; }
};

class ColorCorrectionImpl : public Body::Link::ColorCorrection
{
    bool m_active = false;
    ExposureImpl m_exposure{m_active};
    WhiteBalanceImpl m_wb{m_active};
public:
    Exposure& exposure() override { return m_exposure; }
    WhiteBalance& whiteBalance() override { return m_wb; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        View tmp;
        if (!mods::exposure(view, tmp, m_exposure.get()))
            return false;
        if (!mods::white_balance(tmp, view, m_wb.temperature(), m_wb.tint()))
            return false;
        return true;
    }
};

// ============================================================
// Tone Mapping Module
// ============================================================

class ContrastImpl : public Body::Link::ToneMapping::Contrast
{
    bool& m_active;
    float m_value = 0.5f;
public:
    ContrastImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class RegionImpl : public Body::Link::ToneMapping::CurveAdjustment::Region
{
    bool& m_active;
    float m_value = 0.5f;
public:
    RegionImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class CurveAdjustmentImpl : public Body::Link::ToneMapping::CurveAdjustment
{
    RegionImpl m_highlights, m_shadows;
public:
    CurveAdjustmentImpl(bool& active) : m_highlights(active), m_shadows(active) {}
    Region& highlights() override { return m_highlights; }
    Region& shadows() override { return m_shadows; }
};

class ShadeImpl : public Body::Link::ToneMapping::ClippingPoint::Shade
{
    bool& m_active;
    float m_value = 0.5f;
public:
    ShadeImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class ClippingPointImpl : public Body::Link::ToneMapping::ClippingPoint
{
    ShadeImpl m_black, m_white;
public:
    ClippingPointImpl(bool& active) : m_black(active), m_white(active) {}
    Shade& black() override { return m_black; }
    Shade& white() override { return m_white; }
};

class ToneMappingImpl : public Body::Link::ToneMapping
{
    bool m_active = false;
    ContrastImpl m_contrast{m_active};
    CurveAdjustmentImpl m_curve{m_active};
    ClippingPointImpl m_clip{m_active};
public:
    Contrast& contrast() override { return m_contrast; }
    CurveAdjustment& curveAdjustment() override { return m_curve; }
    ClippingPoint& clippingPoint() override { return m_clip; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        View output;
        if (!mods::tone_map(view, output,
                m_contrast.get(),
                m_curve.highlights().get(),
                m_curve.shadows().get(),
                m_clip.white().get(),
                m_clip.black().get()))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// Global Color Module
// ============================================================

class VibranceImpl : public Body::Link::GlobalColor::Vibrance
{
    bool& m_active;
    float m_value = 0.5f;
public:
    VibranceImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class SaturationImpl : public Body::Link::GlobalColor::Saturation
{
    bool& m_active;
    float m_value = 0.5f;
public:
    SaturationImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class ColourDensityImpl : public Body::Link::GlobalColor::ColourDensity
{
    bool& m_active;
    float m_value = 0.5f;
public:
    ColourDensityImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class GlobalColorImpl : public Body::Link::GlobalColor
{
    bool m_active = false;
    VibranceImpl m_vibrance{m_active};
    SaturationImpl m_saturation{m_active};
    ColourDensityImpl m_density{m_active};
public:
    Vibrance& vibrance() override { return m_vibrance; }
    Saturation& saturation() override { return m_saturation; }
    ColourDensity& colourDensity() override { return m_density; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        View output;
        if (!mods::global_color(view, output,
                m_vibrance.get(), m_saturation.get(), m_density.get()))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// Selective Colour Module
// ============================================================

class HslAdjustImpl : public Body::Link::SelectiveColour::HslAdjust
{
    bool& m_active;
    float m_hue = 0.5f, m_sat = 0.5f, m_lum = 0.5f;
public:
    HslAdjustImpl(bool& active) : m_active(active) {}
    float hue() override { return m_hue; }
    void hue(float v) override { m_hue = v; m_active = true; }
    float saturation() override { return m_sat; }
    void saturation(float v) override { m_sat = v; m_active = true; }
    float luminance() override { return m_lum; }
    void luminance(float v) override { m_lum = v; m_active = true; }
    View run(View view) override { return view; }
};

class SelectiveColourImpl : public Body::Link::SelectiveColour
{
    bool m_active = false;
    HslAdjustImpl m_red{m_active}, m_orange{m_active}, m_yellow{m_active}, m_green{m_active};
    HslAdjustImpl m_cyan{m_active}, m_blue{m_active}, m_purple{m_active}, m_magenta{m_active};
public:
    HslAdjust& red() override { return m_red; }
    HslAdjust& orange() override { return m_orange; }
    HslAdjust& yellow() override { return m_yellow; }
    HslAdjust& green() override { return m_green; }
    HslAdjust& cyan() override { return m_cyan; }
    HslAdjust& blue() override { return m_blue; }
    HslAdjust& purple() override { return m_purple; }
    HslAdjust& magenta() override { return m_magenta; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        float hue[8] = { m_red.hue(), m_orange.hue(), m_yellow.hue(), m_green.hue(),
                         m_cyan.hue(), m_blue.hue(), m_purple.hue(), m_magenta.hue() };
        float sat[8] = { m_red.saturation(), m_orange.saturation(), m_yellow.saturation(), m_green.saturation(),
                         m_cyan.saturation(), m_blue.saturation(), m_purple.saturation(), m_magenta.saturation() };
        float lum[8] = { m_red.luminance(), m_orange.luminance(), m_yellow.luminance(), m_green.luminance(),
                         m_cyan.luminance(), m_blue.luminance(), m_purple.luminance(), m_magenta.luminance() };

        View output;
        if (!mods::selective_color(view, output, hue, sat, lum))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// Detail Module
// ============================================================

class SharpenImpl : public Body::Link::Detail::Sharpen
{
    bool& m_active;
    float m_amount = 0.0f, m_radius = 0.4f;
public:
    SharpenImpl(bool& active) : m_active(active) {}
    float amount() override { return m_amount; }
    void amount(float v) override { m_amount = v; m_active = true; }
    float radius() override { return m_radius; }
    void radius(float v) override { m_radius = v; m_active = true; }
    View run(View view) override { return view; }
};

class DenoiseChannelImpl : public Body::Link::Detail::Denoise::Channel
{
    bool& m_active;
    float m_value;
public:
    DenoiseChannelImpl(bool& active, float def) : m_active(active), m_value(def) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class DenoiseImpl : public Body::Link::Detail::Denoise
{
    DenoiseChannelImpl m_luma, m_chroma;
public:
    DenoiseImpl(bool& active) : m_luma(active, 0.0f), m_chroma(active, 0.0f) {}
    Channel& luminance() override { return m_luma; }
    Channel& chroma() override { return m_chroma; }
};

class DetailImpl : public Body::Link::Detail
{
    bool m_active = false;
    SharpenImpl m_sharpen{m_active};
    DenoiseImpl m_denoise{m_active};
public:
    Sharpen& sharpen() override { return m_sharpen; }
    Denoise& denoise() override { return m_denoise; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        View output;
        if (!mods::detail(view, output,
                m_sharpen.amount(), m_sharpen.radius(),
                m_denoise.luminance().get(), m_denoise.chroma().get()))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// LinkImpl - Only runs active modules
// ============================================================

class LinkImpl : public Body::Link
{
    Name m_name;
    GeometricImpl m_geometric;
    ColorCorrectionImpl m_colorCorrection;
    ToneMappingImpl m_toneMapping;
    GlobalColorImpl m_globalColor;
    SelectiveColourImpl m_selectiveColour;
    DetailImpl m_detail;

public:
    LinkImpl(Name name) : m_name(std::move(name)) {}

    Name name() override { return m_name; }
    Geometric& geometric() override { return m_geometric; }
    ColorCorrection& colorCorrection() override { return m_colorCorrection; }
    ToneMapping& toneMapping() override { return m_toneMapping; }
    GlobalColor& globalColor() override { return m_globalColor; }
    SelectiveColour& selectiveColour() override { return m_selectiveColour; }
    Detail& detail() override { return m_detail; }

    View run(View view) override
    {
        // Only run modules that have been activated by setting a dial
        if (m_geometric.isActive()) m_geometric.apply(view);
        if (m_colorCorrection.isActive()) m_colorCorrection.apply(view);
        if (m_toneMapping.isActive()) m_toneMapping.apply(view);
        if (m_globalColor.isActive()) m_globalColor.apply(view);
        if (m_selectiveColour.isActive()) m_selectiveColour.apply(view);
        if (m_detail.isActive()) m_detail.apply(view);
        return view;
    }
};

// ============================================================
// IteratorImpl
// ============================================================

class IteratorImpl : public Body::Iterator
{
    std::vector<std::unique_ptr<LinkImpl>>& m_links;
    size_t m_index = 0;
public:
    IteratorImpl(std::vector<std::unique_ptr<LinkImpl>>& links) : m_links(links) {}

    Body::Link& current() override
    {
        if (m_index >= m_links.size())
            throw std::out_of_range("Iterator out of range");
        return *m_links[m_index];
    }

    bool next() override
    {
        if (m_index + 1 < m_links.size()) { ++m_index; return true; }
        return false;
    }

    void reset() override { m_index = 0; }
};

// ============================================================
// TailImpl - Has access to full-res data and links for export
// ============================================================

class TailImpl : public Tail
{
    DataImpl& m_full_data;  // Full resolution from Head
    std::vector<std::unique_ptr<LinkImpl>>& m_links;

public:
    TailImpl(DataImpl& full_data, std::vector<std::unique_ptr<LinkImpl>>& links)
        : m_full_data(full_data), m_links(links) {}

    bool save(const std::string& path, int max_dim) override
    {
        // Run pipeline on full-res data at requested output size
        View linear = m_full_data.view();

        // Scale to output size BEFORE processing
        if (max_dim > 0)
        {
            float scale = (float)max_dim / std::max(linear.cols, linear.rows);
            if (scale < 1.0f)
            {
                View small;
                cv::resize(linear, small, cv::Size(), scale, scale, cv::INTER_AREA);
                linear = small;
            }
        }

        // Run all links
        for (auto& link : m_links)
            linear = link->run(linear);

        // Convert to display format and save
        View display = toDisplayView(linear);

        cv::Mat cpu;
        display.copyTo(cpu);

        return cv::imwrite(path, cpu);
    }

    View view(int max_dim) override
    {
        // Run pipeline on full-res data at requested output size
        View linear = m_full_data.view();

        // Scale to output size BEFORE processing
        if (max_dim > 0)
        {
            float scale = (float)max_dim / std::max(linear.cols, linear.rows);
            if (scale < 1.0f)
            {
                View small;
                cv::resize(linear, small, cv::Size(), scale, scale, cv::INTER_AREA);
                linear = small;
            }
        }

        // Run all links
        for (auto& link : m_links)
            linear = link->run(linear);

        return toDisplayView(linear);
    }
};

// ============================================================
// BodyImpl
// ============================================================

class BodyImpl : public Body
{
    DataImpl& m_working;    // Working size data for preview
    DataImpl& m_full;       // Full resolution data for export
    std::vector<std::unique_ptr<LinkImpl>> m_links;
    std::unique_ptr<IteratorImpl> m_iterator;
    std::unique_ptr<TailImpl> m_tail;

public:
    BodyImpl(DataImpl& working, DataImpl& full) : m_working(working), m_full(full) {}

    Link& add(Name name) override
    {
        m_links.push_back(std::make_unique<LinkImpl>(std::move(name)));
        return *m_links.back();
    }

    Link& get(Name name) override
    {
        for (auto& link : m_links)
            if (link->name() == name) return *link;
        throw std::runtime_error("Link not found: " + name);
    }

    Iterator& links() override
    {
        if (!m_iterator) m_iterator = std::make_unique<IteratorImpl>(m_links);
        m_iterator->reset();
        return *m_iterator;
    }

    Data& data() override { return m_working; }

    View view(int max_dim = 0) override
    {
        // Run all links on working size data
        View linear = m_working.view();
        for (auto& link : m_links)
            linear = link->run(linear);

        // Return display-ready 8-bit BGR, optionally scaled further
        return toDisplayView(linear, max_dim);
    }

    Tail& tail() override
    {
        // Tail gets full-res data and links for export at any size
        if (!m_tail) m_tail = std::make_unique<TailImpl>(m_full, m_links);
        return *m_tail;
    }
};

// ============================================================
// HeadImpl
// ============================================================

class HeadImpl : public Head
{
    DataImpl m_data;      // Scene-linear RGB + full metadata (full resolution)
    DataImpl m_view;      // Embedded preview + view-specific metadata
    DataImpl m_working;   // Scaled working copy for preview processing
    std::unique_ptr<BodyImpl> m_body;
    int m_current_working_size = 0;
public:
    HeadImpl(Info dataInfo, View dataView, Info viewInfo, View viewImage)
        : m_data(std::move(dataInfo), std::move(dataView))
        , m_view(std::move(viewInfo), std::move(viewImage)) {}

    Data& data() override { return m_data; }
    Data& view() override { return m_view; }

    Body& body(int working_size = 0) override
    {
        // If working_size changed or body doesn't exist, create/recreate
        if (!m_body || working_size != m_current_working_size)
        {
            m_current_working_size = working_size;

            // Get full resolution data
            View full = m_data.view();

            if (working_size > 0)
            {
                // Scale down for faster preview processing
                float scale = (float)working_size / std::max(full.cols, full.rows);
                if (scale < 1.0f)
                {
                    View small;
                    cv::resize(full, small, cv::Size(), scale, scale, cv::INTER_AREA);
                    m_working = DataImpl(m_data.info(), std::move(small));
                }
                else
                {
                    m_working = DataImpl(m_data.info(), full);
                }
            }
            else
            {
                m_working = DataImpl(m_data.info(), full);
            }

            // Body gets working data for preview, full data for export via tail
            m_body = std::make_unique<BodyImpl>(m_working, m_data);
        }
        return *m_body;
    }
};

// ============================================================
// PipeImpl
// ============================================================

class PipeImpl : public Pipe
{
public:
    pqtr::Hold<Head> open(pqtr::Hold<pqtr::Sink> sink) override
    {
        // Decode RAW file using RAWS library
        raws::Result raw = raws::decode(*sink);
        if (!raw.success)
            return pqtr::Hold<Head>(nullptr);

        return pqtr::Hold<Head>(new HeadImpl(
            std::move(raw.dataInfo), std::move(raw.data),
            std::move(raw.previewInfo), std::move(raw.preview)));
    }
};

// ============================================================
// Factory
// ============================================================

pqtr::Hold<Pipe> make()
{
    return pqtr::Hold<Pipe>(new PipeImpl());
}

} // namespace pipe
