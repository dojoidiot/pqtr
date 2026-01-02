// PngTail.cpp - labs::Tail implementation for PNG encoding
//
// Usage: auto tail = std::make_unique<PngTail>();

#include "labs.hpp"
#include <vector>
#include <cstdint>
#include <cstring>
#include <zlib.h>

using namespace pqtr;

namespace {

// ============================================================
// CRC32
// ============================================================

static uint32_t crc_table[256];
static bool crc_table_init = false;

static void init_crc_table()
{
    if (crc_table_init) return;
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        crc_table[n] = c;
    }
    crc_table_init = true;
}

static uint32_t crc32(const uint8_t *data, size_t len)
{
    init_crc_table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

// ============================================================
// Adler-32
// ============================================================

static uint32_t adler32(const uint8_t *data, size_t len)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

// ============================================================
// Helpers
// ============================================================

static void write_be32(std::vector<uint8_t> &out, uint32_t val)
{
    out.push_back((val >> 24) & 0xFF);
    out.push_back((val >> 16) & 0xFF);
    out.push_back((val >> 8) & 0xFF);
    out.push_back(val & 0xFF);
}

static void write_le16(std::vector<uint8_t> &out, uint16_t val)
{
    out.push_back(val & 0xFF);
    out.push_back((val >> 8) & 0xFF);
}

static void write_chunk(std::vector<uint8_t> &out, const char *type, const uint8_t *data, size_t len)
{
    write_be32(out, static_cast<uint32_t>(len));
    size_t crc_start = out.size();
    out.insert(out.end(), type, type + 4);
    if (data && len > 0)
        out.insert(out.end(), data, data + len);
    write_be32(out, crc32(&out[crc_start], 4 + len));
}

// ============================================================
// DEFLATE store (no compression)
// ============================================================

static std::vector<uint8_t> deflate_store(const uint8_t *data, size_t len)
{
    std::vector<uint8_t> out;
    out.push_back(0x78);
    out.push_back(0x01);

    const size_t BLOCK_MAX = 65535;
    size_t pos = 0;

    while (pos < len) {
        size_t block_len = (len - pos > BLOCK_MAX) ? BLOCK_MAX : (len - pos);
        bool final = (pos + block_len >= len);

        out.push_back(final ? 0x01 : 0x00);
        uint16_t blen = static_cast<uint16_t>(block_len);
        write_le16(out, blen);
        write_le16(out, ~blen);

        out.insert(out.end(), data + pos, data + pos + block_len);
        pos += block_len;
    }

    write_be32(out, adler32(data, len));
    return out;
}

// ============================================================
// PNG Encoder
// ============================================================

std::vector<uint8_t> encodePng(const uint8_t *rgb, int width, int height)
{
    std::vector<uint8_t> out;

    const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);

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
    ihdr[10] = 0;  // compression
    ihdr[11] = 0;  // filter
    ihdr[12] = 0;  // interlace
    write_chunk(out, "IHDR", ihdr, 13);

    size_t row_bytes = width * 3;
    size_t raw_size = height * (1 + row_bytes);
    std::vector<uint8_t> raw(raw_size);

    for (int y = 0; y < height; y++) {
        raw[y * (1 + row_bytes)] = 0;  // filter byte
        memcpy(&raw[y * (1 + row_bytes) + 1], &rgb[y * row_bytes], row_bytes);
    }

    std::vector<uint8_t> compressed = deflate_store(raw.data(), raw.size());
    write_chunk(out, "IDAT", compressed.data(), compressed.size());
    write_chunk(out, "IEND", nullptr, 0);

    return out;
}

} // anonymous namespace

// ============================================================================
// PngTail - PNG encoder plugin
// ============================================================================

class PngTail : public Tail
{
    std::vector<uint8_t> output_;

public:
    void* save(Flow& flow) override
    {
        auto& info = flow.flow();
        int width = static_cast<int>(info.leaf(WIDTH).dial());
        int height = static_cast<int>(info.leaf(HEIGHT).dial());

        if (width <= 0 || height <= 0) {
            return nullptr;
        }

        // Get data as uint8_t RGB (assumes upstream converted to display format)
        uint8_t* src = static_cast<uint8_t*>(flow.data());
        if (!src) {
            // No data - create black image for testing
            size_t pixelCount = static_cast<size_t>(width) * height * 3;
            std::vector<uint8_t> black(pixelCount, 0);
            output_ = encodePng(black.data(), width, height);
            return output_.data();
        }

        output_ = encodePng(src, width, height);
        return output_.data();
    }

    size_t size() const { return output_.size(); }
};

// Factory function
std::unique_ptr<Tail> makePngTail()
{
    return std::make_unique<PngTail>();
}
