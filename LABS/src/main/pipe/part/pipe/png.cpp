// png.cpp - Clean-room PNG encoder for pipe
// Uses store-only DEFLATE (no compression library needed)

#include "pipe.hpp"
#include <vector>
#include <cstdint>
#include <cstring>

namespace pipe {

// CRC32 lookup table (IEEE polynomial)
static uint32_t crc_table[256];
static bool crc_table_init = false;

static void init_crc_table() {
    if (crc_table_init) return;
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc_table[n] = c;
    }
    crc_table_init = true;
}

static uint32_t crc32(const uint8_t* data, size_t len) {
    init_crc_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// Adler-32 checksum for zlib
static uint32_t adler32(const uint8_t* data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// Write 32-bit big-endian
static void write_be32(std::vector<uint8_t>& out, uint32_t val) {
    out.push_back((val >> 24) & 0xFF);
    out.push_back((val >> 16) & 0xFF);
    out.push_back((val >> 8) & 0xFF);
    out.push_back(val & 0xFF);
}

// Write 16-bit little-endian
static void write_le16(std::vector<uint8_t>& out, uint16_t val) {
    out.push_back(val & 0xFF);
    out.push_back((val >> 8) & 0xFF);
}

// Write PNG chunk: length + type + data + crc
static void write_chunk(std::vector<uint8_t>& out, const char* type, const uint8_t* data, size_t len) {
    write_be32(out, static_cast<uint32_t>(len));

    // Type + data for CRC calculation
    size_t crc_start = out.size();
    out.insert(out.end(), type, type + 4);
    if (data && len > 0) {
        out.insert(out.end(), data, data + len);
    }

    // CRC over type + data
    write_be32(out, crc32(&out[crc_start], 4 + len));
}

// Create zlib stream with store-only DEFLATE blocks (no compression)
static std::vector<uint8_t> deflate_store(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out;

    // Zlib header: CMF=0x78 (deflate, 32K window), FLG=0x01 (no dict, check ok)
    out.push_back(0x78);
    out.push_back(0x01);

    // Split into 65535-byte blocks (max for stored block)
    const size_t BLOCK_MAX = 65535;
    size_t pos = 0;

    while (pos < len) {
        size_t block_len = (len - pos > BLOCK_MAX) ? BLOCK_MAX : (len - pos);
        bool final = (pos + block_len >= len);

        // Block header: BFINAL (1 if last) + BTYPE=00 (stored)
        out.push_back(final ? 0x01 : 0x00);

        // LEN and NLEN (little-endian)
        uint16_t blen = static_cast<uint16_t>(block_len);
        write_le16(out, blen);
        write_le16(out, ~blen);

        // Data
        out.insert(out.end(), data + pos, data + pos + block_len);
        pos += block_len;
    }

    // Adler-32 checksum (big-endian)
    write_be32(out, adler32(data, len));

    return out;
}

// ============================================================
// PNG Decoder
// ============================================================

static uint32_t read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

// Inflate stored blocks only (matching our encoder)
static bool inflate_store(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out) {
    if (srcLen < 6) return false;

    // Skip zlib header (2 bytes)
    size_t pos = 2;

    while (pos < srcLen - 4) {  // Leave room for adler32
        uint8_t header = src[pos++];
        bool final = header & 1;
        int type = (header >> 1) & 3;

        if (type != 0) return false;  // Only stored blocks supported

        if (pos + 4 > srcLen) return false;
        uint16_t len = src[pos] | (src[pos+1] << 8);
        pos += 4;  // Skip LEN and NLEN

        if (pos + len > srcLen - 4) return false;
        out.insert(out.end(), src + pos, src + pos + len);
        pos += len;

        if (final) break;
    }
    return true;
}

// Paeth predictor
static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

ImageResult decodePng(const uint8_t* data, size_t size) {
    ImageResult result = {0, 0, {}};

    // Check PNG signature
    const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (size < 8 || memcmp(data, sig, 8) != 0) return result;

    size_t pos = 8;
    int width = 0, height = 0, bitDepth = 0, colorType = 0;
    std::vector<uint8_t> compressed;

    while (pos + 12 <= size) {
        uint32_t len = read_be32(data + pos);
        const uint8_t* type = data + pos + 4;
        const uint8_t* chunk = data + pos + 8;

        if (memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            width = read_be32(chunk);
            height = read_be32(chunk + 4);
            bitDepth = chunk[8];
            colorType = chunk[9];
            // Only support 8-bit RGB (type 2) or RGBA (type 6)
            if (bitDepth != 8 || (colorType != 2 && colorType != 6)) return result;
        } else if (memcmp(type, "IDAT", 4) == 0) {
            compressed.insert(compressed.end(), chunk, chunk + len);
        } else if (memcmp(type, "IEND", 4) == 0) {
            break;
        }

        pos += 12 + len;
    }

    if (width == 0 || height == 0 || compressed.empty()) return result;

    // Decompress
    std::vector<uint8_t> raw;
    if (!inflate_store(compressed.data(), compressed.size(), raw)) return result;

    int channels = (colorType == 6) ? 4 : 3;
    size_t rowBytes = width * channels;
    if (raw.size() < static_cast<size_t>(height) * (1 + rowBytes)) return result;

    // Unfilter and convert to RGB
    result.width = width;
    result.height = height;
    result.rgb.resize(width * height * 3);

    std::vector<uint8_t> prev(rowBytes, 0);
    std::vector<uint8_t> curr(rowBytes);

    for (int y = 0; y < height; y++) {
        size_t rowStart = y * (1 + rowBytes);
        uint8_t filter = raw[rowStart];
        const uint8_t* src = raw.data() + rowStart + 1;

        // Apply filter
        for (size_t x = 0; x < rowBytes; x++) {
            uint8_t a = (x >= static_cast<size_t>(channels)) ? curr[x - channels] : 0;
            uint8_t b = prev[x];
            uint8_t c = (x >= static_cast<size_t>(channels)) ? prev[x - channels] : 0;

            switch (filter) {
                case 0: curr[x] = src[x]; break;  // None
                case 1: curr[x] = src[x] + a; break;  // Sub
                case 2: curr[x] = src[x] + b; break;  // Up
                case 3: curr[x] = src[x] + ((a + b) >> 1); break;  // Average
                case 4: curr[x] = src[x] + paeth(a, b, c); break;  // Paeth
                default: return {0, 0, {}};
            }
        }

        // Copy to output (RGB only)
        uint8_t* dst = result.rgb.data() + y * width * 3;
        for (int x = 0; x < width; x++) {
            dst[x*3+0] = curr[x*channels+0];
            dst[x*3+1] = curr[x*channels+1];
            dst[x*3+2] = curr[x*channels+2];
        }

        std::swap(prev, curr);
    }

    return result;
}

// ============================================================
// PNG Encoder
// ============================================================

std::vector<uint8_t> encodePng(const uint8_t* rgb, int width, int height) {
    std::vector<uint8_t> out;

    // PNG signature
    const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);

    // IHDR chunk
    uint8_t ihdr[13];
    ihdr[0] = (width >> 24) & 0xFF;
    ihdr[1] = (width >> 16) & 0xFF;
    ihdr[2] = (width >> 8) & 0xFF;
    ihdr[3] = width & 0xFF;
    ihdr[4] = (height >> 24) & 0xFF;
    ihdr[5] = (height >> 16) & 0xFF;
    ihdr[6] = (height >> 8) & 0xFF;
    ihdr[7] = height & 0xFF;
    ihdr[8] = 8;   // bit depth
    ihdr[9] = 2;   // color type (RGB)
    ihdr[10] = 0;  // compression method (deflate)
    ihdr[11] = 0;  // filter method
    ihdr[12] = 0;  // interlace (none)
    write_chunk(out, "IHDR", ihdr, 13);

    // Prepare filtered scanlines (filter byte 0 = None for each row)
    size_t row_bytes = width * 3;
    size_t raw_size = height * (1 + row_bytes);
    std::vector<uint8_t> raw(raw_size);

    for (int y = 0; y < height; y++) {
        raw[y * (1 + row_bytes)] = 0;  // filter byte = None
        memcpy(&raw[y * (1 + row_bytes) + 1], &rgb[y * row_bytes], row_bytes);
    }

    // Compress with store-only DEFLATE
    std::vector<uint8_t> compressed = deflate_store(raw.data(), raw.size());

    // IDAT chunk
    write_chunk(out, "IDAT", compressed.data(), compressed.size());

    // IEND chunk
    write_chunk(out, "IEND", nullptr, 0);

    return out;
}

} // namespace pipe
