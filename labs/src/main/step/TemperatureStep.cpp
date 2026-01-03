// TemperatureStep.cpp - white balance on bayer

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

// Need PipeState for temperature
extern "C" {
#include "../../../../pipe/src/main/labs/pipe_state.h"
#include "../../../../pipe/src/main/labs/mods/temperature.c"
}

namespace pqtr {

void* TemperatureStep::exec(Flow& flow) {
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    // Get WB from head
    Stem& wb = flow.head().next("wb");
    float wb_r = wb.test("r") ? wb.leaf("r").dial() : 1.0f;
    float wb_g1 = wb.test("g1") ? wb.leaf("g1").dial() : 1.0f;
    float wb_b = wb.test("b") ? wb.leaf("b").dial() : 1.0f;
    float wb_g2 = wb.test("g2") ? wb.leaf("g2").dial() : 1.0f;

    // Setup PipeState
    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;
    // filters stored as hex string to preserve uint32 precision
    std::string filters_str = flow.head().leaf("filters").text();
    state.filters = static_cast<uint32_t>(std::stoul(filters_str, nullptr, 0));
    state.chroma.as_shot[0] = wb_r;
    state.chroma.as_shot[1] = wb_g1;
    state.chroma.as_shot[2] = wb_b;
    state.chroma.as_shot[3] = wb_g2;

    // Setup temperature data
    TemperatureData data;
    data.coeffs[0] = wb_r;
    data.coeffs[1] = wb_g1;
    data.coeffs[2] = wb_b;
    data.coeffs[3] = wb_g1;
    data.preset = 4;

    // Record params in flow
    Stem& m = flow.flow().next("temperature");
    m.leaf("r").dial(wb_r);
    m.leaf("g1").dial(wb_g1);
    m.leaf("b").dial(wb_b);
    m.leaf("g2").dial(wb_g2);

    // Process in-place
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels);

    temperature_process(in, out.data(), &state, &data);

    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "TemperatureStep: WB R=" << wb_r << " G=" << wb_g1 << " B=" << wb_b << "\n";

    return flow.data();
}

}  // namespace pqtr
