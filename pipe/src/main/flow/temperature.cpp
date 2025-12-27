// temperature.cpp - White balance correction
//
// Matches darktable's temperature module.
// Applies WB coefficients to BAYER MOSAIC data (before demosaic).
// This is the correct IOP stage order: rawprepare → temperature → demosaic

#include "../../../inc/pipe.hpp"
#include <algorithm>

namespace flow
{

// -------------------------------------------------------------------------
// TemperatureImpl - White balance on Bayer mosaic
// -------------------------------------------------------------------------

// Bayer pattern for RGGB: returns 0=R, 1=G, 2=B
static inline int bayer_color(int x, int y)
{
    int px = x & 1;
    int py = y & 1;
    if (py == 0) return px == 0 ? 0 : 1;  // R or G
    else         return px == 0 ? 1 : 2;  // G or B
}

class TemperatureImpl : public Temperature
{
    float coeffs_[3] = {1.0f, 1.0f, 1.0f};

public:
    std::string name() const override { return "temperature"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setCoeffs(float r, float g, float b) override
    {
        coeffs_[0] = r;
        coeffs_[1] = g;
        coeffs_[2] = b;
    }

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());

        float* bayer = flow.fdata();  // Work on mosaic, not RGB

        // Apply WB coefficients based on bayer position
        // Each pixel gets multiplied by its color's coefficient
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                size_t idx = static_cast<size_t>(y) * width + x;
                int color = bayer_color(x, y);  // 0=R, 1=G, 2=B
                bayer[idx] *= coeffs_[color];
            }
        }
    }
};

// -------------------------------------------------------------------------
// Factory
// -------------------------------------------------------------------------

std::unique_ptr<Temperature> makeTemperature()
{
    return std::make_unique<TemperatureImpl>();
}

} // namespace flow
