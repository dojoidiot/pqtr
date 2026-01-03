// HighlightsStep.cpp - highlight recovery

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/highlights.c"
}

namespace pqtr {

void* HighlightsStep::exec(Flow& flow) {
    PipeState& state = flow.state();
    int width = state.width;
    int height = state.height;
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup params exactly as pipe/gold.cpp
    HighlightsParams params;
    highlights_reset(&params, DT_IOP_HIGHLIGHTS_OPPOSED, 1.0f, 1.0f);
    HighlightsData data = params;

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels);

    highlights_process(in, out.data(), &state, &data);

    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "HighlightsStep: OPPOSED mode\n";

    return flow.data();
}

}  // namespace pqtr
