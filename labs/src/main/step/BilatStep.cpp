// BilatStep.cpp - local contrast (local laplacian)

#include "labs.hpp"
#include <iostream>
#include <cstring>

// Define missing macro before including bilat.c
#ifndef COPY_PIXEL_DEFINED
#define COPY_PIXEL_DEFINED
#define copy_pixel(to, from) do { for(int c = 0; c < 4; c++) (to)[c] = (from)[c]; } while(0)
#endif

extern "C" {
#include "../../../../pipe/src/main/labs/mods/bilat.c"
}

namespace pqtr {

void* BilatStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup data from DT defaults (same as pipe gold.cpp)
    BilatData data;
    data.mode = 1;           // local_laplacian
    data.sigma_r = 0.5f;     // highlights
    data.sigma_s = 0.5f;     // shadows
    data.detail = 0.1f;      // clarity
    data.midtone = 0.5f;     // sigma

    // Record in flow
    Stem& m = flow.flow().next("bilat");
    m.leaf("mode").dial(static_cast<float>(data.mode));
    m.leaf("sigma_r").dial(data.sigma_r);
    m.leaf("sigma_s").dial(data.sigma_s);
    m.leaf("detail").dial(data.detail);
    m.leaf("midtone").dial(data.midtone);

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    bilat_process(in, out.data(), width, height, &data);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "BilatStep: local laplacian\n";

    return flow.data();
}

}  // namespace pqtr
