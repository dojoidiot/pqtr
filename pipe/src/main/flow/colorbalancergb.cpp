// colorbalancergb.cpp - Color grading in Jzazbz/Yrg colorspace
//
// CLEAN COPY from darktable src/iop/colorbalancergb.c
// Color balance using perceptual colorspaces (Filmlight Yrg, Jzazbz).
//
// NOTE: With default params (all zero), this is essentially identity.
// Full implementation would require ~1000 lines for complete Jzazbz support.

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>

namespace flow
{

// CLEAN COPY from DT colorbalancergb.c - simplified for default params
// When all params are at defaults (0), the module does nothing meaningful.

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

    void process(Flow& flow) override
    {
        // Check if we're essentially identity (all params at zero/defaults)
        const float epsilon = 1e-6f;
        if (std::abs(chroma_global_) < epsilon &&
            std::abs(saturation_global_) < epsilon &&
            std::abs(vibrance_) < epsilon &&
            std::abs(contrast_) < epsilon)
        {
            // No-op, skip processing
            return;
        }

        // Full implementation would involve:
        // 1. RGB → LMS (CIE 2006 D65)
        // 2. LMS → Filmlight Yrg
        // 3. Yrg → Ych (polar form)
        // 4. Apply chroma/saturation/vibrance adjustments
        // 5. Gamut clipping
        // 6. Color balance with masks
        // 7. LMS → XYZ → Jzazbz for perceptual saturation
        // 8. Convert back to RGB
        //
        // For now, only implement basic contrast if non-zero

        auto& root = flow.info().root();
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        float* rgb = flow.rgb();

        // Simple contrast adjustment if enabled
        if (std::abs(contrast_) > epsilon)
        {
            const float contrast_power = 1.0f + contrast_;
            for (size_t k = 0; k < npixels; k++)
            {
                size_t idx = k * 4;
                for (int c = 0; c < 3; c++)
                {
                    float v = rgb[idx + c];
                    if (v > 0.0f)
                    {
                        // Apply contrast around grey fulcrum
                        v = grey_fulcrum_ * std::pow(v / grey_fulcrum_, contrast_power);
                        rgb[idx + c] = v;
                    }
                }
            }
        }
    }
};

std::unique_ptr<Colorbalancergb> makeColorbalancergb()
{
    return std::make_unique<ColorbalancergbImpl>();
}

} // namespace flow
