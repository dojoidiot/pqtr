// HighlightsStep.cpp - highlight recovery

#include "pqtr.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/highlights.c"
}

namespace pqtr::Labs {

class HighlightsStep : public Step
{
public:
    void *exec(Flow &flow) override;
};

void* HighlightsStep::exec(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;

    // Setup params exactly as pipe/gold.cpp
    HighlightsParams params;
    highlights_reset(&params, DT_IOP_HIGHLIGHTS_OPPOSED, 1.0f, 1.0f);
    HighlightsData data = params;

    // Build local PipeState for C module
    PipeState state;
    state.width = width;
    state.height = height;
    state.filters = flow.filters();
    state.temperature.enabled = flow.temperature().enabled() ? 1 : 0;
    for (int k = 0; k < 4; k++)
        state.temperature.coeffs[k] = flow.temperature().coeff(k);
    state.chroma.late_correction = flow.chroma().lateCorrection() ? 1 : 0;
    for (int k = 0; k < 4; k++) {
        state.chroma.D65coeffs[k] = flow.chroma().D65(k);
        state.chroma.as_shot[k] = flow.chroma().asShot(k);
    }

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels);

    highlights_process(in, out.data(), &state, &data);

    // Record params in flow (algorithm defaults)
    Stem& m = flow.flow().next("highlights");
    m.leaf("mode").dial(static_cast<float>(params.mode));
    m.leaf("clip").dial(params.clip);
    m.leaf("candidating").dial(params.candidating);

    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "HighlightsStep: OPPOSED mode\n";

    return flow.data();
}

std::unique_ptr<Step> highlightsStep() { return std::make_unique<HighlightsStep>(); }

}  // namespace pqtr::Labs
