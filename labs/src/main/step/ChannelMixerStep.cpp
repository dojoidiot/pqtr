// ChannelMixerStep.cpp - chromatic adaptation (CAT16)

#include "labs.hpp"
#include <iostream>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/channelmixerrgb.c"
}

// Rec2020 <-> XYZ matrices (from colorin.c)
static const float REC2020_to_XYZ[4][4] = {
    { 0.673474789f, 0.165675461f, 0.125049725f, 0.f },
    { 0.279040545f, 0.675347328f, 0.045612101f, 0.f },
    { -0.001932710f, 0.029981442f, 0.796851277f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const float XYZ_to_REC2020[4][4] = {
    { 1.647250295f, -0.393625855f, -0.235971376f, 0.f },
    { -0.682616651f, 1.647609591f, 0.012813044f, 0.f },
    { 0.029678674f, -0.062945843f, 1.253884912f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

namespace pqtr {

void* ChannelMixerStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;

    // Matrices for Rec2020 pipeline
    dt_colormatrix_t rec2020_to_xyz_4x4 = {
        { REC2020_to_XYZ[0][0], REC2020_to_XYZ[0][1], REC2020_to_XYZ[0][2], 0.0f },
        { REC2020_to_XYZ[1][0], REC2020_to_XYZ[1][1], REC2020_to_XYZ[1][2], 0.0f },
        { REC2020_to_XYZ[2][0], REC2020_to_XYZ[2][1], REC2020_to_XYZ[2][2], 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    dt_colormatrix_t xyz_to_rec2020_4x4 = {
        { XYZ_to_REC2020[0][0], XYZ_to_REC2020[0][1], XYZ_to_REC2020[0][2], 0.0f },
        { XYZ_to_REC2020[1][0], XYZ_to_REC2020[1][1], XYZ_to_REC2020[1][2], 0.0f },
        { XYZ_to_REC2020[2][0], XYZ_to_REC2020[2][1], XYZ_to_REC2020[2][2], 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    // Setup data from DT dump values
    ChannelMixerRGBData data;
    data.adaptation = DT_ADAPTATION_CAT16;

    // Illuminant in CAT16 LMS
    data.illuminant[0] = 1.003973126f;
    data.illuminant[1] = 0.993787944f;
    data.illuminant[2] = 0.741390944f;
    data.illuminant[3] = 0.0f;

    // Identity MIX matrix
    memset(data.MIX, 0, sizeof(data.MIX));
    data.MIX[0][0] = 1.0f;
    data.MIX[1][1] = 1.0f;
    data.MIX[2][2] = 1.0f;

    for (int i = 0; i < 4; i++) {
        data.saturation[i] = 0.0f;
        data.lightness[i] = 0.0f;
        data.grey[i] = 0.0f;
    }

    data.p = 1.008250713f;
    data.gamut = 1.0f;
    data.clip = 1;
    data.apply_grey = 0;
    data.version = CHANNELMIXERRGB_V_3;

    // Record in flow
    Stem& m = flow.flow().next("channelmixer");
    m.leaf("adaptation").dial(static_cast<float>(data.adaptation));
    m.leaf("illuminant_0").dial(data.illuminant[0]);
    m.leaf("illuminant_1").dial(data.illuminant[1]);
    m.leaf("illuminant_2").dial(data.illuminant[2]);
    m.leaf("p").dial(data.p);
    m.leaf("gamut").dial(data.gamut);
    m.leaf("clip").dial(static_cast<float>(data.clip));

    // Process in-place
    float* in = static_cast<float*>(flow.data());
    size_t npixels = static_cast<size_t>(width) * height;
    std::vector<float> out(npixels * 4);

    channelmixerrgb_process(in, out.data(), width, height,
                            rec2020_to_xyz_4x4, xyz_to_rec2020_4x4, &data);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "ChannelMixerStep: CAT16 adaptation\n";

    return flow.data();
}

}  // namespace pqtr
