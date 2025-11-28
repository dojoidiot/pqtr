// data.hpp
// Data persistence API for LABS
//
// Provides serialization for all LABS data types:
// - geos::Data (loss metrics)
// - 3D LUT (17³ color transform)
// - pipe::Link (edit steps)
// - geos::Result (optimization results) - future
//
// See data.md for format specification.

#pragma once

#include <geos.hpp>
#include <pipe.hpp>
#include <string>
#include <vector>

namespace data
{
    // ============================================================
    // Hex Encoding/Decoding (for LUT storage - compact uint16)
    // ============================================================

    namespace hex
    {
        // Encode uint16 array to hex string (4 chars per value)
        std::string encode(const uint16_t* data, size_t count);

        // Decode hex string to uint16 array
        std::vector<uint16_t> decode(const std::string& encoded);

    } // namespace hex

    // ============================================================
    // 3D LUT Serialization
    // ============================================================

    namespace lut
    {
        // Serialize 3D LUT to JSON object string
        // Format: {"grid": 17, "data": "hex..."} - uint16 per channel
        std::string toJson(const float* lut, int gridSize);

        // Deserialize 3D LUT from JSON object string
        // Returns empty vector if parsing fails
        std::vector<float> fromJson(const std::string& json, int& gridSize);

        // Save LUT to file
        bool save(const float* lut, int gridSize, const std::string& path);

        // Load LUT from file (returns empty vector on failure)
        std::vector<float> load(const std::string& path, int& gridSize);

    } // namespace lut

    // ============================================================
    // Geos Data (Loss Metrics)
    // ============================================================

    namespace geos
    {
        // Serialize to JSON string
        std::string toJson(const ::geos::Data& d);

        // Deserialize from JSON string
        ::geos::Data fromJson(const std::string& json);

        // Save to file
        bool save(const ::geos::Data& d, const std::string& path);

        // Load from file
        ::geos::Data load(const std::string& path);

    } // namespace geos

    // ============================================================
    // Link Serialization (Edit Steps)
    // ============================================================

    namespace link
    {
        // Serialize link to JSON string
        std::string toJson(pipe::Body::Link& link);

        // Deserialize JSON string into link (modifies link in-place)
        bool fromJson(pipe::Body::Link& link, const std::string& json);

        // Save link to file
        bool save(pipe::Body::Link& link, const std::string& path);

        // Load link from file (modifies link in-place)
        bool load(pipe::Body::Link& link, const std::string& path);

    } // namespace link

} // namespace data
