// copy.cpp - XMP sidecar parser and Vibe mapping
//
// Parses Darktable XMP files and maps settings to vibe.hpp structure in flow::Tree
//
// Output format matches vibe.hpp API:
//   vibe.linear.colorCorrection.exposure
//   vibe.linear.toneMapping.contrast
//   etc.

#include "flow.hpp"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <zlib.h>

namespace flow
{

// ============================================================================
// Hex decoder
// ============================================================================

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static std::vector<uint8_t> decode_hex(const std::string &hex)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i + 1 < hex.size(); i += 2)
    {
        int hi = hex_digit(hex[i]);
        int lo = hex_digit(hex[i + 1]);
        if (hi >= 0 && lo >= 0)
            bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

static float read_float_le(const uint8_t *p)
{
    uint32_t v = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

static int32_t read_int32_le(const uint8_t *p)
{
    return static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// ============================================================================
// Base64 + zlib decompression for darktable compressed params
// ============================================================================

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64_index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

static std::vector<uint8_t> decode_base64(const std::string &b64)
{
    std::vector<uint8_t> out;
    out.reserve(b64.size() * 3 / 4);

    int val = 0, bits = 0;
    for (char c : b64)
    {
        if (c == '=') break;
        int idx = b64_index(c);
        if (idx < 0) continue;
        val = (val << 6) | idx;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
        }
    }
    return out;
}

static std::vector<uint8_t> decompress_gz(const std::string &params)
{
    // Format: "gzNN<base64>" where NN is version
    if (params.size() < 4 || params[0] != 'g' || params[1] != 'z')
        return {};

    // Skip "gzNN" prefix (4 chars)
    std::string b64 = params.substr(4);
    auto compressed = decode_base64(b64);
    if (compressed.empty())
        return {};

    // Decompress with zlib
    std::vector<uint8_t> out(4096);
    z_stream strm = {};
    strm.next_in = compressed.data();
    strm.avail_in = static_cast<uInt>(compressed.size());
    strm.next_out = out.data();
    strm.avail_out = static_cast<uInt>(out.size());

    // Try zlib format first (darktable uses zlib header)
    if (inflateInit(&strm) != Z_OK)
        return {};

    int ret = inflate(&strm, Z_FINISH);
    size_t outSize = out.size() - strm.avail_out;
    inflateEnd(&strm);

    if (ret != Z_STREAM_END && ret != Z_OK)
        return {};

    out.resize(outSize);
    return out;
}

// ============================================================================
// Simple XML attribute parser
// ============================================================================

static std::string get_attr(const std::string &tag, const std::string &attr)
{
    std::string search = attr + "=\"";
    size_t pos = tag.find(search);
    if (pos == std::string::npos)
        return "";

    pos += search.size();
    size_t end = tag.find('"', pos);
    if (end == std::string::npos)
        return "";

    return tag.substr(pos, end - pos);
}

// ============================================================================
// Darktable module param decoders
// ============================================================================

// exposure module (modversion 7):
//   float mode, float black, float exposure, ...
struct ExposureParams
{
    float exposure = 0.0f;  // EV
    bool valid = false;
};

static ExposureParams decode_exposure(const std::string &hex)
{
    ExposureParams p;
    auto bytes = decode_hex(hex);
    if (bytes.size() >= 12)
    {
        p.exposure = read_float_le(&bytes[8]);  // offset 8 = exposure EV
        p.valid = true;
    }
    return p;
}

// temperature module (modversion 4):
//   float coeffs[4], int preset
struct TemperatureParams
{
    float temperature = 5500.0f;  // Kelvin (derived)
    float tint = 1.0f;
    bool valid = false;
};

static TemperatureParams decode_temperature(const std::string &hex)
{
    TemperatureParams p;
    auto bytes = decode_hex(hex);
    if (bytes.size() >= 16)
    {
        float r = read_float_le(&bytes[0]);
        float g = read_float_le(&bytes[4]);
        float b = read_float_le(&bytes[8]);
        if (b > 0.001f && g > 0.001f)
        {
            p.temperature = 5500.0f * (r / b);
            p.tint = g;
        }
        p.valid = true;
    }
    return p;
}

// sigmoid module (modversion 3):
//   float contrast, float skew, ...
struct SigmoidParams
{
    float contrast = 1.5f;
    float skew = 0.0f;
    bool valid = false;
};

static SigmoidParams decode_sigmoid(const std::string &hex)
{
    SigmoidParams p;
    auto bytes = decode_hex(hex);
    if (bytes.size() >= 8)
    {
        p.contrast = read_float_le(&bytes[0]);
        p.skew = read_float_le(&bytes[4]);
        p.valid = true;
    }
    return p;
}

// filmicrgb module (modversion 6) - compressed
// Struct layout from darktable/src/iop/filmicrgb.c
struct FilmicParams
{
    float greyPoint = 0.1845f;    // grey_point_source (18.45%)
    float blackPoint = -8.0f;     // black_point_source (EV below grey)
    float whitePoint = 4.0f;      // white_point_source (EV above grey)
    float contrast = 1.5f;        // contrast
    float latitude = 0.01f;       // latitude
    float balance = 0.0f;         // balance (shadows/highlights)
    float saturation = 0.0f;      // saturation
    bool valid = false;
};

static FilmicParams decode_filmic(const std::string &params)
{
    FilmicParams p;
    auto bytes = decompress_gz(params);
    if (bytes.size() >= 48)  // Need at least 12 floats
    {
        p.greyPoint = read_float_le(&bytes[0]);      // grey_point_source
        p.blackPoint = read_float_le(&bytes[4]);     // black_point_source
        p.whitePoint = read_float_le(&bytes[8]);     // white_point_source
        // bytes[12..15] = security_factor
        // bytes[16..19] = grey_point_target
        // bytes[20..23] = black_point_target
        // bytes[24..27] = white_point_target
        // bytes[28..31] = output_power
        p.latitude = read_float_le(&bytes[32]);      // latitude
        p.contrast = read_float_le(&bytes[36]);      // contrast
        p.saturation = read_float_le(&bytes[40]);    // saturation
        p.balance = read_float_le(&bytes[44]);       // balance
        p.valid = true;
    }
    return p;
}

// colorbalancergb module (modversion 5) - compressed
// Struct layout from darktable/src/iop/colorbalancergb.c (132 bytes)
//
// v1 params (0-95):
//   0: shadows_Y, 4: shadows_C, 8: shadows_H
//  12: midtones_Y, 16: midtones_C, 20: midtones_H
//  24: highlights_Y, 28: highlights_C, 32: highlights_H
//  36: global_Y, 40: global_C, 44: global_H
//  48: shadows_weight, 52: white_fulcrum, 56: highlights_weight
//  60: chroma_shadows, 64: chroma_highlights, 68: chroma_global, 72: chroma_midtones
//  76: saturation_global, 80: saturation_highlights, 84: saturation_midtones, 88: saturation_shadows
//  92: hue_angle
// v2 params (96-111):
//  96: brilliance_global, 100: brilliance_highlights, 104: brilliance_midtones, 108: brilliance_shadows
// v3 params (112-115):
// 112: mask_grey_fulcrum
// v4 params (116-127):
// 116: vibrance, 120: grey_fulcrum, 124: contrast
// v5 params (128-131):
// 128: saturation_formula (int)
struct ColorBalanceParams
{
    float shadowsC = 0.0f;      // chroma
    float shadowsH = 0.0f;      // hue (degrees)
    float highlightsC = 0.0f;
    float highlightsH = 0.0f;
    float globalC = 0.0f;
    float globalH = 0.0f;
    float saturation = 0.0f;    // saturation_global at offset 76
    float vibrance = 0.0f;      // vibrance at offset 116 (v4+)
    float contrast = 0.0f;      // contrast at offset 124 (v4+)
    bool valid = false;
};

static ColorBalanceParams decode_colorbalance(const std::string &params)
{
    ColorBalanceParams p;
    auto bytes = decompress_gz(params);
    if (bytes.size() >= 128)  // Need v4 params (vibrance, contrast)
    {
        // v1 params
        p.shadowsC = read_float_le(&bytes[4]);      // shadows_C
        p.shadowsH = read_float_le(&bytes[8]);      // shadows_H (degrees)
        p.highlightsC = read_float_le(&bytes[28]);  // highlights_C
        p.highlightsH = read_float_le(&bytes[32]);  // highlights_H (degrees)
        p.globalC = read_float_le(&bytes[40]);      // global_C
        p.globalH = read_float_le(&bytes[44]);      // global_H (degrees)
        p.saturation = read_float_le(&bytes[76]);   // saturation_global
        // v4 params
        p.vibrance = read_float_le(&bytes[116]);    // vibrance
        p.contrast = read_float_le(&bytes[124]);    // contrast
        p.valid = true;
    }
    return p;
}

// bilat module (modversion 3):
//   int mode, float sigma_r, float sigma_s, float detail, float midtone
struct BilatParams
{
    int mode = 0;           // 0=local contrast, 1=bilateral
    float sigma_r = 0.5f;   // range control
    float sigma_s = 0.5f;   // spatial control (radius)
    float detail = 0.25f;   // local contrast amount
    float midtone = 0.5f;
    bool valid = false;
};

static BilatParams decode_bilat(const std::string &hex)
{
    BilatParams p;
    auto bytes = decode_hex(hex);
    if (bytes.size() >= 20)
    {
        p.mode = read_int32_le(&bytes[0]);
        p.sigma_r = read_float_le(&bytes[4]);
        p.sigma_s = read_float_le(&bytes[8]);
        p.detail = read_float_le(&bytes[12]);
        p.midtone = read_float_le(&bytes[16]);
        p.valid = true;
    }
    return p;
}

// ============================================================================
// XMP Parser
// ============================================================================

struct DarktableModule
{
    std::string operation;
    bool enabled = false;
    int version = 0;
    std::string params;
    std::string name;
};

static std::vector<DarktableModule> parse_xmp(const char *xmp, size_t size)
{
    std::vector<DarktableModule> modules;
    std::string xml(xmp, size);

    size_t pos = 0;
    while ((pos = xml.find("<rdf:li", pos)) != std::string::npos)
    {
        size_t end = xml.find("/>", pos);
        if (end == std::string::npos)
            break;

        std::string tag = xml.substr(pos, end - pos + 2);

        if (tag.find("darktable:operation=") != std::string::npos)
        {
            DarktableModule mod;
            mod.operation = get_attr(tag, "darktable:operation");
            mod.enabled = get_attr(tag, "darktable:enabled") == "1";
            mod.version = std::atoi(get_attr(tag, "darktable:modversion").c_str());
            mod.params = get_attr(tag, "darktable:params");
            mod.name = get_attr(tag, "darktable:multi_name");
            modules.push_back(mod);
        }

        pos = end + 2;
    }

    return modules;
}

// ============================================================================
// Map darktable modules to vibe.hpp structure
// ============================================================================

static void mapToVibe(const std::vector<DarktableModule> &modules, Stem &vibe)
{
    // Get vibe.linear node (matches vibe::Vibe::Linear)
    auto &linear = vibe.next("linear");

    // Accumulators for multi-instance modules
    float totalExposure = 0.0f;
    bool hasExposure = false;

    for (const auto &mod : modules)
    {
        if (!mod.enabled)
            continue;

        // ----------------------------------------------------------------
        // ColorCorrection: exposure, temperature
        // ----------------------------------------------------------------
        if (mod.operation == "exposure")
        {
            auto p = decode_exposure(mod.params);
            if (p.valid)
            {
                totalExposure += p.exposure;  // Sum multiple exposure modules
                hasExposure = true;
            }
        }
        else if (mod.operation == "temperature")
        {
            auto p = decode_temperature(mod.params);
            if (p.valid)
            {
                auto &cc = linear.next("colorCorrection");
                auto &wb = cc.next("whiteBalance");
                wb.leaf("temperature").dial(p.temperature);
                wb.leaf("tint").dial(p.tint);
            }
        }
        // ----------------------------------------------------------------
        // ToneMapping: sigmoid, filmicrgb
        // ----------------------------------------------------------------
        else if (mod.operation == "sigmoid")
        {
            auto p = decode_sigmoid(mod.params);
            if (p.valid)
            {
                auto &tm = linear.next("toneMapping");
                tm.leaf("contrast").dial(p.contrast);
                tm.leaf("skew").dial(p.skew);
            }
        }
        else if (mod.operation == "filmicrgb" || mod.operation == "filmic")
        {
            auto p = decode_filmic(mod.params);
            if (p.valid)
            {
                auto &tm = linear.next("toneMapping");
                // greyPoint is in percentage (18.45), convert to fraction
                tm.leaf("greyPoint").dial(p.greyPoint / 100.0f);
                auto &clip = tm.next("clippingPoint");
                clip.leaf("black").dial(p.blackPoint);
                clip.leaf("white").dial(p.whitePoint);
                // Only set contrast if not already set by sigmoid
                if (!tm.test("contrast"))
                    tm.leaf("contrast").dial(p.contrast);
            }
        }
        // ----------------------------------------------------------------
        // GlobalColor: colorbalancergb
        // ----------------------------------------------------------------
        else if (mod.operation == "colorbalancergb")
        {
            auto p = decode_colorbalance(mod.params);
            if (p.valid)
            {
                auto &gc = linear.next("globalColor");
                gc.leaf("saturation").dial(p.saturation);
                gc.leaf("vibrance").dial(p.vibrance);

                // Split tone from chroma/hue
                auto &st = linear.next("splitTone");
                if (std::abs(p.shadowsC) > 0.001f)
                {
                    auto &shadows = st.next("shadows");
                    shadows.leaf("chroma").dial(p.shadowsC);
                    shadows.leaf("hue").dial(p.shadowsH);
                }
                if (std::abs(p.highlightsC) > 0.001f)
                {
                    auto &highlights = st.next("highlights");
                    highlights.leaf("chroma").dial(p.highlightsC);
                    highlights.leaf("hue").dial(p.highlightsH);
                }
            }
        }
        // ----------------------------------------------------------------
        // Detail: bilat (local contrast)
        // ----------------------------------------------------------------
        else if (mod.operation == "bilat")
        {
            auto p = decode_bilat(mod.params);
            if (p.valid && p.mode == 0)  // mode 0 = local contrast
            {
                auto &detail = linear.next("detail");
                auto &lc = detail.next("localContrast");
                lc.leaf("amount").dial(p.detail);
                lc.leaf("radius").dial(p.sigma_s);
            }
        }
    }

    // Write accumulated exposure
    if (hasExposure)
    {
        auto &cc = linear.next("colorCorrection");
        cc.leaf("exposure").dial(totalExposure);
    }
}

// ============================================================================
// Public API
// ============================================================================

bool copy(const char *xmp_data, size_t xmp_size, Tree &info)
{
    if (!xmp_data || xmp_size == 0)
        return false;

    auto modules = parse_xmp(xmp_data, xmp_size);
    if (modules.empty())
        return false;

    auto &vibe = info.root().next("vibe");
    mapToVibe(modules, vibe);

    // Summary
    int count = 0;
    for (const auto &m : modules)
        if (m.enabled)
            count++;
    vibe.leaf("_modules").dial(static_cast<float>(count));

    return true;
}

} // namespace flow
