// SonyHead.cpp - Sony ARW head using pipe/src/main/labs/sony.c

#include "labs.hpp"
#include <fstream>
#include <vector>
#include <cstring>
#include <iostream>

// Forward declarations from sony.c
extern "C" {
    typedef struct {
        int width;
        int height;
        int strip_offset;
        uint16_t sony_curve[4];
        int black_level;
        int white_level;
        float wb_rggb[4];
        float color_matrix[9];
        uint32_t filters;
        float exposure_bias;
        float xyz_to_cam[9];
        float d65_coeffs[4];
    } SonyARWMeta;

    int sony_arw_read_meta(const char* filename, SonyARWMeta* meta);
    int sony_arw2_decode(const uint8_t* compressed_data, int compressed_size,
                         int width, int height, const uint16_t* sony_curve,
                         uint16_t* output);
}

namespace pqtr {

std::unique_ptr<Flow> SonyHead::load(Flow& flow, const void* bytes, size_t size) {
    // bytes is the filename
    std::string filename(static_cast<const char*>(bytes), size);

    // Read metadata
    SonyARWMeta meta;
    if (sony_arw_read_meta(filename.c_str(), &meta) != 0) {
        std::cerr << "SonyHead: failed to read metadata from " << filename << "\n";
        return nullptr;
    }

    // Populate head stem with metadata
    Stem& h = flow.head();

    std::string name = filename;
    h.leaf(NAME).text(name);
    h.leaf(WIDTH).dial(static_cast<float>(meta.width));
    h.leaf(HEIGHT).dial(static_cast<float>(meta.height));
    h.leaf(BLACK).dial(static_cast<float>(meta.black_level));
    h.leaf(WHITE).dial(static_cast<float>(meta.white_level));

    // Bayer filter pattern (stored as hex string to preserve uint32 precision)
    char filters_hex[16];
    snprintf(filters_hex, sizeof(filters_hex), "0x%08x", meta.filters);
    std::string filters_str(filters_hex);
    h.leaf("filters").text(filters_str);

    // Exposure bias
    h.leaf("exposure_bias").dial(meta.exposure_bias);

    // White balance RGGB
    Stem& wb = h.next("wb");
    wb.leaf("r").dial(meta.wb_rggb[0]);
    wb.leaf("g1").dial(meta.wb_rggb[1]);
    wb.leaf("b").dial(meta.wb_rggb[2]);
    wb.leaf("g2").dial(meta.wb_rggb[3]);

    // D65 coefficients
    Stem& d65 = h.next("d65");
    d65.leaf("r").dial(meta.d65_coeffs[0]);
    d65.leaf("g1").dial(meta.d65_coeffs[1]);
    d65.leaf("b").dial(meta.d65_coeffs[2]);
    d65.leaf("g2").dial(meta.d65_coeffs[3]);

    // Color matrix (3x3)
    Stem& cm = h.next("color_matrix");
    for (int i = 0; i < 9; i++) {
        cm.leaf(std::to_string(i)).dial(meta.color_matrix[i]);
    }

    // XYZ to CAM matrix
    Stem& xyz = h.next("xyz_to_cam");
    for (int i = 0; i < 9; i++) {
        xyz.leaf(std::to_string(i)).dial(meta.xyz_to_cam[i]);
    }

    // Sony curve values
    Stem& curve = h.next("sony_curve");
    for (int i = 0; i < 4; i++) {
        curve.leaf(std::to_string(i)).dial(static_cast<float>(meta.sony_curve[i]));
    }

    // Read full file for compressed data
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "SonyHead: failed to open " << filename << "\n";
        return nullptr;
    }

    size_t file_size = file.tellg();
    file.seekg(meta.strip_offset);

    int compressed_size = file_size - meta.strip_offset;
    std::vector<uint8_t> compressed(compressed_size);
    file.read(reinterpret_cast<char*>(compressed.data()), compressed_size);
    file.close();

    // Allocate output buffer for uint16_t bayer
    size_t npixels = static_cast<size_t>(meta.width) * meta.height;
    flow.resize(npixels * sizeof(uint16_t));

    // Decode
    if (sony_arw2_decode(compressed.data(), compressed_size,
                         meta.width, meta.height, meta.sony_curve,
                         static_cast<uint16_t*>(flow.data())) != 0) {
        std::cerr << "SonyHead: decode failed\n";
        return nullptr;
    }

    std::cout << "SonyHead: loaded " << filename << "\n";
    std::cout << "  size: " << meta.width << "x" << meta.height << "\n";
    std::cout << "  black: " << meta.black_level << " white: " << meta.white_level << "\n";
    std::cout << "  decoded: " << npixels << " pixels\n";

    return nullptr;
}

}  // namespace pqtr
