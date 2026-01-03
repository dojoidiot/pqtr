// TemperatureStep.cpp - white balance on bayer

#include "pqtr.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/temperature.c"
}

namespace pqtr::Labs {

class TemperatureStep : public Step
{
public:
    void *exec(Flow &flow) override;
};

void* TemperatureStep::exec(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup temperature data from camera WB
    TemperatureData data;
    data.coeffs[0] = static_cast<float>(flow.chroma().asShot(0));
    data.coeffs[1] = static_cast<float>(flow.chroma().asShot(1));
    data.coeffs[2] = static_cast<float>(flow.chroma().asShot(2));
    data.coeffs[3] = static_cast<float>(flow.chroma().asShot(1));  // Note: uses [1] not [3]
    data.preset = 4;

    // Build local PipeState for C module
    PipeState state;
    state.width = width;
    state.height = height;
    state.filters = flow.filters();

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels);

    temperature_process(in, out.data(), &state, &data);

    // Write temperature output back to flow
    flow.temperature().enabled(state.temperature.enabled != 0);
    for (int k = 0; k < 4; k++)
        flow.temperature().coeff(k, state.temperature.coeffs[k]);

    // Record params in flow (camera WB from head)
    Stem& m = flow.flow().next("temperature");
    m.leaf("coeff_r").dial(data.coeffs[0]);
    m.leaf("coeff_g").dial(data.coeffs[1]);
    m.leaf("coeff_b").dial(data.coeffs[2]);
    m.leaf("preset").dial(static_cast<float>(data.preset));

    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "TemperatureStep: WB R=" << data.coeffs[0]
              << " G=" << data.coeffs[1] << " B=" << data.coeffs[2] << "\n";

    return flow.data();
}

std::unique_ptr<Step> temperatureStep() { return std::make_unique<TemperatureStep>(); }

}  // namespace pqtr::Labs
