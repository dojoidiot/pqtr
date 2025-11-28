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
    // Base64 Encoding/Decoding (for LUT storage)
    // ============================================================

    namespace base64
    {
        // Encode binary data to base64 string
        std::string encode(const void* data, size_t size);

        // Decode base64 string to binary data
        std::vector<uint8_t> decode(const std::string& encoded);

    } // namespace base64

    // ============================================================
    // 3D LUT Serialization
    // ============================================================

    namespace lut
    {
        // Serialize 3D LUT to JSON object string
        // Format: {"grid": 17, "data": "base64..."}
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
