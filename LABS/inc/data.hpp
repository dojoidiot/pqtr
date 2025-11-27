// data.hpp
// Data persistence API for LABS
//
// Provides serialization for all LABS data types:
// - diff::Data (diff results)
// - pipe::Link (edit steps) - future
// - tune::Result (optimization results) - future
//
// See data.md for format specification.

#pragma once

#include <diff.hpp>
#include <string>

namespace data
{
    // ============================================================
    // Diff Data
    // ============================================================

    namespace diff
    {
        // Serialize to JSON string
        std::string toJson(const ::diff::Data& d);

        // Deserialize from JSON string
        ::diff::Data fromJson(const std::string& json);

        // Save to file
        bool save(const ::diff::Data& d, const std::string& path);

        // Load from file
        ::diff::Data load(const std::string& path);

    } // namespace diff

} // namespace data
