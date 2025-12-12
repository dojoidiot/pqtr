// lute.cpp - LUTE implementation
// Camera profile manager with BaseCurve, PolyColor, LutCurve, HsvLut transforms

#include <lute.hpp>
#include "mods/mods.h"
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

namespace lute
{

// ============================================================================
// BaseCurveImpl
// ============================================================================

class BaseCurveImpl : public Lute::BaseCurve
{
public:
    BaseCurveImpl() { reset(); }

    const float* curve() const override { return curve_.data(); }

    void setCurve(const float* values) override
    {
        std::memcpy(curve_.data(), values, CURVE_SIZE * sizeof(float));
        active_ = true;
    }

    void reset() override
    {
        mods::base_curve_identity(curve_.data());
        active_ = false;
    }

    bool isActive() const override { return active_; }

private:
    std::array<float, CURVE_SIZE> curve_;
    bool active_ = false;
};

// ============================================================================
// PolyColorImpl
// ============================================================================

class PolyColorImpl : public Lute::PolyColor
{
public:
    PolyColorImpl() { reset(); }

    const float* coeffs() const override { return coeffs_.data(); }

    void setCoeffs(const float* values) override
    {
        std::memcpy(coeffs_.data(), values, COEFFS_SIZE * sizeof(float));
        active_ = true;
    }

    void reset() override
    {
        mods::identity_poly_color(coeffs_.data());
        active_ = false;
    }

    bool isActive() const override { return active_; }

    bool estimate(View base, View target) override
    {
        if (mods::estimate_poly_color(base, target, coeffs_.data()))
        {
            active_ = true;
            return true;
        }
        return false;
    }

private:
    std::array<float, COEFFS_SIZE> coeffs_;
    bool active_ = false;
};

// ============================================================================
// LutCurveImpl
// ============================================================================

class LutCurveImpl : public Lute::LutCurve
{
public:
    LutCurveImpl() { reset(); }

    const float* lut() const override { return lut_.data(); }

    void setLut(const float* values) override
    {
        std::memcpy(lut_.data(), values, lut_.size() * sizeof(float));
        estimated_ = true;
    }

    void reset() override
    {
        for (int c = 0; c < 3; c++)
            for (int i = 0; i < GRID_SIZE; i++)
                lut_[c * GRID_SIZE + i] = float(i) / (GRID_SIZE - 1);
        estimated_ = false;
    }

    bool isEstimated() const override { return estimated_; }

    bool estimate(View base, View target) override
    {
        if (mods::estimate_lut(base, target, lut_.data(), GRID_SIZE))
        {
            estimated_ = true;
            return true;
        }
        return false;
    }

private:
    std::array<float, GRID_SIZE * 3> lut_;
    bool estimated_ = false;
};

// ============================================================================
// HsvLutImpl
// ============================================================================

class HsvLutImpl : public Lute::HsvLut
{
public:
    HsvLutImpl() { reset(); }

    const float* lut() const override { return lut_.data(); }

    void setLut(const float* values) override
    {
        std::memcpy(lut_.data(), values, LUT_SIZE * sizeof(float));
        estimated_ = true;
    }

    void reset() override
    {
        mods::hsv_lut_identity(lut_.data());
        estimated_ = false;
    }

    bool isEstimated() const override { return estimated_; }

    bool estimate(View base, View target) override
    {
        if (mods::hsv_lut_estimate(base, target, lut_.data()))
        {
            estimated_ = true;
            return true;
        }
        return false;
    }

private:
    std::array<float, LUT_SIZE> lut_;
    bool estimated_ = false;
};

// ============================================================================
// ProfileImpl
// ============================================================================

class ProfileImpl : public Lute::Profile
{
public:
    ProfileImpl(const Name& model, const Name& style, const Name& dro)
        : camera_model_(model), creative_style_(style), dro_(dro)
    {
        key_ = model + "_" + style;
        if (!dro.empty()) key_ += "_" + dro;
    }

    Name key() const override { return key_; }
    Name cameraModel() const override { return camera_model_; }
    Name creativeStyle() const override { return creative_style_; }
    Name dro() const override { return dro_; }

    Lute::BaseCurve& baseCurve() override { return base_curve_; }
    Lute::PolyColor& polyColor() override { return poly_color_; }
    Lute::LutCurve& lutCurve() override { return lut_curve_; }
    Lute::HsvLut& hsvLut() override { return hsv_lut_; }

    float coverage() const override { return float(sample_count_) / 1000.0f; }
    bool converged(float threshold) const override { return coverage() > threshold; }
    int sampleCount() const override { return sample_count_; }

    void addSample() { sample_count_++; }

    bool save(const Name& path) const override
    {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;

        // Header
        uint32_t magic = 0x4C555445; // "LUTE"
        uint32_t version = 1;
        out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));

        // Strings (length-prefixed)
        auto writeString = [&out](const Name& s) {
            uint32_t len = s.size();
            out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            out.write(s.data(), len);
        };
        writeString(camera_model_);
        writeString(creative_style_);
        writeString(dro_);

        // Sample count
        out.write(reinterpret_cast<const char*>(&sample_count_), sizeof(sample_count_));

        // Transform data
        out.write(reinterpret_cast<const char*>(base_curve_.curve()),
                  Lute::BaseCurve::CURVE_SIZE * sizeof(float));
        out.write(reinterpret_cast<const char*>(poly_color_.coeffs()),
                  Lute::PolyColor::COEFFS_SIZE * sizeof(float));
        out.write(reinterpret_cast<const char*>(lut_curve_.lut()),
                  Lute::LutCurve::GRID_SIZE * 3 * sizeof(float));
        out.write(reinterpret_cast<const char*>(hsv_lut_.lut()),
                  Lute::HsvLut::LUT_SIZE * sizeof(float));

        return out.good();
    }

    bool load(const Name& path) override
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;

        // Header
        uint32_t magic, version;
        in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        in.read(reinterpret_cast<char*>(&version), sizeof(version));

        if (magic != 0x4C555445 || version != 1) return false;

        // Strings
        auto readString = [&in]() -> Name {
            uint32_t len;
            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            Name s(len, '\0');
            in.read(s.data(), len);
            return s;
        };
        camera_model_ = readString();
        creative_style_ = readString();
        dro_ = readString();
        key_ = camera_model_ + "_" + creative_style_;
        if (!dro_.empty()) key_ += "_" + dro_;

        // Sample count
        in.read(reinterpret_cast<char*>(&sample_count_), sizeof(sample_count_));

        // Transform data
        std::array<float, Lute::BaseCurve::CURVE_SIZE> curve_data;
        in.read(reinterpret_cast<char*>(curve_data.data()),
                Lute::BaseCurve::CURVE_SIZE * sizeof(float));
        base_curve_.setCurve(curve_data.data());

        std::array<float, Lute::PolyColor::COEFFS_SIZE> poly_data;
        in.read(reinterpret_cast<char*>(poly_data.data()),
                Lute::PolyColor::COEFFS_SIZE * sizeof(float));
        poly_color_.setCoeffs(poly_data.data());

        std::array<float, Lute::LutCurve::GRID_SIZE * 3> lut_curve_data;
        in.read(reinterpret_cast<char*>(lut_curve_data.data()),
                Lute::LutCurve::GRID_SIZE * 3 * sizeof(float));
        lut_curve_.setLut(lut_curve_data.data());

        std::array<float, Lute::HsvLut::LUT_SIZE> hsv_data;
        in.read(reinterpret_cast<char*>(hsv_data.data()),
                Lute::HsvLut::LUT_SIZE * sizeof(float));
        hsv_lut_.setLut(hsv_data.data());

        return in.good();
    }

    void reset() override
    {
        base_curve_.reset();
        poly_color_.reset();
        lut_curve_.reset();
        hsv_lut_.reset();
        sample_count_ = 0;
    }

private:
    Name key_;
    Name camera_model_;
    Name creative_style_;
    Name dro_;
    int sample_count_ = 0;

    BaseCurveImpl base_curve_;
    PolyColorImpl poly_color_;
    LutCurveImpl lut_curve_;
    HsvLutImpl hsv_lut_;
};

// ============================================================================
// LuteImpl
// ============================================================================

class LuteImpl : public Lute
{
public:
    LuteImpl()
    {
        // Default profile dir: ~/.pqtr/var/profiles/
        const char* home = std::getenv("HOME");
        if (home)
            profile_dir_ = std::string(home) + "/.pqtr/var/profiles/";
        else
            profile_dir_ = "/tmp/pqtr/profiles/";
    }

    LuteImpl(const Name& model, const Name& style, const Name& dro)
        : LuteImpl()
    {
        setKey(model, style, dro);
    }

    Profile* profile() override { return profile_.get(); }
    const Profile* profile() const override { return profile_.get(); }

    void setKey(const Name& model, const Name& style, const Name& dro) override
    {
        profile_ = std::make_unique<ProfileImpl>(model, style, dro);

        // Try to load from disk
        Name path = profilePath();
        if (fs::exists(path))
        {
            if (!profile_->load(path))
                std::cerr << "[LUTE] Failed to load profile: " << path << "\n";
        }
    }

    View view(View in) override
    {
        if (!profile_) return in;

        View out;

        // Apply base curve
        if (profile_->baseCurve().isActive())
        {
            mods::base_curve(in, out, profile_->baseCurve().curve());
            in = out;
        }

        // Apply poly color
        if (profile_->polyColor().isActive())
        {
            mods::poly_color(in, out, profile_->polyColor().coeffs());
            in = out;
        }

        // Apply LUT curve
        if (profile_->lutCurve().isEstimated())
        {
            mods::lut_curve(in, out, profile_->lutCurve().lut(), Lute::LutCurve::GRID_SIZE);
            in = out;
        }

        // Apply HSV LUT
        if (profile_->hsvLut().isEstimated())
        {
            mods::hsv_lut_apply(in, out, profile_->hsvLut().lut());
            in = out;
        }

        return in;
    }

    bool tune(View flat, View preview) override
    {
        if (!profile_) return false;

        // Estimate transforms from flat (RAW) vs preview (embedded JPEG)
        auto* impl = dynamic_cast<ProfileImpl*>(profile_.get());
        if (!impl) return false;

        bool ok = true;

        // Estimate poly color
        if (!profile_->polyColor().estimate(flat, preview))
            ok = false;

        // Estimate LUT curve
        if (!profile_->lutCurve().estimate(flat, preview))
            ok = false;

        // Estimate HSV LUT
        if (!profile_->hsvLut().estimate(flat, preview))
            ok = false;

        if (ok) impl->addSample();

        return ok;
    }

    Name profileDir() const override { return profile_dir_; }

    bool save() override
    {
        if (!profile_) return false;

        Name path = profilePath();
        fs::create_directories(fs::path(path).parent_path());

        return profile_->save(path);
    }

private:
    Name profilePath() const
    {
        if (!profile_) return "";
        return profile_dir_ + profile_->key() + ".lute";
    }

    std::unique_ptr<ProfileImpl> profile_;
    Name profile_dir_;
};

// ============================================================================
// Factory functions
// ============================================================================

Hold create()
{
    return std::make_unique<LuteImpl>();
}

Hold create(const Name& cameraModel, const Name& creativeStyle, const Name& dro)
{
    return std::make_unique<LuteImpl>(cameraModel, creativeStyle, dro);
}

} // namespace lute
