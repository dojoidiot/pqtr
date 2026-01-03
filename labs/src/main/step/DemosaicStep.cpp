// DemosaicStep.cpp - bayer → RGB

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/pipe_state.h"
#include "../../../../pipe/src/main/labs/mods/demosaic.c"
}

namespace pqtr {

void* DemosaicStep::exec(Flow& flow) {
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup PipeState
    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;
    // filters stored as hex string to preserve uint32 precision
    std::string filters_str = flow.head().leaf("filters").text();
    state.filters = static_cast<uint32_t>(std::stoul(filters_str, nullptr, 0));

    // PPG demosaic
    DemosaicParams params;
    memset(&params, 0, sizeof(params));
    params.demosaicing_method = 0;  // PPG
    params.dual_thrs = 0.2f;
    params.cs_thrs = 0.40f;
    params.cs_iter = 8;

    // Record in flow
    Stem& m = flow.flow().next("demosaic");
    m.leaf("method").dial(0);

    // Input is float bayer, output is float RGBA (4 channels)
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    demosaic_process(in, out.data(), &state, &params);

    // Resize flow buffer for RGBA
    flow.resize(npixels * 4 * sizeof(float));
    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "DemosaicStep: PPG " << width << "x" << height << " → RGBA\n";

    return flow.data();
}

}  // namespace pqtr
