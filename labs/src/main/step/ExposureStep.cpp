// ExposureStep.cpp - exposure compensation

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/exposure.c"
}

namespace pqtr {

void* ExposureStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup params exactly as pipe/gold.cpp
    ExposureParams params;
    params.mode = 0;
    params.black = 0.0f;
    params.exposure = state.exposure_bias;  // From PipeState
    params.deflicker_percentile = 50.0f;
    params.deflicker_target_level = -4.0f;
    params.compensate_exposure_bias = 0;

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    exposure_process(in, out.data(), width, height, 4, &params);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "ExposureStep: " << state.exposure_bias << " EV\n";

    return flow.data();
}

}  // namespace pqtr
