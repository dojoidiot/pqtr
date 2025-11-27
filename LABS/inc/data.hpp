// data.hpp
// Data persistence API for LABS
//
// Provides serialization for all LABS data types:
// - tune::Data (loss metrics)
// - pipe::Link (edit steps) - future
// - tune::Result (optimization results) - future
//
// See data.md for format specification.

#pragma once

#include <tune.hpp>
#include <string>

namespace data
{
    // ============================================================
    // Tune Data (Loss Metrics)
    // ============================================================

    namespace tune
    {
        // Serialize to JSON string
        std::string toJson(const ::tune::Data& d);

        // Deserialize from JSON string
        ::tune::Data fromJson(const std::string& json);

        // Save to file
        bool save(const ::tune::Data& d, const std::string& path);

        // Load from file
        ::tune::Data load(const std::string& path);

    } // namespace tune

} // namespace data
