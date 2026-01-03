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
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    // Get exposure bias from head
    float exposure_bias = 1.05f;
    if (flow.head().test("exposure_bias"))
        exposure_bias = flow.head().leaf("exposure_bias").dial();

    // Setup params
    ExposureParams params;
    params.mode = 0;
    params.black = 0.0f;
    params.exposure = exposure_bias;
    params.deflicker_percentile = 50.0f;
    params.deflicker_target_level = -4.0f;
    params.compensate_exposure_bias = 0;

    // Record in flow
    Stem& m = flow.flow().next("exposure");
    m.leaf("ev").dial(exposure_bias);

    // Process in-place
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    exposure_process(in, out.data(), width, height, 4, &params);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "ExposureStep: " << exposure_bias << " EV\n";

    return flow.data();
}

}  // namespace pqtr
