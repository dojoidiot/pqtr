// gamma.cpp - sRGB transfer function
//
// Matches darktable's gamma module.
// Input: linear sRGB from colorout
// Output: display sRGB (gamma corrected)

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>  // std::max, std::min

namespace flow
{

class GammaImpl : public Gamma
{
public:
    std::string name() const override { return "gamma"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        float* rgb = flow.rgb();

        // sRGB gamma function (IEC 61966-2-1)
        auto srgb_gamma = [](float v) -> float {
            if (v <= 0.0031308f) {
                return 12.92f * v;
            } else {
                return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
            }
        };

        // Process each pixel: linear sRGB → display sRGB
        for (size_t i = 0; i < npixels; i++)
        {
            size_t idx = i * 4;

            // Clip to [0, 1] then apply gamma
            float r = std::max(0.0f, std::min(1.0f, rgb[idx + 0]));
            float g = std::max(0.0f, std::min(1.0f, rgb[idx + 1]));
            float b = std::max(0.0f, std::min(1.0f, rgb[idx + 2]));

            rgb[idx + 0] = srgb_gamma(r);
            rgb[idx + 1] = srgb_gamma(g);
            rgb[idx + 2] = srgb_gamma(b);
        }
    }
};

std::unique_ptr<Gamma> makeGamma()
{
    return std::make_unique<GammaImpl>();
}

} // namespace flow
