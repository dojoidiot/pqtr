// colorout.cpp - Lab → XYZ → linear sRGB (D50 adapted)
//
// Matches darktable's colorout module.
// Uses D50 adapted XYZ→sRGB matrix (not standard D65).

#include "../../../inc/pipe.hpp"
#include <cmath>

namespace flow
{

// D50 white point
static constexpr float D50_X = 0.9642f;
static constexpr float D50_Y = 1.0000f;
static constexpr float D50_Z = 0.8249f;

// Lab f^-1() function
static inline float lab_f_inv(float t)
{
    constexpr float delta = 6.0f / 29.0f;

    if (t > delta) {
        return t * t * t;
    } else {
        return 3.0f * delta * delta * (t - 4.0f / 29.0f);
    }
}

// D50 adapted XYZ → sRGB matrix (from DT colorspaces_inline_conversions.h)
// This is NOT the standard D65 sRGB matrix!
static constexpr float XYZ_TO_SRGB[9] = {
     3.1338561f, -1.6168667f, -0.4906146f,
    -0.9787684f,  1.9161415f,  0.0334540f,
     0.0719453f, -0.2289914f,  1.4052427f
};

class ColoroutImpl : public Colorout
{
public:
    std::string name() const override { return "colorout"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        float* data = flow.rgb();

        // Process each pixel: Lab → XYZ → linear sRGB
        for (size_t i = 0; i < npixels; i++)
        {
            size_t idx = i * 4;
            float L = data[idx + 0];
            float a = data[idx + 1];
            float b = data[idx + 2];

            // Step 1: Lab → XYZ (D50)
            float fy = (L + 16.0f) / 116.0f;
            float fx = a / 500.0f + fy;
            float fz = fy - b / 200.0f;

            float X = D50_X * lab_f_inv(fx);
            float Y = D50_Y * lab_f_inv(fy);
            float Z = D50_Z * lab_f_inv(fz);

            // Step 2: XYZ → linear sRGB (D50 adapted)
            float r = XYZ_TO_SRGB[0] * X + XYZ_TO_SRGB[1] * Y + XYZ_TO_SRGB[2] * Z;
            float g = XYZ_TO_SRGB[3] * X + XYZ_TO_SRGB[4] * Y + XYZ_TO_SRGB[5] * Z;
            float bval = XYZ_TO_SRGB[6] * X + XYZ_TO_SRGB[7] * Y + XYZ_TO_SRGB[8] * Z;

            // Store linear sRGB
            data[idx + 0] = r;
            data[idx + 1] = g;
            data[idx + 2] = bval;
        }
    }
};

std::unique_ptr<Colorout> makeColorout()
{
    return std::make_unique<ColoroutImpl>();
}

} // namespace flow
