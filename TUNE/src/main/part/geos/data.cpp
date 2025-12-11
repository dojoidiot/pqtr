// data.cpp
// Serialization for geos data types and 3D LUT

#include <data.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>

// ============================================================
// Hex Encoding/Decoding (uint16 values)
// ============================================================

namespace data::hex
{

static const char HEX_CHARS[] = "0123456789abcdef";

std::string encode(const uint16_t* data, size_t count)
{
    std::string result;
    result.reserve(count * 4);

    for (size_t i = 0; i < count; i++)
    {
        uint16_t v = data[i];
        result += HEX_CHARS[(v >> 12) & 0xF];
        result += HEX_CHARS[(v >> 8) & 0xF];
        result += HEX_CHARS[(v >> 4) & 0xF];
        result += HEX_CHARS[v & 0xF];
    }

    return result;
}

static inline int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::vector<uint16_t> decode(const std::string& encoded)
{
    std::vector<uint16_t> result;
    result.reserve(encoded.size() / 4);

    for (size_t i = 0; i + 3 < encoded.size(); i += 4)
    {
        int h0 = hexVal(encoded[i]);
        int h1 = hexVal(encoded[i + 1]);
        int h2 = hexVal(encoded[i + 2]);
        int h3 = hexVal(encoded[i + 3]);

        if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) continue;

        uint16_t v = (h0 << 12) | (h1 << 8) | (h2 << 4) | h3;
        result.push_back(v);
    }

    return result;
}

} // namespace data::hex

// ============================================================
// 3D LUT Serialization
// ============================================================

namespace data::lut
{

std::string toJson(const float* lut, int gridSize)
{
    size_t numFloats = gridSize * gridSize * gridSize * 3;

    // Convert float [0,1] to uint16 [0,65535]
    std::vector<uint16_t> u16(numFloats);
    for (size_t i = 0; i < numFloats; i++)
    {
        float v = std::max(0.0f, std::min(1.0f, lut[i]));
        u16[i] = static_cast<uint16_t>(v * 65535.0f + 0.5f);
    }

    std::string hexData = hex::encode(u16.data(), numFloats);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"grid\": " << gridSize << ",\n";
    ss << "  \"data\": \"" << hexData << "\"\n";
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

    // Parse hex data
    pos = json.find("\"data\"");
    if (pos == std::string::npos)
        return {};
    pos = json.find('"', pos + 6);
    if (pos == std::string::npos)
        return {};
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return {};

    std::string hexData = json.substr(pos + 1, end - pos - 1);
    std::vector<uint16_t> u16 = hex::decode(hexData);

    // Convert uint16 [0,65535] back to float [0,1]
    std::vector<float> result(u16.size());
    for (size_t i = 0; i < u16.size(); i++)
    {
        result[i] = static_cast<float>(u16[i]) / 65535.0f;
    }

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
// HSV LUT Serialization
// ============================================================

namespace data::hsvlut
{

std::string toJson(const float* lut, int hBins, int sBins)
{
    size_t numFloats = hBins * sBins * 3;

    // HSV deltas can be negative (e.g., -30° hue shift)
    // Store as signed int16 [-32768, 32767] mapped from [-1, 1]
    std::vector<uint16_t> u16(numFloats);
    for (size_t i = 0; i < numFloats; i++)
    {
        // Clamp to [-1, 1] and convert to uint16 (0 = -1, 32768 = 0, 65535 = +1)
        float v = std::max(-1.0f, std::min(1.0f, lut[i]));
        u16[i] = static_cast<uint16_t>((v + 1.0f) * 32767.5f);
    }

    std::string hexData = hex::encode(u16.data(), numFloats);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"h_bins\": " << hBins << ",\n";
    ss << "  \"s_bins\": " << sBins << ",\n";
    ss << "  \"data\": \"" << hexData << "\"\n";
    ss << "}";
    return ss.str();
}

std::vector<float> fromJson(const std::string& json, int& hBins, int& sBins)
{
    // Parse h_bins
    size_t pos = json.find("\"h_bins\"");
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos)
        return {};
    hBins = std::stoi(json.substr(pos + 1));

    // Parse s_bins
    pos = json.find("\"s_bins\"");
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos);
    if (pos == std::string::npos)
        return {};
    sBins = std::stoi(json.substr(pos + 1));

    // Parse hex data
    pos = json.find("\"data\"");
    if (pos == std::string::npos)
        return {};
    pos = json.find('"', pos + 6);
    if (pos == std::string::npos)
        return {};
    size_t end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return {};

    std::string hexData = json.substr(pos + 1, end - pos - 1);
    std::vector<uint16_t> u16 = hex::decode(hexData);

    // Convert uint16 back to float [-1, 1]
    std::vector<float> result(u16.size());
    for (size_t i = 0; i < u16.size(); i++)
    {
        result[i] = (static_cast<float>(u16[i]) / 32767.5f) - 1.0f;
    }

    return result;
}

} // namespace data::hsvlut

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

    // Polynomial coefficients (Camera Math) - if active
    if (link.polyColor().isActive())
    {
        ss << "  \"poly_coeffs\": [";
        const float* coeffs = link.polyColor().coeffs();
        for (int i = 0; i < 30; i++)
        {
            if (i > 0) ss << ", ";
            ss << std::setprecision(6) << coeffs[i];
        }
        ss << "],\n";
        ss << std::setprecision(4);
    }

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
    ss << "    }";

    // 3D LUT (only if estimated)
    if (link.lutCurve().isEstimated())
    {
        ss << ",\n";
        ss << "    \"lut\": " << lut::toJson(link.lutCurve().lut(), link.lutCurve().GRID_SIZE);
    }

    // HSV LUT (only if estimated)
    if (link.hsvLut().isEstimated())
    {
        ss << ",\n";
        ss << "    \"hsv_lut\": " << hsvlut::toJson(link.hsvLut().lut(), link.hsvLut().H_BINS, link.hsvLut().S_BINS);
    }

    ss << "\n  }\n";
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
    // Polynomial coefficients (Camera Math) - if present
    size_t polyPos = json.find("\"poly_coeffs\"");
    if (polyPos != std::string::npos)
    {
        size_t arrStart = json.find('[', polyPos);
        size_t arrEnd = json.find(']', arrStart);
        if (arrStart != std::string::npos && arrEnd != std::string::npos)
        {
            std::string arrStr = json.substr(arrStart + 1, arrEnd - arrStart - 1);
            std::istringstream iss(arrStr);
            float coeffs[30];
            int count = 0;
            std::string token;
            while (std::getline(iss, token, ',') && count < 30)
            {
                coeffs[count++] = std::stof(token);
            }
            if (count == 30)
            {
                link.polyColor().setCoeffs(coeffs);
            }
        }
    }

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

    // 3D LUT (if present)
    size_t lutPos = json.find("\"lut\"");
    if (lutPos != std::string::npos)
    {
        // Find the LUT object - it starts at the next {
        size_t lutStart = json.find('{', lutPos);
        if (lutStart != std::string::npos)
        {
            // Find matching closing brace
            int braceCount = 1;
            size_t lutEnd = lutStart + 1;
            while (lutEnd < json.size() && braceCount > 0)
            {
                if (json[lutEnd] == '{') braceCount++;
                else if (json[lutEnd] == '}') braceCount--;
                lutEnd++;
            }

            std::string lutJson = json.substr(lutStart, lutEnd - lutStart);
            int gridSize = 0;
            std::vector<float> lutData = lut::fromJson(lutJson, gridSize);

            if (!lutData.empty() && gridSize == link.lutCurve().GRID_SIZE)
            {
                link.lutCurve().setLut(lutData.data());
            }
        }
    }

    // HSV LUT (if present)
    size_t hsvLutPos = json.find("\"hsv_lut\"");
    if (hsvLutPos != std::string::npos)
    {
        // Find the HSV LUT object - it starts at the next {
        size_t hsvLutStart = json.find('{', hsvLutPos);
        if (hsvLutStart != std::string::npos)
        {
            // Find matching closing brace
            int braceCount = 1;
            size_t hsvLutEnd = hsvLutStart + 1;
            while (hsvLutEnd < json.size() && braceCount > 0)
            {
                if (json[hsvLutEnd] == '{') braceCount++;
                else if (json[hsvLutEnd] == '}') braceCount--;
                hsvLutEnd++;
            }

            std::string hsvLutJson = json.substr(hsvLutStart, hsvLutEnd - hsvLutStart);
            int hBins = 0, sBins = 0;
            std::vector<float> hsvLutData = hsvlut::fromJson(hsvLutJson, hBins, sBins);

            if (!hsvLutData.empty() && hBins == link.hsvLut().H_BINS && sBins == link.hsvLut().S_BINS)
            {
                link.hsvLut().setLut(hsvLutData.data());
            }
        }
    }

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

// ============================================================
// Info Serialization (Camera Metadata)
// ============================================================

namespace data::info
{

std::string toJson(const pipe::InfoMap& info)
{
    std::ostringstream ss;
    ss << "{\n";

    bool first = true;
    for (const auto& [key, value] : info)
    {
        if (!first) ss << ",\n";
        first = false;

        // Escape any quotes in the value
        std::string escaped;
        for (char c : value)
        {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else escaped += c;
        }

        ss << "  \"" << key << "\": \"" << escaped << "\"";
    }

    ss << "\n}\n";
    return ss.str();
}

bool save(const pipe::InfoMap& info, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << toJson(info);
    file.close();
    return true;
}

} // namespace data::info

// ============================================================
// Links Serialization (Multiple Edit Steps)
// ============================================================

namespace data::links
{

std::string toJson(std::vector<pipe::Body::Link*>& linkPtrs)
{
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"links\": [\n";

    for (size_t i = 0; i < linkPtrs.size(); i++)
    {
        // Get the single-link JSON and indent it
        std::string linkJson = link::toJson(*linkPtrs[i]);

        // Indent each line by 4 spaces
        std::istringstream iss(linkJson);
        std::string line;
        bool firstLine = true;
        while (std::getline(iss, line))
        {
            if (!firstLine) ss << "\n";
            firstLine = false;
            ss << "    " << line;
        }

        if (i < linkPtrs.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

bool fromJson(std::vector<pipe::Body::Link*>& linkPtrs, const std::string& json)
{
    // Find "links" array
    size_t pos = json.find("\"links\"");
    if (pos == std::string::npos) return false;

    pos = json.find('[', pos);
    if (pos == std::string::npos) return false;

    // Parse each link object in the array
    size_t linkIdx = 0;
    while (linkIdx < linkPtrs.size())
    {
        // Find next object start
        size_t objStart = json.find('{', pos + 1);
        if (objStart == std::string::npos) break;

        // Find matching closing brace (handle nested braces)
        int braceCount = 1;
        size_t objEnd = objStart + 1;
        while (objEnd < json.size() && braceCount > 0)
        {
            if (json[objEnd] == '{') braceCount++;
            else if (json[objEnd] == '}') braceCount--;
            objEnd++;
        }

        std::string linkJson = json.substr(objStart, objEnd - objStart);
        link::fromJson(*linkPtrs[linkIdx], linkJson);

        pos = objEnd;
        linkIdx++;
    }

    return linkIdx == linkPtrs.size();
}

bool save(std::vector<pipe::Body::Link*>& linkPtrs, const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
        return false;
    file << toJson(linkPtrs);
    file.close();
    return true;
}

bool load(std::vector<pipe::Body::Link*>& linkPtrs, const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return fromJson(linkPtrs, buffer.str());
}

} // namespace data::links
