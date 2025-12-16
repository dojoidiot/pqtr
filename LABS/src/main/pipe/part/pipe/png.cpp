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
