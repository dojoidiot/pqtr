// JpgTail.cpp - labs::Tail implementation for JPEG encoding
//
// Usage: auto tail = std::make_unique<JpgTail>();
//        auto tail = std::make_unique<JpgTail>(95);  // custom quality

#include "labs.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>

using namespace pqtr;

namespace {

// ============================================================
// JPEG markers
// ============================================================

enum {
    M_SOI = 0xD8, M_EOI = 0xD9, M_SOS = 0xDA, M_DQT = 0xDB,
    M_DHT = 0xC4, M_SOF0 = 0xC0, M_APP0 = 0xE0
};

// Zig-zag order
static const uint8_t zigzag[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

// Standard quantization tables
static const uint8_t std_lum_qt[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99
};

static const uint8_t std_chr_qt[64] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99
};

// Huffman tables
static const uint8_t dc_lum_bits[16] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t dc_lum_vals[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
static const uint8_t dc_chr_bits[16] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
static const uint8_t dc_chr_vals[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

static const uint8_t ac_lum_bits[16] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 125};
static const uint8_t ac_lum_vals[162] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

static const uint8_t ac_chr_bits[16] = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 119};
static const uint8_t ac_chr_vals[162] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};

// ============================================================
// JPEG Encoder
// ============================================================

struct JpegEncoder
{
    std::vector<uint8_t> out;
    uint32_t bitbuf = 0;
    int bitcnt = 0;
    int16_t dc_y = 0, dc_cb = 0, dc_cr = 0;
    uint8_t qt_y[64], qt_c[64];
    uint16_t huff_dc_y[12], huff_dc_c[12];
    uint8_t huff_dc_y_len[12], huff_dc_c_len[12];
    uint16_t huff_ac_y[256], huff_ac_c[256];
    uint8_t huff_ac_y_len[256], huff_ac_c_len[256];

    void write8(uint8_t v) { out.push_back(v); }
    void write16(uint16_t v) { out.push_back(v >> 8); out.push_back(v & 0xFF); }

    void buildHuffCodes(uint16_t *codes, uint8_t *lens, const uint8_t *bits, const uint8_t *vals) {
        uint16_t code = 0;
        int k = 0;
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < bits[i]; j++) {
                codes[vals[k]] = code;
                lens[vals[k]] = i + 1;
                code++; k++;
            }
            code <<= 1;
        }
    }

    void writeBits(uint16_t bits, int len) {
        bitbuf = (bitbuf << len) | bits;
        bitcnt += len;
        while (bitcnt >= 8) {
            uint8_t b = (bitbuf >> (bitcnt - 8)) & 0xFF;
            write8(b);
            if (b == 0xFF) write8(0);
            bitcnt -= 8;
        }
    }

    void flushBits() { if (bitcnt > 0) writeBits(0x7F, 7); }

    int bitSize(int v) {
        int n = 0;
        v = v < 0 ? -v : v;
        while (v) { n++; v >>= 1; }
        return n;
    }

    void encodeDC(int dc, int16_t &pred, const uint16_t *codes, const uint8_t *lens) {
        int diff = dc - pred;
        pred = dc;
        int size = bitSize(diff);
        writeBits(codes[size], lens[size]);
        if (size > 0) {
            int val = diff < 0 ? diff - 1 : diff;
            writeBits(val & ((1 << size) - 1), size);
        }
    }

    void encodeAC(const int16_t *block, const uint16_t *codes, const uint8_t *lens) {
        int zeros = 0;
        for (int i = 1; i < 64; i++) {
            int16_t ac = block[zigzag[i]];
            if (ac == 0) {
                zeros++;
            } else {
                while (zeros >= 16) { writeBits(codes[0xF0], lens[0xF0]); zeros -= 16; }
                int size = bitSize(ac);
                int rs = (zeros << 4) | size;
                writeBits(codes[rs], lens[rs]);
                int val = ac < 0 ? ac - 1 : ac;
                writeBits(val & ((1 << size) - 1), size);
                zeros = 0;
            }
        }
        if (zeros > 0) writeBits(codes[0], lens[0]);
    }

    void fdct(const uint8_t *pixels, int stride, int16_t *block, const uint8_t *qt, int shift) {
        float tmp[64];
        for (int y = 0; y < 8; y++) {
            for (int u = 0; u < 8; u++) {
                float cu = (u == 0) ? 0.353553f : 0.5f;
                float sum = 0;
                for (int x = 0; x < 8; x++)
                    sum += (pixels[y * stride + x] - shift) * cosf((2 * x + 1) * u * 3.14159f / 16);
                tmp[y * 8 + u] = cu * sum;
            }
        }
        for (int u = 0; u < 8; u++) {
            for (int v = 0; v < 8; v++) {
                float cv = (v == 0) ? 0.353553f : 0.5f;
                float sum = 0;
                for (int y = 0; y < 8; y++)
                    sum += tmp[y * 8 + u] * cosf((2 * y + 1) * v * 3.14159f / 16);
                block[v * 8 + u] = static_cast<int16_t>(roundf(cv * sum / qt[v * 8 + u]));
            }
        }
    }

    void encodeBlock(const uint8_t *y, const uint8_t *cb, const uint8_t *cr, int stride) {
        int16_t block[64];
        fdct(y, stride, block, qt_y, 128);
        encodeDC(block[0], dc_y, huff_dc_y, huff_dc_y_len);
        encodeAC(block, huff_ac_y, huff_ac_y_len);

        fdct(cb, stride, block, qt_c, 128);
        encodeDC(block[0], dc_cb, huff_dc_c, huff_dc_c_len);
        encodeAC(block, huff_ac_c, huff_ac_c_len);

        fdct(cr, stride, block, qt_c, 128);
        encodeDC(block[0], dc_cr, huff_dc_c, huff_dc_c_len);
        encodeAC(block, huff_ac_c, huff_ac_c_len);
    }

    void writeMarker(uint8_t m) { write8(0xFF); write8(m); }

    void writeDQT(int id, const uint8_t *qt) {
        writeMarker(M_DQT);
        write16(67);
        write8(id);
        for (int i = 0; i < 64; i++) write8(qt[zigzag[i]]);
    }

    void writeDHT(int tc, int th, const uint8_t *bits, const uint8_t *vals) {
        int total = 0;
        for (int i = 0; i < 16; i++) total += bits[i];
        writeMarker(M_DHT);
        write16(19 + total);
        write8((tc << 4) | th);
        for (int i = 0; i < 16; i++) write8(bits[i]);
        for (int i = 0; i < total; i++) write8(vals[i]);
    }

    void encode(const uint8_t *rgb, int w, int h, int quality) {
        int q = quality < 50 ? 5000 / quality : 200 - quality * 2;
        for (int i = 0; i < 64; i++) {
            qt_y[i] = std::max(1, std::min(255, (std_lum_qt[i] * q + 50) / 100));
            qt_c[i] = std::max(1, std::min(255, (std_chr_qt[i] * q + 50) / 100));
        }

        buildHuffCodes(huff_dc_y, huff_dc_y_len, dc_lum_bits, dc_lum_vals);
        buildHuffCodes(huff_dc_c, huff_dc_c_len, dc_chr_bits, dc_chr_vals);
        buildHuffCodes(huff_ac_y, huff_ac_y_len, ac_lum_bits, ac_lum_vals);
        buildHuffCodes(huff_ac_c, huff_ac_c_len, ac_chr_bits, ac_chr_vals);

        writeMarker(M_SOI);

        // APP0
        writeMarker(M_APP0);
        write16(16);
        write8('J'); write8('F'); write8('I'); write8('F'); write8(0);
        write8(1); write8(1); write8(0);
        write16(1); write16(1);
        write8(0); write8(0);

        writeDQT(0, qt_y);
        writeDQT(1, qt_c);

        // SOF0
        writeMarker(M_SOF0);
        write16(17);
        write8(8);
        write16(h); write16(w);
        write8(3);
        write8(1); write8(0x11); write8(0);
        write8(2); write8(0x11); write8(1);
        write8(3); write8(0x11); write8(1);

        writeDHT(0, 0, dc_lum_bits, dc_lum_vals);
        writeDHT(0, 1, dc_chr_bits, dc_chr_vals);
        writeDHT(1, 0, ac_lum_bits, ac_lum_vals);
        writeDHT(1, 1, ac_chr_bits, ac_chr_vals);

        // SOS
        writeMarker(M_SOS);
        write16(12);
        write8(3);
        write8(1); write8(0x00);
        write8(2); write8(0x11);
        write8(3); write8(0x11);
        write8(0); write8(63); write8(0);

        int mcu_w = (w + 7) / 8;
        int mcu_h = (h + 7) / 8;
        std::vector<uint8_t> y_plane(64), cb_plane(64), cr_plane(64);

        for (int my = 0; my < mcu_h; my++) {
            for (int mx = 0; mx < mcu_w; mx++) {
                for (int j = 0; j < 8; j++) {
                    for (int i = 0; i < 8; i++) {
                        int px = std::min(mx * 8 + i, w - 1);
                        int py = std::min(my * 8 + j, h - 1);
                        int idx = (py * w + px) * 3;
                        int r = rgb[idx], g = rgb[idx + 1], b = rgb[idx + 2];
                        y_plane[j * 8 + i] = std::max(0, std::min(255, (77 * r + 150 * g + 29 * b) >> 8));
                        cb_plane[j * 8 + i] = std::max(0, std::min(255, 128 + ((-43 * r - 85 * g + 128 * b) >> 8)));
                        cr_plane[j * 8 + i] = std::max(0, std::min(255, 128 + ((128 * r - 107 * g - 21 * b) >> 8)));
                    }
                }
                encodeBlock(y_plane.data(), cb_plane.data(), cr_plane.data(), 8);
            }
        }

        flushBits();
        writeMarker(M_EOI);
    }
};

std::vector<uint8_t> encodeJpeg(const uint8_t *rgb, int width, int height, int quality)
{
    if (!rgb || width <= 0 || height <= 0) return {};
    quality = std::max(1, std::min(100, quality));
    JpegEncoder enc;
    enc.encode(rgb, width, height, quality);
    return std::move(enc.out);
}

} // anonymous namespace

// ============================================================================
// JpgTail - JPEG encoder plugin
// ============================================================================

class JpgTail : public Tail
{
    std::vector<uint8_t> output_;
    int quality_;

public:
    explicit JpgTail(int quality = 90) : quality_(quality) {}

    void* save(Flow& flow) override
    {
        auto& info = flow.flow();
        int width = static_cast<int>(info.leaf(WIDTH).dial());
        int height = static_cast<int>(info.leaf(HEIGHT).dial());

        if (width <= 0 || height <= 0)
            return nullptr;

        // Get data as uint8_t RGB (assumes upstream converted to display format)
        uint8_t* src = static_cast<uint8_t*>(flow.data());

        output_ = encodeJpeg(src, width, height, quality_);
        return output_.data();
    }

    size_t size() const { return output_.size(); }
    void setQuality(int q) { quality_ = std::max(1, std::min(100, q)); }
};

// Factory function
std::unique_ptr<Tail> makeJpgTail()
{
    return std::make_unique<JpgTail>();
}
