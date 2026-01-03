// ExposureStep.cpp - exposure compensation

#include "pqtr.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/exposure.c"
}

namespace pqtr::Labs {

class ExposureStep : public Step
{
public:
    void *exec(Flow &flow) override;
};

void* ExposureStep::exec(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;
    float exposureBias = flow.exposureBias();

    // Setup params exactly as pipe/gold.cpp
    ExposureParams params;
    params.mode = 0;
    params.black = 0.0f;
    params.exposure = exposureBias;  // From camera metadata
    params.deflicker_percentile = 50.0f;
    params.deflicker_target_level = -4.0f;
    params.compensate_exposure_bias = 0;

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    exposure_process(in, out.data(), width, height, 4, &params);

    // Record params in flow (exposure from head, rest defaults)
    Stem& m = flow.flow().next("exposure");
    m.leaf("mode").dial(static_cast<float>(params.mode));
    m.leaf("black").dial(params.black);
    m.leaf("exposure").dial(params.exposure);
    m.leaf("compensate_exposure_bias").dial(static_cast<float>(params.compensate_exposure_bias));

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "ExposureStep: " << exposureBias << " EV\n";

    return flow.data();
}

std::unique_ptr<Step> exposureStep() { return std::make_unique<ExposureStep>(); }

}  // namespace pqtr::Labs
