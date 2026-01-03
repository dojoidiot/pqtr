// FilmicStep.cpp - tone mapping (HDR → SDR)

#include "pqtr.hpp"
#include <iostream>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/filmicrgb.c"
}

namespace pqtr::Labs {

class FilmicStep : public Step
{
public:
    void *exec(Flow &flow) override;
};

void* FilmicStep::exec(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;

    FilmicRGBData data;
    filmicrgb_reset(&data);

    // Record in flow (defaults from filmicrgb_reset)
    Stem& m = flow.flow().next("filmic");
    m.leaf("grey_source").dial(data.grey_source);
    m.leaf("black_source").dial(data.black_source);
    m.leaf("white_source").dial(data.white_source);
    m.leaf("dynamic_range").dial(data.dynamic_range);
    m.leaf("contrast").dial(data.contrast);
    m.leaf("saturation").dial(data.saturation);

    // Process
    float* in = static_cast<float*>(flow.data());
    std::vector<float> out(npixels * 4);

    filmicrgb_process(in, out.data(), width, height, &data,
                      FILMIC_INPUT_MATRIX_TRANS, FILMIC_OUTPUT_MATRIX,
                      FILMIC_OUTPUT_MATRIX_TRANS, FILMIC_EXPORT_INPUT_MATRIX_TRANS,
                      FILMIC_EXPORT_OUTPUT_MATRIX, FILMIC_EXPORT_OUTPUT_MATRIX_TRANS,
                      0.0f, 1.0f, 1);

    std::memcpy(flow.data(), out.data(), npixels * 4 * sizeof(float));

    std::cout << "FilmicStep: tone mapping\n";

    return flow.data();
}

std::unique_ptr<Step> filmicStep() { return std::make_unique<FilmicStep>(); }

}  // namespace pqtr::Labs
