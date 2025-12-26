// dark.cpp - XMP parser test tool
//
// Reads darktable XMP sidecar files and prints module parameters.
// Used for step-wise darktable module matching.
//
// Usage: ./dark <file.xmp>

#include "../../inc/pipe.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <zlib.h>

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

[[maybe_unused]]
static int32_t read_int32_le(const uint8_t *p)
{
    return static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// ============================================================================
// Base64 + zlib decompression for darktable compressed params
// ============================================================================

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
    std::vector<uint8_t> out(8192);
    z_stream strm = {};
    strm.next_in = compressed.data();
    strm.avail_in = static_cast<uInt>(compressed.size());
    strm.next_out = out.data();
    strm.avail_out = static_cast<uInt>(out.size());

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
// Darktable module parser
// ============================================================================

struct DarktableModule
{
    std::string operation;
    bool enabled = false;
    int num = -1;
    int version = 0;
    std::string params;
    std::string multi_name;
};

static std::vector<DarktableModule> parse_xmp(const std::string &xml)
{
    std::vector<DarktableModule> modules;

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
            mod.num = std::atoi(get_attr(tag, "darktable:num").c_str());
            mod.version = std::atoi(get_attr(tag, "darktable:modversion").c_str());
            mod.params = get_attr(tag, "darktable:params");
            mod.multi_name = get_attr(tag, "darktable:multi_name");
            modules.push_back(mod);
        }

        pos = end + 2;
    }

    return modules;
}

// ============================================================================
// Param dumping helpers
// ============================================================================

static void dump_hex_params(const std::string &params, int max_floats = 16)
{
    auto bytes = decode_hex(params);
    if (bytes.empty()) {
        std::cout << "    (empty)\n";
        return;
    }

    std::cout << "    raw: " << bytes.size() << " bytes\n";

    int nfloats = std::min(max_floats, (int)(bytes.size() / 4));
    for (int i = 0; i < nfloats; i++) {
        float f = read_float_le(&bytes[i * 4]);
        std::cout << "    [" << i << "] = " << f << "\n";
    }
}

static void dump_gz_params(const std::string &params, int max_floats = 16)
{
    auto bytes = decompress_gz(params);
    if (bytes.empty()) {
        std::cout << "    (failed to decompress)\n";
        return;
    }

    std::cout << "    raw: " << bytes.size() << " bytes (decompressed)\n";

    int nfloats = std::min(max_floats, (int)(bytes.size() / 4));
    for (int i = 0; i < nfloats; i++) {
        float f = read_float_le(&bytes[i * 4]);
        std::cout << "    [" << i << "] = " << f << "\n";
    }
}

static void dump_params(const DarktableModule &mod)
{
    if (mod.params.empty()) {
        std::cout << "    (no params)\n";
        return;
    }

    // Check if compressed (starts with "gz")
    if (mod.params.size() > 2 && mod.params[0] == 'g' && mod.params[1] == 'z') {
        dump_gz_params(mod.params);
    } else {
        dump_hex_params(mod.params);
    }
}

// ============================================================================
// Main
// ============================================================================

static std::string read_file(const char *path)
{
    std::ifstream f(path);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.xmp> [--dump]\n";
        std::cerr << "  --dump  Show raw parameter values\n";
        return 1;
    }

    bool dump = (argc > 2 && std::string(argv[2]) == "--dump");

    std::string xmp = read_file(argv[1]);
    if (xmp.empty()) {
        std::cerr << "Failed to read: " << argv[1] << "\n";
        return 1;
    }

    auto modules = parse_xmp(xmp);
    if (modules.empty()) {
        std::cerr << "No modules found in XMP\n";
        return 1;
    }

    std::cout << "=== Darktable Modules ===\n\n";
    std::cout << "Found " << modules.size() << " modules:\n\n";

    for (const auto &mod : modules) {
        std::cout << "[" << mod.num << "] " << mod.operation;
        if (!mod.multi_name.empty())
            std::cout << " (" << mod.multi_name << ")";
        std::cout << " v" << mod.version;
        std::cout << (mod.enabled ? " [ON]" : " [off]");
        std::cout << "\n";

        if (dump && mod.enabled) {
            dump_params(mod);
            std::cout << "\n";
        }
    }

    // Summary of enabled modules in order
    std::cout << "\n=== Pipeline Order ===\n\n";
    int step = 0;
    for (const auto &mod : modules) {
        if (mod.enabled) {
            std::cout << step++ << ": " << mod.operation << "\n";
        }
    }

    return 0;
}
