// data.cpp
// Serialization for geos data types and 3D LUT

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
// Geos Data (Loss Metrics)
// ============================================================

namespace data::geos
{

std::string toJson(const ::geos::Data& d)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "{\n";
    ss << "  \"spectral\": " << d.spectral << ",\n";
    ss << "  \"frequency\": " << d.frequency << "\n";
    ss << "}";
    return ss.str();
}

::geos::Data fromJson(const std::string& json)
{
    ::geos::Data d;
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

bool save(const ::geos::Data& d, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << toJson(d);
    file.close();
    return true;
}

::geos::Data load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return ::geos::Data{};
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromJson(buffer.str());
}

} // namespace data::geos

// ============================================================
// Link Serialization (Edit Steps)
// ============================================================

namespace data::link
{

std::string toJson(pipe::Body::Link& link)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << "{\n";
    ss << "  \"name\": \"" << link.name() << "\",\n";
    ss << "  \"modules\": {\n";

    // Color Correction
    ss << "    \"color_correction\": {\n";
    ss << "      \"exposure\": " << link.colorCorrection().exposure().get() << ",\n";
    ss << "      \"temperature\": " << link.colorCorrection().whiteBalance().temperature() << ",\n";
    ss << "      \"tint\": " << link.colorCorrection().whiteBalance().tint() << "\n";
    ss << "    },\n";

    // Tone Mapping
    ss << "    \"tone_mapping\": {\n";
    ss << "      \"contrast\": " << link.toneMapping().contrast().get() << ",\n";
    ss << "      \"highlights\": " << link.toneMapping().curveAdjustment().highlights().get() << ",\n";
    ss << "      \"shadows\": " << link.toneMapping().curveAdjustment().shadows().get() << ",\n";
    ss << "      \"toe_pivot\": " << link.toneMapping().curveAdjustment().toePivot().get() << ",\n";
    ss << "      \"shoulder_pivot\": " << link.toneMapping().curveAdjustment().shoulderPivot().get() << ",\n";
    ss << "      \"white_point\": " << link.toneMapping().clippingPoint().white().get() << ",\n";
    ss << "      \"black_point\": " << link.toneMapping().clippingPoint().black().get() << "\n";
    ss << "    },\n";

    // Global Color
    ss << "    \"global_color\": {\n";
    ss << "      \"vibrance\": " << link.globalColor().vibrance().get() << ",\n";
    ss << "      \"saturation\": " << link.globalColor().saturation().get() << ",\n";
    ss << "      \"density\": " << link.globalColor().colourDensity().get() << "\n";
    ss << "    },\n";

    // Split Tone
    ss << "    \"split_tone\": {\n";
    ss << "      \"shadow_temp\": " << link.splitTone().shadows().temperature() << ",\n";
    ss << "      \"shadow_tint\": " << link.splitTone().shadows().tint() << ",\n";
    ss << "      \"highlight_temp\": " << link.splitTone().highlights().temperature() << ",\n";
    ss << "      \"highlight_tint\": " << link.splitTone().highlights().tint() << "\n";
    ss << "    },\n";

    // Selective Colour (8 bands × 3 dials each)
    ss << "    \"selective_color\": {\n";
    const char* bands[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
    pipe::Body::Link::SelectiveColour::HslAdjust* hsl[] = {
        &link.selectiveColour().red(), &link.selectiveColour().orange(),
        &link.selectiveColour().yellow(), &link.selectiveColour().green(),
        &link.selectiveColour().cyan(), &link.selectiveColour().blue(),
        &link.selectiveColour().purple(), &link.selectiveColour().magenta()
    };
    for (int i = 0; i < 8; i++) {
        ss << "      \"" << bands[i] << "\": {"
           << " \"hue\": " << hsl[i]->hue()
           << ", \"sat\": " << hsl[i]->saturation()
           << ", \"lum\": " << hsl[i]->luminance()
           << " }";
        if (i < 7) ss << ",";
        ss << "\n";
    }
    ss << "    },\n";

    // Detail
    ss << "    \"detail\": {\n";
    ss << "      \"sharpen_amount\": " << link.detail().sharpen().amount() << ",\n";
    ss << "      \"sharpen_radius\": " << link.detail().sharpen().radius() << ",\n";
    ss << "      \"denoise_luma\": " << link.detail().denoise().luminance().get() << ",\n";
    ss << "      \"denoise_chroma\": " << link.detail().denoise().chroma().get() << "\n";
    ss << "    }\n";

    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

// Simple JSON value parser helper
static float parseFloat(const std::string& json, const std::string& key, float defaultVal = 0.5f)
{
    size_t pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return defaultVal;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return defaultVal;
    return std::stof(json.substr(pos + 1));
}

bool fromJson(pipe::Body::Link& link, const std::string& json)
{
    // Color Correction
    link.colorCorrection().exposure().set(parseFloat(json, "exposure"));
    link.colorCorrection().whiteBalance().temperature(parseFloat(json, "temperature"));
    link.colorCorrection().whiteBalance().tint(parseFloat(json, "tint"));

    // Tone Mapping
    link.toneMapping().contrast().set(parseFloat(json, "contrast"));
    link.toneMapping().curveAdjustment().highlights().set(parseFloat(json, "highlights"));
    link.toneMapping().curveAdjustment().shadows().set(parseFloat(json, "shadows"));
    link.toneMapping().curveAdjustment().toePivot().set(parseFloat(json, "toe_pivot"));
    link.toneMapping().curveAdjustment().shoulderPivot().set(parseFloat(json, "shoulder_pivot"));
    link.toneMapping().clippingPoint().white().set(parseFloat(json, "white_point"));
    link.toneMapping().clippingPoint().black().set(parseFloat(json, "black_point"));

    // Global Color
    link.globalColor().vibrance().set(parseFloat(json, "vibrance"));
    link.globalColor().saturation().set(parseFloat(json, "saturation"));
    link.globalColor().colourDensity().set(parseFloat(json, "density"));

    // Split Tone
    link.splitTone().shadows().temperature(parseFloat(json, "shadow_temp"));
    link.splitTone().shadows().tint(parseFloat(json, "shadow_tint"));
    link.splitTone().highlights().temperature(parseFloat(json, "highlight_temp"));
    link.splitTone().highlights().tint(parseFloat(json, "highlight_tint"));

    // Selective Color - parse each band
    // Find each band's sub-object and parse its hue/sat/lum
    auto parseHsl = [&json](const std::string& band, float& h, float& s, float& l) {
        size_t pos = json.find("\"" + band + "\"");
        if (pos == std::string::npos) return;
        size_t end = json.find("}", pos);
        std::string sub = json.substr(pos, end - pos);
        h = parseFloat(sub, "hue");
        s = parseFloat(sub, "sat");
        l = parseFloat(sub, "lum");
    };

    float h, s, l;
    parseHsl("red", h, s, l);
    link.selectiveColour().red().hue(h);
    link.selectiveColour().red().saturation(s);
    link.selectiveColour().red().luminance(l);

    parseHsl("orange", h, s, l);
    link.selectiveColour().orange().hue(h);
    link.selectiveColour().orange().saturation(s);
    link.selectiveColour().orange().luminance(l);

    parseHsl("yellow", h, s, l);
    link.selectiveColour().yellow().hue(h);
    link.selectiveColour().yellow().saturation(s);
    link.selectiveColour().yellow().luminance(l);

    parseHsl("green", h, s, l);
    link.selectiveColour().green().hue(h);
    link.selectiveColour().green().saturation(s);
    link.selectiveColour().green().luminance(l);

    parseHsl("cyan", h, s, l);
    link.selectiveColour().cyan().hue(h);
    link.selectiveColour().cyan().saturation(s);
    link.selectiveColour().cyan().luminance(l);

    parseHsl("blue", h, s, l);
    link.selectiveColour().blue().hue(h);
    link.selectiveColour().blue().saturation(s);
    link.selectiveColour().blue().luminance(l);

    parseHsl("purple", h, s, l);
    link.selectiveColour().purple().hue(h);
    link.selectiveColour().purple().saturation(s);
    link.selectiveColour().purple().luminance(l);

    parseHsl("magenta", h, s, l);
    link.selectiveColour().magenta().hue(h);
    link.selectiveColour().magenta().saturation(s);
    link.selectiveColour().magenta().luminance(l);

    // Detail
    link.detail().sharpen().amount(parseFloat(json, "sharpen_amount", 0.0f));
    link.detail().sharpen().radius(parseFloat(json, "sharpen_radius", 0.4f));
    link.detail().denoise().luminance().set(parseFloat(json, "denoise_luma", 0.0f));
    link.detail().denoise().chroma().set(parseFloat(json, "denoise_chroma", 0.0f));

    return true;
}

bool save(pipe::Body::Link& link, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << toJson(link);
    file.close();
    return true;
}

bool load(pipe::Body::Link& link, const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromJson(link, buffer.str());
}

} // namespace data::link
