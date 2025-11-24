// module.h
// Phase 1 Module Interface
//
// Defines the common interface for all Phase 1 processing modules.
// Extends Phase 0 with Camera/View/Style phase separation.
// Each module is self-contained and parameter-agnostic.

#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <map>

namespace mods {

// Module parameter container
// Maps parameter names to float values (can be populated from any source)
using Params = std::map<std::string, float>;

// Module interface
// Each module implements process() to transform input → output
class Module {
public:
    virtual ~Module() = default;

    // Process image with given parameters
    // Input/output are cv::UMat for GPU processing
    // Returns false on error
    virtual bool process(
        const cv::UMat& input,
        cv::UMat& output,
        const Params& params
    ) = 0;

    // Module name (for logging and tracking)
    virtual std::string name() const = 0;

    // Default parameters
    virtual Params defaults() const = 0;

protected:
    // Helper: Check if module is enabled via params["enabled"]
    // Returns true if params["enabled"] != 0.0f (or not present)
    // If disabled, module should do passthrough: output = input
    static bool isEnabled(const Params& params) {
        auto it = params.find("enabled");
        if (it == params.end()) return true;  // Default: enabled
        return it->second != 0.0f;
    }
};

} // namespace mods
