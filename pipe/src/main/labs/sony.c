/*
    Sony ARW2 Decoder - EXACT COPY of DarktTable/RawSpeed code

    Sources copied from:
    - rawspeed/src/librawspeed/bitstreams/BitStream.h (BitStreamCacheLeftInRightOut)
    - rawspeed/src/librawspeed/bitstreams/BitStreamer.h (fill, getBits, peekBits)
    - rawspeed/src/librawspeed/adt/Bit.h (extractLowBits)
    - rawspeed/src/librawspeed/common/TableLookUp.cpp (setTable)
    - rawspeed/src/librawspeed/common/RawImage.h (setWithLookUp)
    - rawspeed/src/librawspeed/decoders/ArwDecoder.cpp (decodeCurve)
    - rawspeed/src/librawspeed/decompressors/SonyArw2Decompressor.cpp (decompressRow)
    - darktable/src/common/pfm.c (dt_write_pfm - row reversal)
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
   From rawspeed/src/librawspeed/adt/Bit.h lines 96-106
   extractLowBits
   ============================================================================ */

static inline uint32_t extractLowBits(uint32_t value, unsigned nBits) {
    // invariant(nBits >= 0);
    // invariant(nBits != 0);             // Would result in out-of-bound shift.
    // invariant(nBits <= bitwidth<T>()); // No-op is fine.
    unsigned numHighPaddingBits = 32 - nBits;
    // invariant(numHighPaddingBits >= 0);
    // invariant(numHighPaddingBits < bitwidth<T>()); // Shift is in-bounds.
    value <<= numHighPaddingBits;
    value >>= numHighPaddingBits;
    return value;
}

/* ============================================================================
   From rawspeed/src/librawspeed/bitstreams/BitStream.h lines 41-90
   BitStreamCacheBase + BitStreamCacheLeftInRightOut
   ============================================================================ */

typedef struct {
    uint64_t cache;    // the actual bits stored in the cache
    int fillLevel;     // bits left in cache
    // static constexpr int Size = 64;
    // static constexpr int MaxGetBits = 32;
} BitStreamCache;

static inline void cache_push(BitStreamCache* c, uint64_t bits, int count) {
    // cache |= bits << fillLevel;
    // fillLevel += count;
    c->cache |= bits << c->fillLevel;
    c->fillLevel += count;
}

static inline uint32_t cache_peek(BitStreamCache* c, int count) {
    // return extractLowBits(static_cast<uint32_t>(cache), count);
    return extractLowBits((uint32_t)c->cache, count);
}

static inline void cache_skip(BitStreamCache* c, int count) {
    // cache >>= count;
    // fillLevel -= count;
    c->cache >>= count;
    c->fillLevel -= count;
}

/* ============================================================================
   BitStreamerLSB - simplified from rawspeed/src/librawspeed/bitstreams/BitStreamer.h
   ============================================================================ */

typedef struct {
    const uint8_t* data;
    int size;
    int pos;
    BitStreamCache cache;
} BitStreamerLSB;

static void BitStreamerLSB_init(BitStreamerLSB* bs, const uint8_t* data, int size) {
    bs->data = data;
    bs->size = size;
    bs->pos = 0;
    bs->cache.cache = 0;
    bs->cache.fillLevel = 0;
}

/* From BitStreamer.h fillCache - lines 155-182
   For LSB: ChunkType = uint32_t, ChunkEndianness = little, MaxProcessBytes = 4 */
static void BitStreamerLSB_fill(BitStreamerLSB* bs, int nbits) {
    // if (cache.fillLevel >= nbits) return;
    if (bs->cache.fillLevel >= nbits)
        return;

    // Read 4 bytes at a time, little-endian
    while (bs->cache.fillLevel < nbits && bs->pos + 4 <= bs->size) {
        // auto chunk = getByteSwapped<uint32_t>(input, false);  // little-endian, no swap needed
        uint32_t chunk = bs->data[bs->pos] |
                        ((uint32_t)bs->data[bs->pos + 1] << 8) |
                        ((uint32_t)bs->data[bs->pos + 2] << 16) |
                        ((uint32_t)bs->data[bs->pos + 3] << 24);
        // cache.push(chunk, 32);
        cache_push(&bs->cache, chunk, 32);
        bs->pos += 4;
    }
}

/* From BitStreamer.h peekBits - lines 279-286 */
static uint32_t BitStreamerLSB_peekBits(BitStreamerLSB* bs, int nbits) {
    // fill(nbits);
    // return peekBitsNoFill(nbits);
    BitStreamerLSB_fill(bs, nbits);
    return cache_peek(&bs->cache, nbits);
}

/* From BitStreamer.h getBits - lines 294-301 */
static uint32_t BitStreamerLSB_getBits(BitStreamerLSB* bs, int nbits) {
    // fill(nbits);
    // return getBitsNoFill(nbits);
    BitStreamerLSB_fill(bs, nbits);
    uint32_t ret = cache_peek(&bs->cache, nbits);
    cache_skip(&bs->cache, nbits);
    return ret;
}

/* ============================================================================
   From rawspeed/src/librawspeed/common/TableLookUp.cpp lines 37-85
   ============================================================================ */

// constexpr int TABLE_MAX_ELTS = std::numeric_limits<uint16_t>::max() + 1;  // 65536
// constexpr int TABLE_SIZE = TABLE_MAX_ELTS * 2;  // 131072
#define TABLE_MAX_ELTS 65536
#define TABLE_SIZE (TABLE_MAX_ELTS * 2)

typedef struct {
    uint16_t* tables;
    int ntables;
    int dither;
} TableLookUp;

static inline int clampBits(int value, int bits) {
    int max_val = (1 << bits) - 1;
    if (value < 0) return 0;
    if (value > max_val) return max_val;
    return value;
}

/* TableLookUp::setTable - lines 50-85 */
static void TableLookUp_setTable(TableLookUp* lut, int ntable, const uint16_t* table, int nfilled) {
    // auto t = Array2DRef(tables.data(), TABLE_SIZE, ntables)[ntable];
    uint16_t* t = lut->tables + ntable * TABLE_SIZE;

    if (!lut->dither) {
        for (int i = 0; i < TABLE_MAX_ELTS; i++) {
            t[i] = (i < nfilled) ? table[i] : table[nfilled - 1];
        }
        return;
    }

    for (int i = 0; i < nfilled; i++) {
        int center = table[i];
        int lower = i > 0 ? table[i - 1] : center;
        int upper = i < (nfilled - 1) ? table[i + 1] : center;
        // Non-monotonic LUT handling: don't interpolate across the cross-over.
        // lower = std::min(lower, center);
        // upper = std::max(upper, center);
        if (lower > center) lower = center;
        if (upper < center) upper = center;
        int delta = upper - lower;
        // invariant(delta >= 0);
        // t(i * 2) = clampBits(center - ((upper - lower + 2) / 4), 16);
        // t((i * 2) + 1) = implicit_cast<uint16_t>(delta);
        t[i * 2] = (uint16_t)clampBits(center - ((upper - lower + 2) / 4), 16);
        t[(i * 2) + 1] = (uint16_t)delta;
    }

    for (int i = nfilled; i < TABLE_MAX_ELTS; i++) {
        t[i * 2] = table[nfilled - 1];
        t[(i * 2) + 1] = 0;
    }
}

/* ============================================================================
   From rawspeed/src/librawspeed/common/RawImage.h lines 345-363
   RawImageDataU16::setWithLookUp
   ============================================================================ */

static inline void setWithLookUp(TableLookUp* table, uint16_t value, uint16_t* dest, uint32_t* random) {
    // if (table == nullptr) { *dest = value; return; }
    if (table == NULL) {
        *dest = value;
        return;
    }
    if (table->dither) {
        // uint32_t base = table->tables[(2 * value) + 0];
        // uint32_t delta = table->tables[(2 * value) + 1];
        // uint32_t r = *random;
        // uint32_t pix = base + ((delta * (r & 2047) + 1024) >> 12);
        // *random = 15700 * (r & 65535) + (r >> 16);
        // *dest = implicit_cast<uint16_t>(pix);
        uint32_t base = table->tables[(2 * value) + 0];
        uint32_t delta = table->tables[(2 * value) + 1];
        uint32_t r = *random;
        uint32_t pix = base + ((delta * (r & 2047) + 1024) >> 12);
        *random = 15700 * (r & 65535) + (r >> 16);
        *dest = (uint16_t)pix;
        return;
    }
    // *dest = table->tables[value];
    *dest = table->tables[value];
}

/* ============================================================================
   From rawspeed/src/librawspeed/decoders/ArwDecoder.cpp lines 147-163
   ArwDecoder::decodeCurve
   ============================================================================ */

static void decodeCurve(const uint16_t* sony_curve_raw, uint16_t* curve) {
    // std::vector<uint16_t> curve(0x4001);
    // std::array<uint32_t, 6> sony_curve = {{0, 0, 0, 0, 0, 4095}};
    uint32_t sony_curve[6] = {0, 0, 0, 0, 0, 4095};

    // for (uint32_t i = 0; i < 4; i++)
    //     sony_curve[i + 1] = (c->getU16(i) >> 2) & 0xfff;
    for (uint32_t i = 0; i < 4; i++)
        sony_curve[i + 1] = (sony_curve_raw[i] >> 2) & 0xfff;

    // for (uint32_t i = 0; i < 0x4001; i++)
    //     curve[i] = implicit_cast<uint16_t>(i);
    for (uint32_t i = 0; i < 0x4001; i++)
        curve[i] = (uint16_t)i;

    // for (uint32_t i = 0; i < 5; i++)
    //     for (uint32_t j = sony_curve[i] + 1; j <= sony_curve[i + 1]; j++)
    //         curve[j] = implicit_cast<uint16_t>(curve[j - 1] + (1 << i));
    for (uint32_t i = 0; i < 5; i++)
        for (uint32_t j = sony_curve[i] + 1; j <= sony_curve[i + 1]; j++)
            curve[j] = (uint16_t)(curve[j - 1] + (1 << i));
}

/* ============================================================================
   From rawspeed/src/librawspeed/decompressors/SonyArw2Decompressor.cpp lines 57-110
   SonyArw2Decompressor::decompressRow
   ============================================================================ */

static void decompressRow(const uint8_t* input, int width, int row,
                          uint16_t* output, TableLookUp* table) {
    // ByteStream rowBs = input;
    // rowBs.skipBytes(row * out.width());
    // rowBs = rowBs.peekStream(out.width());
    const uint8_t* rowData = input + row * width;

    // BitStreamerLSB bits(rowBs.peekRemainingBuffer().getAsArray1DRef());
    BitStreamerLSB bits;
    BitStreamerLSB_init(&bits, rowData, width);

    // uint32_t random = bits.peekBits(24);
    uint32_t random = BitStreamerLSB_peekBits(&bits, 24);

    // Each loop iteration processes 16 pixels, consuming 128 bits of input.
    // for (int col = 0; col < out.width(); col += ((col & 1) != 0) ? 31 : 1) {
    for (int col = 0; col < width; col += ((col & 1) != 0) ? 31 : 1) {
        // 30 bits.
        // int _max = bits.getBits(11);
        // int _min = bits.getBits(11);
        // int _imax = bits.getBits(4);
        // int _imin = bits.getBits(4);
        int _max = BitStreamerLSB_getBits(&bits, 11);
        int _min = BitStreamerLSB_getBits(&bits, 11);
        int _imax = BitStreamerLSB_getBits(&bits, 4);
        int _imin = BitStreamerLSB_getBits(&bits, 4);

        // if (_imax == _imin)
        //     ThrowRDE("ARW2 invariant failed, same pixel is both min and max");
        if (_imax == _imin) {
            fprintf(stderr, "ARW2 invariant failed, same pixel is both min and max\n");
            return;
        }

        // int sh = 0;
        // while ((sh < 4) && ((0x80 << sh) <= (_max - _min)))
        //     sh++;
        int sh = 0;
        while ((sh < 4) && ((0x80 << sh) <= (_max - _min)))
            sh++;

        // for (int i = 0; i < 16; i++) {
        for (int i = 0; i < 16; i++) {
            int p;
            // if (i == _imax)
            //     p = _max;
            // else {
            //     if (i == _imin)
            //         p = _min;
            //     else {
            //         p = (bits.getBits(7) << sh) + _min;
            //         p = std::min(p, 0x7ff);
            //     }
            // }
            if (i == _imax)
                p = _max;
            else {
                if (i == _imin)
                    p = _min;
                else {
                    p = (BitStreamerLSB_getBits(&bits, 7) << sh) + _min;
                    // p = std::min(p, 0x7ff);
                    if (p > 0x7ff) p = 0x7ff;
                }
            }
            // rawdata.setWithLookUp(
            //     implicit_cast<uint16_t>(p << 1),
            //     reinterpret_cast<std::byte*>(&out(row, col + (i * 2))), &random);
            setWithLookUp(table, (uint16_t)(p << 1), &output[row * width + col + (i * 2)], &random);
        }
    }
}

/* ============================================================================
   Main decoder interface - calls decompressRow for each row
   From SonyArw2Decompressor::decompressThread lines 112-134
   ============================================================================ */

int sony_arw2_decode(
    const uint8_t* compressed_data,
    int compressed_size,
    int width,
    int height,
    const uint16_t* sony_curve_raw,  /* 4 values from TIFF tag SONYCURVE */
    uint16_t* output)
{
    /* Validate dimensions - from SonyArw2Decompressor constructor lines 48-51 */
    // if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 32 != 0 ||
    //     mRaw->dim.x > 9600 || mRaw->dim.y > 6376)
    if (width <= 0 || height <= 0 || width % 32 != 0 ||
        width > 9600 || height > 6376) {
        fprintf(stderr, "Unexpected image dimensions found: (%d; %d)\n", width, height);
        return -1;
    }

    /* Validate size: 1 byte per pixel - from line 54 */
    // input = input_.peekStream(mRaw->dim.x * mRaw->dim.y);
    if (compressed_size < width * height) {
        fprintf(stderr, "Compressed data too small: %d < %d\n", compressed_size, width * height);
        return -1;
    }

    /* Build the linearization curve - from ArwDecoder::decodeRawInternal line 236 */
    // std::vector<uint16_t> curve = decodeCurve(raw);
    uint16_t* curve = (uint16_t*)malloc(0x4001 * sizeof(uint16_t));
    if (!curve) return -1;
    decodeCurve(sony_curve_raw, curve);

    /* Set up the dither table - from RawImageCurveGuard constructor lines 377-384 */
    // RawImageCurveGuard curveHandler(&mRaw, curve, uncorrectedRawValues);
    // (*mRaw)->setTable(curve, true);  // dither = true
    TableLookUp table;
    table.ntables = 1;
    table.dither = 1;  // true
    table.tables = (uint16_t*)malloc(TABLE_SIZE * sizeof(uint16_t));
    if (!table.tables) {
        free(curve);
        return -1;
    }
    TableLookUp_setTable(&table, 0, curve, 0x4001);

    /* Decompress each row - from decompressThread lines 120-133 */
    // for (int y = 0; y < mRaw->dim.y; y++) {
    //     decompressRow(y);
    // }
    for (int row = 0; row < height; row++) {
        decompressRow(compressed_data, width, row, output, &table);
    }

    free(table.tables);
    free(curve);
    return 0;
}

/* ============================================================================
   PPM writing with row reversal
   From darktable/src/common/pfm.c lines 253-286
   dt_write_pfm for bpp == sizeof(uint16_t)
   ============================================================================ */

int sony_write_ppm(const char* filename, const uint16_t* data, int width, int height) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Cannot open %s for writing\n", filename);
        return -1;
    }

    // fprintf(f, "P5\n%d %d\n", (int)width, (int)height);
    fprintf(f, "P5\n%d %d\n", width, height);

    // for(size_t row = 0; row < height; row++)
    // {
    //     // NOTE: PFM has rows in reverse order
    //     const size_t row_in = height - 1 - row;
    //     const void *in = data + bpp * width * row_in;
    //     int cnt = fwrite(in, sizeof(uint16_t), width, f);
    //     if(cnt != width) break;
    // }
    for (int row = 0; row < height; row++) {
        // NOTE: PFM has rows in reverse order
        int row_in = height - 1 - row;
        const uint16_t* in = data + width * row_in;
        size_t cnt = fwrite(in, sizeof(uint16_t), width, f);
        if (cnt != (size_t)width) break;
    }

    fclose(f);
    return 0;
}
