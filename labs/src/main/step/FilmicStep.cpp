// FilmicStep.cpp - tone mapping (HDR → SDR)

#include "labs.hpp"
#include <iostream>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/filmicrgb.c"
}

namespace pqtr {

void* FilmicStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    FilmicRGBData data;
    filmicrgb_reset(&data);

    // Record in flow
    flow.flow().next("filmic").leaf("enabled").dial(1);

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    filmicrgb_process(in, out.data(), width, height, &data,
                      FILMIC_INPUT_MATRIX_TRANS, FILMIC_OUTPUT_MATRIX,
                      FILMIC_OUTPUT_MATRIX_TRANS, FILMIC_EXPORT_INPUT_MATRIX_TRANS,
                      FILMIC_EXPORT_OUTPUT_MATRIX, FILMIC_EXPORT_OUTPUT_MATRIX_TRANS,
                      0.0f, 1.0f, 1);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "FilmicStep: tone mapping\n";

    return flow.data();
}

}  // namespace pqtr
