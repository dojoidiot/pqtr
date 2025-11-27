// edge.hpp
// Internal: Golden section optimizer for detail dials (4 parameters)
// Not a public header - used only within tune module

#pragma once

#include <tune.hpp>
#include "diff.hpp"

namespace tune::internal
{

    // Run golden section optimization on detail dials
    // Returns number of evaluations performed
    int optimizeEdge(
        pipe::Body& body,
        pipe::Body::Link& link,
        float targetLaplacianVar,
        const Config& config,
        Callback progress
    );

} // namespace tune::internal
