// data.hpp
// Data persistence API for LABS
//
// Provides serialization for all LABS data types:
// - tune::Data (loss metrics)
// - 3D LUT (17³ color transform)
// - pipe::Link (edit steps / dial values)
//
// See data.md for format specification.

#pragma once

#include <tune.hpp>
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

    // ============================================================
    // Link Serialization (Edit Steps)
    // ============================================================

    namespace link
    {
        // Serialize a Link to JSON string
        // Only saves dials that have been explicitly set
        std::string toJson(pipe::Body::Link& link);

        // Apply JSON settings to a Link
        // Link must already exist in body; this sets dial values
        bool fromJson(pipe::Body::Link& link, const std::string& json);

        // Save Link to file
        bool save(pipe::Body::Link& link, const std::string& path);

        // Load Link settings from file and apply to existing Link
        bool load(pipe::Body::Link& link, const std::string& path);

    } // namespace link

} // namespace data
