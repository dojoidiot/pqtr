// DemosaicStep.cpp - bayer → RGB

#include "pqtr.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/demosaic.c"
}

namespace pqtr::Labs {

class DemosaicStep : public Step
{
public:
    void *exec(Flow &flow) override;
};

void* DemosaicStep::exec(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup params exactly as pipe/gold.cpp
    DemosaicParams params;
    memset(&params, 0, sizeof(params));
    params.demosaicing_method = 0;  // PPG
    params.dual_thrs = 0.2f;
    params.cs_thrs = 0.40f;
    params.cs_iter = 8;

    // Build local PipeState for C module
    PipeState state;
    state.width = width;
    state.height = height;
    state.filters = flow.filters();

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

std::unique_ptr<Step> demosaicStep() { return std::make_unique<DemosaicStep>(); }

}  // namespace pqtr::Labs
