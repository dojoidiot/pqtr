// RawprepareStep.cpp - uint16 bayer → float bayer (normalized)

#include "labs.hpp"
#include <iostream>
#include <vector>
#include <cstring>

extern "C" {
#include "../../../../pipe/src/main/labs/mods/rawprepare.c"
}

namespace pqtr {

void* RawprepareStep::exec(Flow& flow) {
    int width = static_cast<int>(flow.head().leaf(WIDTH).dial());
    int height = static_cast<int>(flow.head().leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    // Get black/white from head (camera-specific)
    int black = 512;
    int white = 16383;
    if (flow.head().test(BLACK))
        black = static_cast<int>(flow.head().leaf(BLACK).dial());
    if (flow.head().test(WHITE))
        white = static_cast<int>(flow.head().leaf(WHITE).dial());

    // Setup params
    RawprepareParams params;
    rawprepare_reset(&params, 0, 0, 0, 0,
                     black, black, black, black, white);

    RawprepareData data;
    rawprepare_commit_params(&params, &data);

    // Record params in flow
    Stem& m = flow.flow().next("rawprepare");
    m.leaf("black").dial(static_cast<float>(black));
    m.leaf("white").dial(static_cast<float>(white));

    // Input is uint16, output is float
    uint16_t* in = static_cast<uint16_t*>(flow.data());

    // Allocate new buffer for float output
    std::vector<float> out(npixels);

    rawprepare_process(in, out.data(), width, height, width, height, &data);

    // Resize flow buffer and copy result
    flow.resize(npixels * sizeof(float));
    std::memcpy(flow.data(), out.data(), npixels * sizeof(float));

    std::cout << "RawprepareStep: " << width << "x" << height
              << " black=" << black << " white=" << white << "\n";

    return flow.data();
}

}  // namespace pqtr
