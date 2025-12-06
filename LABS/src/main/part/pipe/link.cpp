// link.cpp
// Module implementations and LinkImpl
// Contains dial state management for all 6 golden modules

#include "link.hpp"
#include "mods/mods.h"
#include <stdexcept>
#include <iostream>

namespace pipe::internal
{

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
// Base Curve Module (from RAW decoder)
// ============================================================

class BaseCurveImpl : public Body::Link::BaseCurve
{
    float m_curve[CURVE_SIZE];
    bool m_active = false;

public:
    BaseCurveImpl()
    {
        reset();
    }

    const float* curve() const override { return m_curve; }

    void setCurve(const float* values) override
    {
        for (int i = 0; i < CURVE_SIZE; i++)
            m_curve[i] = values[i];
        m_active = true;
    }

    void reset() override
    {
        mods::base_curve_identity(m_curve);
        m_active = false;
    }

    bool isActive() const override { return m_active; }

    bool apply(View& view)
    {
        if (!m_active)
            return true;  // No-op

        View output;
        if (!mods::base_curve(view, output, m_curve))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// Polynomial Color Module (Camera Math)
// ============================================================

class PolyColorImpl : public Body::Link::PolyColor
{
    float m_coeffs[COEFFS_SIZE];
    bool m_active = false;

public:
    PolyColorImpl()
    {
        reset();
    }

    const float* coeffs() const override { return m_coeffs; }

    void setCoeffs(const float* values) override
    {
        for (int i = 0; i < COEFFS_SIZE; i++)
            m_coeffs[i] = values[i];
        m_active = true;
    }

    void reset() override
    {
        mods::identity_poly_color(m_coeffs);
        m_active = false;
    }

    bool isActive() const override { return m_active; }

    bool apply(View& view)
    {
        if (!m_active)
            return true;  // No-op

        View output;
        if (!mods::poly_color(view, output, m_coeffs))
            return false;
        view = output;
        return true;
    }
};

// ============================================================
// 3D LUT Module (full RGB→RGB transform)
// ============================================================

class LutCurveImpl : public Body::Link::LutCurve
{
    static constexpr int GRID = GRID_SIZE;  // 17 (from pipe.hpp)
    static constexpr int TOTAL = GRID * GRID * GRID * 3;  // 14,739
    float m_lut[TOTAL];
    bool m_estimated = false;

public:
    LutCurveImpl()
    {
        reset();
    }

    const float* lut() const override { return m_lut; }

    void setLut(const float* values) override
    {
        for (int i = 0; i < TOTAL; i++)
            m_lut[i] = values[i];
        m_estimated = true;
    }

    bool estimate(View base, View target) override
    {
        if (!mods::lut3d_estimate(base, target, m_lut, GRID))
            return false;
        m_estimated = true;
        return true;
    }

    void reset() override
    {
        // Initialize to identity: output RGB = input RGB
        for (int ri = 0; ri < GRID; ri++)
        {
            for (int gi = 0; gi < GRID; gi++)
            {
                for (int bi = 0; bi < GRID; bi++)
                {
                    int idx = ((ri * GRID + gi) * GRID + bi) * 3;
                    m_lut[idx + 0] = static_cast<float>(ri) / (GRID - 1);  // R
                    m_lut[idx + 1] = static_cast<float>(gi) / (GRID - 1);  // G
                    m_lut[idx + 2] = static_cast<float>(bi) / (GRID - 1);  // B
                }
            }
        }
        m_estimated = false;
    }

    bool isEstimated() const override { return m_estimated; }

    bool apply(View& view)
    {
        if (!m_estimated) {
            std::cerr << "[LUT3D] Not estimated, skipping apply\n";
            return true;  // No-op if not estimated
        }

        std::cerr << "[LUT3D] Applying 3D LUT to " << view.cols << "x" << view.rows << " image\n";
        View output;
        if (!mods::lut3d_apply(view, output, m_lut, GRID)) {
            std::cerr << "[LUT3D] Apply FAILED\n";
            return false;
        }
        view = output;
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

class PivotImpl : public Body::Link::ToneMapping::CurveAdjustment::Pivot
{
    bool& m_active;
    float m_value = 0.5f;
public:
    PivotImpl(bool& active) : m_active(active) {}
    float get() override { return m_value; }
    void set(float v) override { m_value = v; m_active = true; }
    View run(View view) override { return view; }
};

class CurveAdjustmentImpl : public Body::Link::ToneMapping::CurveAdjustment
{
    RegionImpl m_highlights, m_shadows;
    PivotImpl m_toePivot, m_shoulderPivot;
public:
    CurveAdjustmentImpl(bool& active)
        : m_highlights(active), m_shadows(active)
        , m_toePivot(active), m_shoulderPivot(active) {}
    Region& highlights() override { return m_highlights; }
    Region& shadows() override { return m_shadows; }
    Pivot& toePivot() override { return m_toePivot; }
    Pivot& shoulderPivot() override { return m_shoulderPivot; }
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
                m_curve.toePivot().get(),
                m_curve.shoulderPivot().get(),
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
// Split Tone Module (shadow/highlight color grading)
// ============================================================

class TempTintImpl : public Body::Link::SplitTone::TempTint
{
    bool& m_active;
    float m_temp = 0.5f, m_tint = 0.5f;
public:
    TempTintImpl(bool& active) : m_active(active) {}
    float temperature() override { return m_temp; }
    void temperature(float v) override { m_temp = v; m_active = true; }
    float tint() override { return m_tint; }
    void tint(float v) override { m_tint = v; m_active = true; }
    View run(View view) override { return view; }
};

class SplitToneImpl : public Body::Link::SplitTone
{
    bool m_active = false;
    TempTintImpl m_shadows{m_active};
    TempTintImpl m_highlights{m_active};
public:
    TempTint& shadows() override { return m_shadows; }
    TempTint& highlights() override { return m_highlights; }

    bool isActive() const { return m_active; }

    bool apply(View& view)
    {
        View output;
        if (!mods::split_tone(view, output,
                m_shadows.temperature(), m_shadows.tint(),
                m_highlights.temperature(), m_highlights.tint()))
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
// LinkImpl
// ============================================================

LinkImpl::LinkImpl(Name name)
    : m_name(std::move(name))
    , m_geometric(std::make_unique<GeometricImpl>())
    , m_colorCorrection(std::make_unique<ColorCorrectionImpl>())
    , m_baseCurve(std::make_unique<BaseCurveImpl>())
    , m_polyColor(std::make_unique<PolyColorImpl>())
    , m_lutCurve(std::make_unique<LutCurveImpl>())
    , m_toneMapping(std::make_unique<ToneMappingImpl>())
    , m_globalColor(std::make_unique<GlobalColorImpl>())
    , m_splitTone(std::make_unique<SplitToneImpl>())
    , m_selectiveColour(std::make_unique<SelectiveColourImpl>())
    , m_detail(std::make_unique<DetailImpl>())
{
}

LinkImpl::~LinkImpl() = default;

Name LinkImpl::name() { return m_name; }
Body::Link::Geometric& LinkImpl::geometric() { return *m_geometric; }
Body::Link::ColorCorrection& LinkImpl::colorCorrection() { return *m_colorCorrection; }
Body::Link::BaseCurve& LinkImpl::baseCurve() { return *m_baseCurve; }
Body::Link::PolyColor& LinkImpl::polyColor() { return *m_polyColor; }
Body::Link::LutCurve& LinkImpl::lutCurve() { return *m_lutCurve; }
Body::Link::ToneMapping& LinkImpl::toneMapping() { return *m_toneMapping; }
Body::Link::GlobalColor& LinkImpl::globalColor() { return *m_globalColor; }
Body::Link::SplitTone& LinkImpl::splitTone() { return *m_splitTone; }
Body::Link::SelectiveColour& LinkImpl::selectiveColour() { return *m_selectiveColour; }
Body::Link::Detail& LinkImpl::detail() { return *m_detail; }

View LinkImpl::run(View view)
{
    if (m_geometric->isActive()) m_geometric->apply(view);
    if (m_colorCorrection->isActive()) m_colorCorrection->apply(view);
    if (m_baseCurve->isActive()) m_baseCurve->apply(view);
    if (m_polyColor->isActive()) m_polyColor->apply(view);
    if (m_lutCurve->isEstimated()) m_lutCurve->apply(view);
    if (m_toneMapping->isActive()) m_toneMapping->apply(view);
    if (m_globalColor->isActive()) m_globalColor->apply(view);
    if (m_splitTone->isActive()) m_splitTone->apply(view);
    if (m_selectiveColour->isActive()) m_selectiveColour->apply(view);
    if (m_detail->isActive()) m_detail->apply(view);
    return view;
}

// ============================================================
// IteratorImpl
// ============================================================

IteratorImpl::IteratorImpl(std::vector<std::unique_ptr<LinkImpl>>& links)
    : m_links(links)
{
}

Body::Link& IteratorImpl::current()
{
    if (m_index >= m_links.size())
        throw std::out_of_range("Iterator out of range");
    return *m_links[m_index];
}

bool IteratorImpl::next()
{
    if (m_index + 1 < m_links.size()) { ++m_index; return true; }
    return false;
}

void IteratorImpl::reset() { m_index = 0; }

} // namespace pipe::internal
