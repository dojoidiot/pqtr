// channelmixerrgb.cpp - RGB channel mixing
//
// CLEAN COPY from darktable src/iop/channelmixerrgb.c
// Simple RGB channel mixing with 3x3 matrix + optional saturation/lightness.
//
// IOP order: 28.5 (after exposure, before colorbalancergb)
// Works in: RGB (scene-linear)

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>

namespace flow
{

// =============================================================================
// Implementation
// =============================================================================

class ChannelmixerrgbImpl : public Channelmixerrgb
{
    // RGB mixing matrix (3x3): out = M * in
    // Row 0: red output = red[0]*R + red[1]*G + red[2]*B
    // Row 1: green output = green[0]*R + green[1]*G + green[2]*B
    // Row 2: blue output = blue[0]*R + blue[1]*G + blue[2]*B
    float red_[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float green_[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    float blue_[4] = {0.0f, 0.0f, 1.0f, 0.0f};

    // Saturation/lightness/grey adjustments (per-channel)
    float saturation_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float lightness_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float grey_[4] = {0.0f, 0.0f, 0.0f, 0.0f};

public:
    std::string name() const override { return "channelmixerrgb"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setMatrix(const float red[4], const float green[4], const float blue[4]) override
    {
        for (int i = 0; i < 4; i++) {
            red_[i] = red[i];
            green_[i] = green[i];
            blue_[i] = blue[i];
        }
    }

    void setSaturation(const float sat[4]) override
    {
        for (int i = 0; i < 4; i++) saturation_[i] = sat[i];
    }

    void setLightness(const float light[4]) override
    {
        for (int i = 0; i < 4; i++) lightness_[i] = light[i];
    }

    void setGrey(const float grey[4]) override
    {
        for (int i = 0; i < 4; i++) grey_[i] = grey[i];
    }

    bool isIdentity() const
    {
        // Check if matrix is identity
        const float eps = 1e-6f;
        bool identity =
            std::abs(red_[0] - 1.0f) < eps && std::abs(red_[1]) < eps && std::abs(red_[2]) < eps &&
            std::abs(green_[0]) < eps && std::abs(green_[1] - 1.0f) < eps && std::abs(green_[2]) < eps &&
            std::abs(blue_[0]) < eps && std::abs(blue_[1]) < eps && std::abs(blue_[2] - 1.0f) < eps;

        // Check if saturation/lightness/grey are all zero
        for (int i = 0; i < 4; i++) {
            if (std::abs(saturation_[i]) > eps) identity = false;
            if (std::abs(lightness_[i]) > eps) identity = false;
            if (std::abs(grey_[i]) > eps) identity = false;
        }

        return identity;
    }

    void process(Flow& flow) override
    {
        // Fast path: identity matrix with no adjustments
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

            // Apply 3x3 mixing matrix
            // From DT: output[c] = red[c]*R + green[c]*G + blue[c]*B
            // But our layout is: red row, green row, blue row
            float out_R = red_[0] * R + red_[1] * G + red_[2] * B;
            float out_G = green_[0] * R + green_[1] * G + green_[2] * B;
            float out_B = blue_[0] * R + blue_[1] * G + blue_[2] * B;

            rgb[idx + 0] = out_R;
            rgb[idx + 1] = out_G;
            rgb[idx + 2] = out_B;
        }
    }
};

std::unique_ptr<Channelmixerrgb> makeChannelmixerrgb()
{
    return std::make_unique<ChannelmixerrgbImpl>();
}

} // namespace flow
