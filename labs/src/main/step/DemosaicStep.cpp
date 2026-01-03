// DemosaicStep.cpp - bayer → RGB

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/demosaic.c"
}

namespace pqtr {

void* DemosaicStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup params exactly as pipe/gold.cpp
    DemosaicParams params;
    memset(&params, 0, sizeof(params));
    params.demosaicing_method = 0;  // PPG
    params.dual_thrs = 0.2f;
    params.cs_thrs = 0.40f;
    params.cs_iter = 8;

    // Input is float bayer, output is float RGBA (4 channels)
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    demosaic_process(in, out.data(), &state, &params);

    // Record params in flow (algorithm defaults)
    Stem& m = flow.flow().next("demosaic");
    m.leaf("method").dial(static_cast<float>(params.demosaicing_method));
    m.leaf("dual_thrs").dial(params.dual_thrs);
    m.leaf("cs_thrs").dial(params.cs_thrs);
    m.leaf("cs_iter").dial(static_cast<float>(params.cs_iter));

    // Resize flow buffer for RGBA
    flow.resize(npixels * 4 * sizeof(float));
    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "DemosaicStep: PPG " << width << "x" << height << " → RGBA\n";

    return flow.data();
}

}  // namespace pqtr
