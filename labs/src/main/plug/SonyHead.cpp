// SonyHead.cpp - Sony ARW head using pipe/src/main/labs/sony.c

#include "pqtr.hpp"
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

namespace pqtr::Labs {

class SonyHead : public Head
{
public:
    std::unique_ptr<Flow> load(Flow &flow, const void *bytes, size_t size) override;
};

std::unique_ptr<Flow> SonyHead::load(Flow& flow, const void* bytes, size_t size) {
    // bytes is the filename
    std::string filename(static_cast<const char*>(bytes), size);

    // Read metadata
    SonyARWMeta meta;
    if (sony_arw_read_meta(filename.c_str(), &meta) != 0) {
        std::cerr << "SonyHead: failed to read metadata from " << filename << "\n";
        return nullptr;
    }

    // Populate Flow execution state
    flow.width(meta.width);
    flow.height(meta.height);
    flow.filters(meta.filters);
    flow.chroma().asShot(0, meta.wb_rggb[0]);
    flow.chroma().asShot(1, meta.wb_rggb[1]);
    flow.chroma().asShot(2, meta.wb_rggb[2]);
    flow.chroma().asShot(3, meta.wb_rggb[3]);
    flow.chroma().lateCorrection(true);
    flow.chroma().D65(0, meta.d65_coeffs[0]);
    flow.chroma().D65(1, meta.d65_coeffs[1]);
    flow.chroma().D65(2, meta.d65_coeffs[2]);
    flow.chroma().D65(3, meta.d65_coeffs[3]);
    flow.exposureBias(meta.exposure_bias);

    // Also populate head stem for persistence/JSON
    Stem& h = flow.head();
    std::string name = filename;
    h.leaf(NAME).text(name);
    h.leaf(WIDTH).dial(static_cast<float>(meta.width));
    h.leaf(HEIGHT).dial(static_cast<float>(meta.height));
    h.leaf(BLACK).dial(static_cast<float>(meta.black_level));
    h.leaf(WHITE).dial(static_cast<float>(meta.white_level));

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

std::unique_ptr<Head> sonyHead() {
    return std::make_unique<SonyHead>();
}

}  // namespace pqtr::Labs
