// ColorBalanceStep.cpp - color grading

#include "labs.hpp"
#include <iostream>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/colorbalancergb.c"
}

namespace pqtr {

void* ColorBalanceStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    ColorBalanceRGBData data;
    colorbalancergb_reset(&data);

    // DT pre-multiplied matrices
    dt_colormatrix_t input_matrix = {
        { 0.406808585f, 0.617819786f, 0.045817737f, 0.0f },
        { 0.067756824f, 0.748962402f, 0.100109622f, 0.0f },
        { 0.022140553f, -0.015321352f, 0.587274075f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };
    dt_colormatrix_t output_matrix = {
        { 1.662934422f, -0.321330518f, -0.237917423f, 0.0f },
        { -0.681079328f, 1.609099507f, 0.035052136f, 0.0f },
        { 0.029973516f, -0.075743161f, 0.961853564f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    // Record in flow (defaults from colorbalancergb_reset)
    Stem& m = flow.flow().next("colorbalance");
    m.leaf("shadows_weight").dial(data.shadows_weight);
    m.leaf("highlights_weight").dial(data.highlights_weight);
    m.leaf("midtones_weight").dial(data.midtones_weight);
    m.leaf("mask_grey_fulcrum").dial(data.mask_grey_fulcrum);
    m.leaf("chroma_global").dial(data.chroma_global);
    m.leaf("vibrance").dial(data.vibrance);
    m.leaf("contrast").dial(data.contrast);

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    colorbalancergb_process(in, out.data(), width, height,
                            input_matrix, output_matrix, &data);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "ColorBalanceStep: grading\n";

    return flow.data();
}

}  // namespace pqtr
