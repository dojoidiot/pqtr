// data.cpp
// Serialization for tune data types and 3D LUT

#include <data.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

// ============================================================
// Base64 Encoding/Decoding
// ============================================================

namespace data::base64
{

static const char* CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const uint8_t DECODE_TABLE[128] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

std::string encode(const void* data, size_t size)
{
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::string result;
    result.reserve((size + 2) / 3 * 4);

    for (size_t i = 0; i < size; i += 3)
    {
        uint32_t n = static_cast<uint32_t>(bytes[i]) << 16;
        if (i + 1 < size) n |= static_cast<uint32_t>(bytes[i + 1]) << 8;
        if (i + 2 < size) n |= static_cast<uint32_t>(bytes[i + 2]);

        result += CHARS[(n >> 18) & 0x3F];
        result += CHARS[(n >> 12) & 0x3F];
        result += (i + 1 < size) ? CHARS[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < size) ? CHARS[n & 0x3F] : '=';
    }

    return result;
}

std::vector<uint8_t> decode(const std::string& encoded)
{
    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);

    uint32_t n = 0;
    int bits = 0;

    for (char c : encoded)
    {
        if (c == '=') break;
        if (c < 0 || static_cast<unsigned char>(c) >= 128) continue;
        uint8_t d = DECODE_TABLE[static_cast<unsigned char>(c)];
        if (d == 64) continue;

        n = (n << 6) | d;
        bits += 6;

        if (bits >= 8)
        {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((n >> bits) & 0xFF));
        }
    }

    return result;
}

} // namespace data::base64

// ============================================================
// 3D LUT Serialization
// ============================================================

namespace data::lut
{

std::string toJson(const float* lut, int gridSize)
{
    size_t numFloats = gridSize * gridSize * gridSize * 3;
    size_t numBytes = numFloats * sizeof(float);

    std::string b64 = base64::encode(lut, numBytes);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"grid\": " << gridSize << ",\n";
    ss << "  \"data\": \"" << b64 << "\"\n";
    ss << "}";
    return ss.str();
}

std::vector<float> fromJson(const std::string& json, int& gridSize)
{
    // Parse grid size
    size_t pos = json.find("\"grid\"");
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos)
        return {};
    gridSize = std::stoi(json.substr(pos + 1));

    // Parse base64 data
    pos = json.find("\"data\"");
    if (pos == std::string::npos)
        return {};
    pos = json.find('"', pos + 6);
    if (pos == std::string::npos)
        return {};
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return {};

    std::string b64 = json.substr(pos + 1, end - pos - 1);
    std::vector<uint8_t> bytes = base64::decode(b64);

    // Convert to floats
    size_t numFloats = bytes.size() / sizeof(float);
    std::vector<float> result(numFloats);
    std::memcpy(result.data(), bytes.data(), bytes.size());

    return result;
}

bool save(const float* lut, int gridSize, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << toJson(lut, gridSize);
    file.close();
    return true;
}

std::vector<float> load(const std::string& path, int& gridSize)
{
    std::ifstream file(path);
    if (!file.is_open())
        return {};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromJson(buffer.str(), gridSize);
}

} // namespace data::lut

// ============================================================
// Tune Data (Loss Metrics)
// ============================================================

namespace data::tune
{

std::string toJson(const ::tune::Data& d)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"spectral\": " << d.spectral << ",\n";
    ss << "  \"frequency\": " << d.frequency << "\n";
    ss << "}";
    return ss.str();
}

::tune::Data fromJson(const std::string& json)
{
    ::tune::Data d;
    size_t pos = json.find("\"spectral\"");
    if (pos != std::string::npos)
    {
        pos = json.find(':', pos);
        if (pos != std::string::npos)
            d.spectral = std::stof(json.substr(pos + 1));
    }
    pos = json.find("\"frequency\"");
    if (pos != std::string::npos)
    {
        pos = json.find(':', pos);
        if (pos != std::string::npos)
            d.frequency = std::stof(json.substr(pos + 1));
    }
    return d;
}

bool save(const ::tune::Data& d, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << toJson(d);
    file.close();
    return true;
}

::tune::Data load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return ::tune::Data{};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromJson(buffer.str());
}

} // namespace data::tune
