// jpeg.cpp - Clean-room baseline JPEG decoder
// Supports: SOF0 (baseline DCT), 8-bit, YCbCr

#include "pipe.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace pipe {

// JPEG markers
enum {
    M_SOI  = 0xD8, M_EOI  = 0xD9, M_SOS  = 0xDA,
    M_DQT  = 0xDB, M_DHT  = 0xC4, M_SOF0 = 0xC0,
    M_APP0 = 0xE0, M_APP1 = 0xE1, M_COM  = 0xFE,
    M_DRI  = 0xDD, M_RST0 = 0xD0
};

// Zig-zag order for 8x8 block
static const uint8_t zigzag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

// Huffman table
struct HuffTable {
    uint8_t bits[16];      // Number of codes of each length
    uint8_t vals[256];     // Symbol values
    uint16_t code[256];    // Huffman codes
    uint8_t size[256];     // Code lengths
    int maxcode[16];       // Max code for each length
    int valptr[16];        // Value pointer for each length
    int nsym;              // Total symbols
};

// Decoder state
struct JpegDecoder {
    const uint8_t* data;
    size_t size;
    size_t pos;

    int width, height;
    int ncomp;
    uint8_t comp_id[3], comp_h[3], comp_v[3], comp_qt[3];
    int max_h, max_v;

    uint8_t qt[4][64];           // Quantization tables
    HuffTable huff_dc[4];        // DC Huffman tables
    HuffTable huff_ac[4];        // AC Huffman tables
    uint8_t comp_dc[3], comp_ac[3];  // Table indices per component

    // Bit reader state
    uint32_t bits;
    int nbits;

    int16_t dc_pred[3];  // DC predictors

    std::vector<uint8_t> rgb;

    uint8_t get8() {
        return (pos < size) ? data[pos++] : 0;
    }

    uint16_t get16() {
        uint8_t hi = get8();
        return (hi << 8) | get8();
    }

    void skip(int n) { pos += n; }

    // Fill bit buffer, handling stuffed bytes
    void fillbits() {
        while (nbits < 24 && pos < size) {
            uint8_t b = data[pos++];
            if (b == 0xFF) {
                uint8_t next = (pos < size) ? data[pos] : 0;
                if (next == 0) pos++;  // Stuffed byte
                else if (next >= M_RST0 && next <= M_RST0 + 7) pos++;
                else { pos--; break; }
            }
            bits = (bits << 8) | b;
            nbits += 8;
        }
    }

    int getbits(int n) {
        if (nbits < n) fillbits();
        nbits -= n;
        return (bits >> nbits) & ((1 << n) - 1);
    }

    int peekbits(int n) {
        if (nbits < n) fillbits();
        return (bits >> (nbits - n)) & ((1 << n) - 1);
    }

    void dropbits(int n) { nbits -= n; }

    // Build Huffman decode tables
    void buildHuff(HuffTable& h) {
        int code = 0, k = 0;
        h.nsym = 0;
        for (int i = 0; i < 16; i++) h.nsym += h.bits[i];

        for (int i = 0; i < 16; i++) {
            h.valptr[i] = k;
            for (int j = 0; j < h.bits[i]; j++) {
                h.code[k] = code;
                h.size[k] = i + 1;
                code++;
                k++;
            }
            h.maxcode[i] = code - 1;
            code <<= 1;
        }
    }

    // Decode one Huffman symbol
    int huffDecode(HuffTable& h) {
        int code = 0;
        for (int i = 0; i < 16; i++) {
            code = (code << 1) | getbits(1);
            if (code <= h.maxcode[i]) {
                return h.vals[h.valptr[i] + code - (h.maxcode[i] - h.bits[i] + 1)];
            }
        }
        return 0;
    }

    // Extend sign bit
    int extend(int v, int bits) {
        if (bits == 0) return 0;
        int vt = 1 << (bits - 1);
        return (v < vt) ? (v - (2 * vt - 1)) : v;
    }

    // Decode one 8x8 block
    void decodeBlock(int16_t* block, int comp) {
        memset(block, 0, 64 * sizeof(int16_t));

        // DC coefficient
        int s = huffDecode(huff_dc[comp_dc[comp]]);
        if (s > 0) {
            int dc = extend(getbits(s), s);
            dc_pred[comp] += dc;
        }
        block[0] = dc_pred[comp] * qt[comp_qt[comp]][0];

        // AC coefficients
        for (int k = 1; k < 64; ) {
            int rs = huffDecode(huff_ac[comp_ac[comp]]);
            int r = rs >> 4;    // Run of zeros
            s = rs & 0x0F;      // Size

            if (s == 0) {
                if (r == 15) k += 16;  // ZRL
                else break;            // EOB
            } else {
                k += r;
                if (k < 64) {
                    int ac = extend(getbits(s), s);
                    block[zigzag[k]] = ac * qt[comp_qt[comp]][k];
                    k++;
                }
            }
        }
    }

    // Inverse DCT (AAN algorithm, integer approximation)
    void idct(int16_t* block, uint8_t* out, int stride) {
        int tmp[64];

        // Rows
        for (int i = 0; i < 8; i++) {
            int* row = &tmp[i * 8];
            int16_t* src = &block[i * 8];

            int s0 = src[0], s1 = src[1], s2 = src[2], s3 = src[3];
            int s4 = src[4], s5 = src[5], s6 = src[6], s7 = src[7];

            int p2 = s2, p3 = s6;
            int p1 = ((p2 + p3) * 2217) >> 12;
            int t2 = (p1 + (p3 * -7567)) >> 12;
            int t3 = (p1 + (p2 * 3135)) >> 12;

            p2 = s0 + s4; p3 = s0 - s4;
            int t0 = p2 + t3, t1 = p3 + t2;
            t2 = p3 - t2; t3 = p2 - t3;

            p1 = s7 + s1; p2 = s5 + s3;
            int p3a = s7 + s3, p4 = s5 + s1;
            int p5 = ((p3a + p4) * 4816) >> 12;
            p1 = (p1 * -3685) >> 12; p2 = (p2 * -10497) >> 12;
            p3a = (p3a * -8034) >> 12; p4 = (p4 * -1597) >> 12;
            p3a += p5; p4 += p5;

            int t4 = (p1 + p3a + (s7 * 1223)) >> 12;
            int t5 = (p2 + p4 + (s5 * 8410)) >> 12;
            int t6 = (p2 + p3a + (s3 * 12785)) >> 12;
            int t7 = (p1 + p4 + (s1 * 6149)) >> 12;

            row[0] = t0 + t7; row[7] = t0 - t7;
            row[1] = t1 + t6; row[6] = t1 - t6;
            row[2] = t2 + t5; row[5] = t2 - t5;
            row[3] = t3 + t4; row[4] = t3 - t4;
        }

        // Columns
        for (int i = 0; i < 8; i++) {
            int s0 = tmp[i], s1 = tmp[i+8], s2 = tmp[i+16], s3 = tmp[i+24];
            int s4 = tmp[i+32], s5 = tmp[i+40], s6 = tmp[i+48], s7 = tmp[i+56];

            int p2 = s2, p3 = s6;
            int p1 = ((p2 + p3) * 2217) >> 12;
            int t2 = (p1 + (p3 * -7567)) >> 12;
            int t3 = (p1 + (p2 * 3135)) >> 12;

            p2 = s0 + s4; p3 = s0 - s4;
            int t0 = p2 + t3, t1 = p3 + t2;
            t2 = p3 - t2; t3 = p2 - t3;

            p1 = s7 + s1; p2 = s5 + s3;
            int p3a = s7 + s3, p4 = s5 + s1;
            int p5 = ((p3a + p4) * 4816) >> 12;
            p1 = (p1 * -3685) >> 12; p2 = (p2 * -10497) >> 12;
            p3a = (p3a * -8034) >> 12; p4 = (p4 * -1597) >> 12;
            p3a += p5; p4 += p5;

            int t4 = (p1 + p3a + (s7 * 1223)) >> 12;
            int t5 = (p2 + p4 + (s5 * 8410)) >> 12;
            int t6 = (p2 + p3a + (s3 * 12785)) >> 12;
            int t7 = (p1 + p4 + (s1 * 6149)) >> 12;

            auto clamp = [](int v) -> uint8_t {
                v = (v + 128 * 16 + 8) >> 4;
                return (v < 0) ? 0 : (v > 255) ? 255 : v;
            };

            out[i + 0*stride] = clamp(t0 + t7);
            out[i + 7*stride] = clamp(t0 - t7);
            out[i + 1*stride] = clamp(t1 + t6);
            out[i + 6*stride] = clamp(t1 - t6);
            out[i + 2*stride] = clamp(t2 + t5);
            out[i + 5*stride] = clamp(t2 - t5);
            out[i + 3*stride] = clamp(t3 + t4);
            out[i + 4*stride] = clamp(t3 - t4);
        }
    }

    bool decode();
};

bool JpegDecoder::decode() {
    if (get8() != 0xFF || get8() != M_SOI) return false;

    while (pos < size) {
        if (get8() != 0xFF) continue;
        uint8_t marker = get8();
        if (marker == 0 || marker == 0xFF) continue;

        if (marker == M_EOI) break;
        if (marker == M_SOS) break;  // Start scan

        uint16_t len = get16();
        size_t next = pos + len - 2;

        if (marker == M_DQT) {
            while (pos < next) {
                uint8_t info = get8();
                int idx = info & 0x0F;
                if (idx > 3) return false;
                for (int i = 0; i < 64; i++) {
                    qt[idx][zigzag[i]] = (info >> 4) ? get16() : get8();
                }
            }
        } else if (marker == M_SOF0) {
            get8();  // precision (8)
            height = get16();
            width = get16();
            ncomp = get8();
            if (ncomp != 3 && ncomp != 1) return false;

            max_h = max_v = 0;
            for (int i = 0; i < ncomp; i++) {
                comp_id[i] = get8();
                uint8_t hv = get8();
                comp_h[i] = hv >> 4;
                comp_v[i] = hv & 0x0F;
                comp_qt[i] = get8();
                if (comp_h[i] > max_h) max_h = comp_h[i];
                if (comp_v[i] > max_v) max_v = comp_v[i];
            }
        } else if (marker == M_DHT) {
            while (pos < next) {
                uint8_t info = get8();
                int tc = info >> 4;   // Table class (0=DC, 1=AC)
                int th = info & 0x0F; // Table index
                if (th > 3) return false;

                HuffTable& h = tc ? huff_ac[th] : huff_dc[th];
                int total = 0;
                for (int i = 0; i < 16; i++) {
                    h.bits[i] = get8();
                    total += h.bits[i];
                }
                for (int i = 0; i < total; i++) {
                    h.vals[i] = get8();
                }
                buildHuff(h);
            }
        } else {
            pos = next;
        }
    }

    // Parse SOS
    (void)get16(); // length, not validated
    int ns = get8();
    for (int i = 0; i < ns; i++) {
        uint8_t id = get8();
        uint8_t td_ta = get8();
        for (int c = 0; c < ncomp; c++) {
            if (comp_id[c] == id) {
                comp_dc[c] = td_ta >> 4;
                comp_ac[c] = td_ta & 0x0F;
            }
        }
    }
    skip(3);  // Ss, Se, Ah/Al

    // Decode MCUs
    int mcu_w = max_h * 8;
    int mcu_h = max_v * 8;
    int mcu_cols = (width + mcu_w - 1) / mcu_w;
    int mcu_rows = (height + mcu_h - 1) / mcu_h;

    // Allocate component planes
    std::vector<uint8_t> planes[3];
    int plane_w[3], plane_h[3];
    for (int c = 0; c < ncomp; c++) {
        plane_w[c] = mcu_cols * comp_h[c] * 8;
        plane_h[c] = mcu_rows * comp_v[c] * 8;
        planes[c].resize(plane_w[c] * plane_h[c]);
    }

    bits = 0; nbits = 0;
    dc_pred[0] = dc_pred[1] = dc_pred[2] = 0;

    int16_t block[64];

    for (int my = 0; my < mcu_rows; my++) {
        for (int mx = 0; mx < mcu_cols; mx++) {
            for (int c = 0; c < ncomp; c++) {
                for (int v = 0; v < comp_v[c]; v++) {
                    for (int h = 0; h < comp_h[c]; h++) {
                        decodeBlock(block, c);
                        int bx = mx * comp_h[c] * 8 + h * 8;
                        int by = my * comp_v[c] * 8 + v * 8;
                        idct(block, &planes[c][by * plane_w[c] + bx], plane_w[c]);
                    }
                }
            }
        }
    }

    // Convert YCbCr to RGB
    rgb.resize(width * height * 3);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int Y, Cb, Cr;

            if (ncomp == 1) {
                Y = planes[0][y * plane_w[0] + x];
                Cb = Cr = 128;
            } else {
                // Handle subsampling
                int cx = x * comp_h[1] / max_h;
                int cy = y * comp_v[1] / max_v;
                Y = planes[0][y * plane_w[0] + x];
                Cb = planes[1][cy * plane_w[1] + cx];
                Cr = planes[2][cy * plane_w[2] + cx];
            }

            // YCbCr to RGB
            int r = Y + ((Cr - 128) * 359 >> 8);
            int g = Y - ((Cb - 128) * 88 >> 8) - ((Cr - 128) * 183 >> 8);
            int b = Y + ((Cb - 128) * 454 >> 8);

            auto clamp = [](int v) -> uint8_t {
                return (v < 0) ? 0 : (v > 255) ? 255 : v;
            };

            int idx = (y * width + x) * 3;
            rgb[idx + 0] = clamp(r);
            rgb[idx + 1] = clamp(g);
            rgb[idx + 2] = clamp(b);
        }
    }

    return true;
}

JpegResult decodeJpeg(const uint8_t* data, size_t size) {
    JpegResult result = {0, 0, {}};

    JpegDecoder dec;
    dec.data = data;
    dec.size = size;
    dec.pos = 0;

    if (dec.decode()) {
        result.width = dec.width;
        result.height = dec.height;
        result.rgb = std::move(dec.rgb);
    }

    return result;
}

} // namespace pipe
