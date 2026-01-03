// ColoroutStep.cpp - Rec2020 → sRGB with gamma

#include "pqtr.hpp"
#include <iostream>
#include <cmath>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/colorout.c"
}

// Rec2020 -> XYZ matrix (from colorin.c)
static const float REC2020_to_XYZ[4][4] = {
    { 0.673474789f, 0.165675461f, 0.125049725f, 0.f },
    { 0.279040545f, 0.675347328f, 0.045612101f, 0.f },
    { -0.001932710f, 0.029981442f, 0.796851277f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

namespace pqtr::Labs {

class ColoroutStep : public Step
{
public:
    void *exec(Flow &flow) override;
};

void* ColoroutStep::exec(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;

    // Record in flow
    flow.flow().next("colorout").leaf("profile").dial(0);  // sRGB

    // Process in-place
    float* data = static_cast<float*>(flow.data());

    for (size_t i = 0; i < npixels; i++) {
        float* px = data + i * 4;

        // Rec2020 -> XYZ
        float xyz[3];
        xyz[0] = REC2020_to_XYZ[0][0] * px[0] + REC2020_to_XYZ[0][1] * px[1] + REC2020_to_XYZ[0][2] * px[2];
        xyz[1] = REC2020_to_XYZ[1][0] * px[0] + REC2020_to_XYZ[1][1] * px[1] + REC2020_to_XYZ[1][2] * px[2];
        xyz[2] = REC2020_to_XYZ[2][0] * px[0] + REC2020_to_XYZ[2][1] * px[1] + REC2020_to_XYZ[2][2] * px[2];

        // XYZ -> linear sRGB
        float lin[3];
        lin[0] = XYZ_D65_to_sRGB[0][0] * xyz[0] + XYZ_D65_to_sRGB[0][1] * xyz[1] + XYZ_D65_to_sRGB[0][2] * xyz[2];
        lin[1] = XYZ_D65_to_sRGB[1][0] * xyz[0] + XYZ_D65_to_sRGB[1][1] * xyz[1] + XYZ_D65_to_sRGB[1][2] * xyz[2];
        lin[2] = XYZ_D65_to_sRGB[2][0] * xyz[0] + XYZ_D65_to_sRGB[2][1] * xyz[1] + XYZ_D65_to_sRGB[2][2] * xyz[2];

        // sRGB gamma
        for (int c = 0; c < 3; c++) {
            float v = lin[c];
            if (v < 0.0f) v = 0.0f;
            if (v <= 0.0031308f)
                px[c] = 12.92f * v;
            else
                px[c] = 1.055f * powf(v, 1.0f/2.4f) - 0.055f;
        }
    }

    std::cout << "ColoroutStep: Rec2020 → sRGB\n";

    return flow.data();
}

std::unique_ptr<Step> coloroutStep() { return std::make_unique<ColoroutStep>(); }

}  // namespace pqtr::Labs
