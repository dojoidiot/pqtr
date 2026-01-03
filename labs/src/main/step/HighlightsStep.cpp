// HighlightsStep.cpp - highlight recovery

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/pipe_state.h"
#include "../../../../pipe/src/main/labs/mods/highlights.c"
}

namespace pqtr {

void* HighlightsStep::exec(Flow& flow) {
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup PipeState with temperature coeffs
    PipeState state;
    memset(&state, 0, sizeof(state));
    state.width = width;
    state.height = height;
    // filters stored as hex string to preserve uint32 precision
    std::string filters_str = flow.head().leaf("filters").text();
    state.filters = static_cast<uint32_t>(std::stoul(filters_str, nullptr, 0));

    // Get temp coeffs from flow
    if (flow.flow().test("temperature")) {
        Stem& temp = flow.flow().next("temperature");
        state.temperature.enabled = 1;
        state.temperature.coeffs[0] = temp.leaf("r").dial();
        state.temperature.coeffs[1] = temp.leaf("g1").dial();
        state.temperature.coeffs[2] = temp.leaf("b").dial();
        state.temperature.coeffs[3] = temp.leaf("g1").dial();
    }

    // OPPOSED mode
    HighlightsParams params;
    highlights_reset(&params, DT_IOP_HIGHLIGHTS_OPPOSED, 1.0f, 1.0f);
    HighlightsData data = params;

    // Record in flow
    Stem& m = flow.flow().next("highlights");
    m.leaf("mode").dial(static_cast<float>(DT_IOP_HIGHLIGHTS_OPPOSED));

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels);

    highlights_process(in, out.data(), &state, &data);

    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "HighlightsStep: OPPOSED mode\n";

    return flow.data();
}

}  // namespace pqtr
