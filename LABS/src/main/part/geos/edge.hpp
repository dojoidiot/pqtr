// edge.hpp
// Internal: Golden section optimizer for detail dials (4 parameters)
// Not a public header - used only within geos module

#pragma once

#include <geos.hpp>
#include "diff.hpp"

namespace geos::internal
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

} // namespace geos::internal
