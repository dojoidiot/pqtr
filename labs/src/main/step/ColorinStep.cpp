// ColorinStep.cpp - Camera RGB → Rec2020

#include "labs.hpp"
#include <iostream>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/colorin.c"
}

namespace pqtr {

void* ColorinStep::exec(Flow& flow) {
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    // cam_to_xyz from DT dump (hardcoded for now)
    const float cam_to_xyz[3][3] = {
        { 0.673474789f, 0.165675461f, 0.125049725f },
        { 0.279040545f, 0.675347328f, 0.045612101f },
        { -0.001932710f, 0.029981442f, 0.796851277f }
    };

    // Record in flow
    Stem& m = flow.flow().next("colorin");
    m.leaf("working_space").dial(2020);  // Rec2020

    // Process
    float* data = static_cast<float*>(flow.data());

    for (size_t i = 0; i < npixels; i++) {
        float* px = data + i * 4;

        // Camera RGB -> XYZ
        float xyz[3];
        xyz[0] = cam_to_xyz[0][0] * px[0] + cam_to_xyz[0][1] * px[1] + cam_to_xyz[0][2] * px[2];
        xyz[1] = cam_to_xyz[1][0] * px[0] + cam_to_xyz[1][1] * px[1] + cam_to_xyz[1][2] * px[2];
        xyz[2] = cam_to_xyz[2][0] * px[0] + cam_to_xyz[2][1] * px[1] + cam_to_xyz[2][2] * px[2];

        // XYZ -> Rec2020
        px[0] = XYZ_to_REC2020[0][0] * xyz[0] + XYZ_to_REC2020[0][1] * xyz[1] + XYZ_to_REC2020[0][2] * xyz[2];
        px[1] = XYZ_to_REC2020[1][0] * xyz[0] + XYZ_to_REC2020[1][1] * xyz[1] + XYZ_to_REC2020[1][2] * xyz[2];
        px[2] = XYZ_to_REC2020[2][0] * xyz[0] + XYZ_to_REC2020[2][1] * xyz[1] + XYZ_to_REC2020[2][2] * xyz[2];
    }

    std::cout << "ColorinStep: Camera RGB → Rec2020\n";

    return flow.data();
}

}  // namespace pqtr
