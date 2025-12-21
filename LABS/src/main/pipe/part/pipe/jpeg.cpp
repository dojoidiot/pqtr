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

ImageResult decodeJpeg(const uint8_t* data, size_t size) {
    ImageResult result = {0, 0, {}};

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

// ============================================================
// JPEG Encoder (baseline DCT, quality 1-100)
// ============================================================

// Standard luminance quantization table
static const uint8_t std_lum_qt[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68,109,103, 77,
    24, 35, 55, 64, 81,104,113, 92,
    49, 64, 78, 87,103,121,120,101,
    72, 92, 95, 98,112,100,103, 99
};

// Standard chrominance quantization table
static const uint8_t std_chr_qt[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

// Standard DC luminance Huffman table
static const uint8_t dc_lum_bits[16] = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const uint8_t dc_lum_vals[12] = {0,1,2,3,4,5,6,7,8,9,10,11};

// Standard DC chrominance Huffman table
static const uint8_t dc_chr_bits[16] = {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static const uint8_t dc_chr_vals[12] = {0,1,2,3,4,5,6,7,8,9,10,11};

// Standard AC luminance Huffman table
static const uint8_t ac_lum_bits[16] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125};
static const uint8_t ac_lum_vals[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

// Standard AC chrominance Huffman table
static const uint8_t ac_chr_bits[16] = {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119};
static const uint8_t ac_chr_vals[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

struct JpegEncoder {
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

    void buildHuffCodes(uint16_t* codes, uint8_t* lens, const uint8_t* bits, const uint8_t* vals) {
        uint16_t code = 0;
        int k = 0;
        for (int i = 0; i < 16; i++) {
            for (int j = 0; j < bits[i]; j++) {
                codes[vals[k]] = code;
                lens[vals[k]] = i + 1;
                code++;
                k++;
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
            if (b == 0xFF) write8(0);  // Stuffing
            bitcnt -= 8;
        }
    }

    void flushBits() {
        if (bitcnt > 0) {
            writeBits(0x7F, 7);  // Pad with 1s
        }
    }

    int bitSize(int v) {
        int n = 0;
        v = v < 0 ? -v : v;
        while (v) { n++; v >>= 1; }
        return n;
    }

    void encodeDC(int dc, int16_t& pred, const uint16_t* codes, const uint8_t* lens) {
        int diff = dc - pred;
        pred = dc;
        int size = bitSize(diff);
        writeBits(codes[size], lens[size]);
        if (size > 0) {
            int val = diff < 0 ? diff - 1 : diff;
            writeBits(val & ((1 << size) - 1), size);
        }
    }

    void encodeAC(const int16_t* block, const uint16_t* codes, const uint8_t* lens) {
        int zeros = 0;
        for (int i = 1; i < 64; i++) {
            int16_t ac = block[zigzag[i]];
            if (ac == 0) {
                zeros++;
            } else {
                while (zeros >= 16) {
                    writeBits(codes[0xF0], lens[0xF0]);  // ZRL
                    zeros -= 16;
                }
                int size = bitSize(ac);
                int rs = (zeros << 4) | size;
                writeBits(codes[rs], lens[rs]);
                int val = ac < 0 ? ac - 1 : ac;
                writeBits(val & ((1 << size) - 1), size);
                zeros = 0;
            }
        }
        if (zeros > 0) writeBits(codes[0], lens[0]);  // EOB
    }

    void fdct(const uint8_t* pixels, int stride, int16_t* block, const uint8_t* qt, int shift) {
        float tmp[64];
        // Rows
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                float sum = 0;
                for (int u = 0; u < 8; u++) {
                    float cu = (u == 0) ? 0.353553f : 0.5f;
                    sum += cu * cosf((2*x+1)*u*3.14159f/16) * (pixels[y*stride+u] - shift);
                }
                tmp[y*8+x] = sum;
            }
        }
        // Columns
        for (int x = 0; x < 8; x++) {
            for (int y = 0; y < 8; y++) {
                float sum = 0;
                for (int v = 0; v < 8; v++) {
                    float cv = (v == 0) ? 0.353553f : 0.5f;
                    sum += cv * cosf((2*y+1)*v*3.14159f/16) * tmp[v*8+x];
                }
                block[y*8+x] = static_cast<int16_t>(roundf(sum / qt[y*8+x]));
            }
        }
    }

    void encodeBlock(const uint8_t* y, const uint8_t* cb, const uint8_t* cr, int stride) {
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

    void writeDQT(int id, const uint8_t* qt) {
        writeMarker(M_DQT);
        write16(67);
        write8(id);
        for (int i = 0; i < 64; i++) write8(qt[zigzag[i]]);
    }

    void writeDHT(int tc, int th, const uint8_t* bits, const uint8_t* vals) {
        int total = 0;
        for (int i = 0; i < 16; i++) total += bits[i];
        writeMarker(M_DHT);
        write16(19 + total);
        write8((tc << 4) | th);
        for (int i = 0; i < 16; i++) write8(bits[i]);
        for (int i = 0; i < total; i++) write8(vals[i]);
    }

    void encode(const uint8_t* rgb, int w, int h, int quality) {
        // Scale quantization tables
        int q = quality < 50 ? 5000 / quality : 200 - quality * 2;
        for (int i = 0; i < 64; i++) {
            qt_y[i] = std::max(1, std::min(255, (std_lum_qt[i] * q + 50) / 100));
            qt_c[i] = std::max(1, std::min(255, (std_chr_qt[i] * q + 50) / 100));
        }

        // Build Huffman tables
        buildHuffCodes(huff_dc_y, huff_dc_y_len, dc_lum_bits, dc_lum_vals);
        buildHuffCodes(huff_dc_c, huff_dc_c_len, dc_chr_bits, dc_chr_vals);
        buildHuffCodes(huff_ac_y, huff_ac_y_len, ac_lum_bits, ac_lum_vals);
        buildHuffCodes(huff_ac_c, huff_ac_c_len, ac_chr_bits, ac_chr_vals);

        // SOI
        writeMarker(M_SOI);

        // APP0 (JFIF)
        writeMarker(M_APP0);
        write16(16);
        write8('J'); write8('F'); write8('I'); write8('F'); write8(0);
        write8(1); write8(1);  // Version
        write8(0);  // Units
        write16(1); write16(1);  // Density
        write8(0); write8(0);  // Thumbnail

        // DQT
        writeDQT(0, qt_y);
        writeDQT(1, qt_c);

        // SOF0
        writeMarker(M_SOF0);
        write16(17);
        write8(8);  // Precision
        write16(h); write16(w);
        write8(3);  // Components
        write8(1); write8(0x11); write8(0);  // Y
        write8(2); write8(0x11); write8(1);  // Cb
        write8(3); write8(0x11); write8(1);  // Cr

        // DHT
        writeDHT(0, 0, dc_lum_bits, dc_lum_vals);
        writeDHT(0, 1, dc_chr_bits, dc_chr_vals);
        writeDHT(1, 0, ac_lum_bits, ac_lum_vals);
        writeDHT(1, 1, ac_chr_bits, ac_chr_vals);

        // SOS
        writeMarker(M_SOS);
        write16(12);
        write8(3);
        write8(1); write8(0x00);  // Y: DC0, AC0
        write8(2); write8(0x11);  // Cb: DC1, AC1
        write8(3); write8(0x11);  // Cr: DC1, AC1
        write8(0); write8(63); write8(0);

        // Convert RGB to YCbCr and encode MCUs
        int mcu_w = (w + 7) / 8;
        int mcu_h = (h + 7) / 8;
        std::vector<uint8_t> y_plane(64), cb_plane(64), cr_plane(64);

        for (int my = 0; my < mcu_h; my++) {
            for (int mx = 0; mx < mcu_w; mx++) {
                // Fill 8x8 block
                for (int j = 0; j < 8; j++) {
                    for (int i = 0; i < 8; i++) {
                        int px = std::min(mx * 8 + i, w - 1);
                        int py = std::min(my * 8 + j, h - 1);
                        int idx = (py * w + px) * 3;
                        int r = rgb[idx], g = rgb[idx+1], b = rgb[idx+2];
                        y_plane[j*8+i] = std::max(0, std::min(255, (77*r + 150*g + 29*b) >> 8));
                        cb_plane[j*8+i] = std::max(0, std::min(255, 128 + ((-43*r - 85*g + 128*b) >> 8)));
                        cr_plane[j*8+i] = std::max(0, std::min(255, 128 + ((128*r - 107*g - 21*b) >> 8)));
                    }
                }
                encodeBlock(y_plane.data(), cb_plane.data(), cr_plane.data(), 8);
            }
        }

        flushBits();
        writeMarker(M_EOI);
    }
};

std::vector<uint8_t> encodeJpeg(const uint8_t* rgb, int width, int height, int quality) {
    if (!rgb || width <= 0 || height <= 0) return {};
    quality = std::max(1, std::min(100, quality));
    JpegEncoder enc;
    enc.encode(rgb, width, height, quality);
    return std::move(enc.out);
}

} // namespace pipe
