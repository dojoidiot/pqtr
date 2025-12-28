// colorbalancergb.cpp - Color grading with saturation/vibrance
//
// CLEAN COPY from darktable src/iop/colorbalancergb.c
// Implements chroma/saturation/vibrance adjustments in scene-linear RGB.
//
// IOP order: 41.5 (after channelmixerrgb, before sigmoid)
// Works in: RGB (scene-linear)

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>

namespace flow
{

// =============================================================================
// Constants - CLEAN COPY from DT
// =============================================================================

// sRGB luminance coefficients (Rec.709)
static constexpr float LUM_R = 0.2126f;
static constexpr float LUM_G = 0.7152f;
static constexpr float LUM_B = 0.0722f;

// =============================================================================
// Implementation
// =============================================================================

class ColorbalancergbImpl : public Colorbalancergb
{
    float chroma_global_ = 0.0f;
    float saturation_global_ = 0.0f;
    float vibrance_ = 0.0f;
    float contrast_ = 0.0f;
    float grey_fulcrum_ = 0.1845f;  // 18.45% grey

public:
    std::string name() const override { return "colorbalancergb"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setParams(float chroma_global, float saturation_global, float vibrance,
                   float contrast, float grey_fulcrum) override
    {
        chroma_global_ = chroma_global;
        saturation_global_ = saturation_global;
        vibrance_ = vibrance;
        contrast_ = contrast;
        grey_fulcrum_ = grey_fulcrum;
    }

    bool isIdentity() const
    {
        const float eps = 1e-6f;
        return std::abs(chroma_global_) < eps &&
               std::abs(saturation_global_) < eps &&
               std::abs(vibrance_) < eps &&
               std::abs(contrast_) < eps;
    }

    void process(Flow& flow) override
    {
        // Fast path: no adjustments
        if (isIdentity()) return;

        auto& root = flow.info().root();
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        float* rgb = flow.rgb();

        // Process each pixel
        for (size_t k = 0; k < npixels; k++)
        {
            size_t idx = k * 4;

            float R = rgb[idx + 0];
            float G = rgb[idx + 1];
            float B = rgb[idx + 2];

            // Clip negative values (scene-linear can have negatives from processing)
            R = std::max(0.0f, R);
            G = std::max(0.0f, G);
            B = std::max(0.0f, B);

            // Calculate luminance (Y)
            float Y = LUM_R * R + LUM_G * G + LUM_B * B;
            if (Y < 1e-8f) Y = 1e-8f;  // Avoid division by zero

            // Calculate current saturation as distance from grey axis
            // sat = sqrt((R-Y)² + (G-Y)² + (B-Y)²) / Y
            float dR = R - Y;
            float dG = G - Y;
            float dB = B - Y;
            float sat = std::sqrt(dR*dR + dG*dG + dB*dB) / Y;
            if (sat < 1e-8f) sat = 1e-8f;

            // =================================================================
            // Chroma boost (linear scaling)
            // From DT: chroma_factor = 1 + chroma_boost
            // =================================================================
            float chroma_factor = 1.0f + chroma_global_;

            // =================================================================
            // Vibrance (adaptive saturation - boosts low saturation more)
            // From DT: vibrance = d->vibrance * (1.0f - powf(Ych[1], fabsf(d->vibrance)))
            // For low saturation pixels, the boost is higher
            // =================================================================
            if (std::abs(vibrance_) > 1e-6f)
            {
                // Normalize saturation to 0-1 range (roughly)
                float norm_sat = std::min(1.0f, sat);
                float vib_boost = vibrance_ * (1.0f - std::pow(norm_sat, std::abs(vibrance_)));
                chroma_factor += vib_boost;
            }

            // =================================================================
            // Saturation boost (multiplicative)
            // From DT: applied as a multiplier on chroma
            // =================================================================
            chroma_factor *= (1.0f + saturation_global_);

            // Clamp to non-negative
            chroma_factor = std::max(0.0f, chroma_factor);

            // =================================================================
            // Apply saturation adjustment
            // out = Y + (in - Y) * factor
            // =================================================================
            float out_R = Y + dR * chroma_factor;
            float out_G = Y + dG * chroma_factor;
            float out_B = Y + dB * chroma_factor;

            // =================================================================
            // Contrast adjustment (around grey fulcrum)
            // From DT: power function around pivot point
            // =================================================================
            if (std::abs(contrast_) > 1e-6f)
            {
                const float contrast_power = 1.0f + contrast_;
                if (out_R > 0.0f) out_R = grey_fulcrum_ * std::pow(out_R / grey_fulcrum_, contrast_power);
                if (out_G > 0.0f) out_G = grey_fulcrum_ * std::pow(out_G / grey_fulcrum_, contrast_power);
                if (out_B > 0.0f) out_B = grey_fulcrum_ * std::pow(out_B / grey_fulcrum_, contrast_power);
            }

            rgb[idx + 0] = out_R;
            rgb[idx + 1] = out_G;
            rgb[idx + 2] = out_B;
        }
    }
};

std::unique_ptr<Colorbalancergb> makeColorbalancergb()
{
    return std::make_unique<ColorbalancergbImpl>();
}

} // namespace flow
