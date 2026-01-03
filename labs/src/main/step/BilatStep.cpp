// BilatStep.cpp - local contrast (pass-through for now - bilat.c has DT dependencies)

#include "labs.hpp"
#include <iostream>

namespace pqtr {

void* BilatStep::exec(Flow& flow) {
    // TODO: bilat.c uses dt_aligned_pixel_t, dt_alloc_align_float, etc.
    // For now, pass through unchanged

    flow.flow().next("bilat").leaf("enabled").dial(0);

    std::cout << "BilatStep: pass-through (TODO)\n";

    return flow.data();
}

}  // namespace pqtr
