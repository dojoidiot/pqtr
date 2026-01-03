// TemperatureStep.cpp - white balance on bayer

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/temperature.c"
}

namespace pqtr {

void* TemperatureStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup temperature data exactly as pipe/gold.cpp
    TemperatureData data;
    data.coeffs[0] = static_cast<float>(state.chroma.as_shot[0]);
    data.coeffs[1] = static_cast<float>(state.chroma.as_shot[1]);
    data.coeffs[2] = static_cast<float>(state.chroma.as_shot[2]);
    data.coeffs[3] = static_cast<float>(state.chroma.as_shot[1]);  // Note: uses [1] not [3]
    data.preset = 4;

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels);

    temperature_process(in, out.data(), &state, &data);

    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "TemperatureStep: WB R=" << data.coeffs[0]
              << " G=" << data.coeffs[1] << " B=" << data.coeffs[2] << "\n";

    return flow.data();
}

}  // namespace pqtr
