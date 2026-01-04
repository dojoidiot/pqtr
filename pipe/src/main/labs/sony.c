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
#include "cameras.h"

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

/* ============================================================================
   Sony ARW metadata extraction - TIFF + MakerNotes parsing

   Copied from pipe/src/main/flow/sony/prepare.cpp (git 224c1aa^)
   Extracts values needed for decoding and PipeState initialization.
   ============================================================================ */

/* Picture profile settings read from RAW MakerNotes */
typedef struct {
    int creative_style;      /* 0=Standard, 1=Vivid, 2=Neutral, 3=Portrait, etc. */
    int picture_profile;     /* Raw value from tag 0x0237 */
    float saturation;        /* Derived saturation boost (-1 to +1) */
    float vibrance;          /* Derived vibrance boost (-1 to +1) */
    float contrast;          /* Derived contrast adjustment */
} PictureProfile;

typedef struct {
    int width;
    int height;
    int strip_offset;
    uint16_t sony_curve[4];  /* Raw 16-bit values (decoder shifts >> 2 internally) */
    int black_level;
    int white_level;
    float wb_rggb[4];        /* WB multipliers R,G,B,G2 normalized to G=1.0 */
    float color_matrix[9];   /* Camera -> XYZ 3x3 matrix, scale 1/1024 (from embedded) */
    uint32_t filters;        /* Bayer pattern code */
    float exposure_bias;     /* Reserved for XMP/style exposure value (default 0.0) */
    float xyz_to_cam[9];     /* XYZ->CAM matrix from cameras.xml (scaled by 10000) */
    float d65_coeffs[4];     /* D65 WB multipliers computed from xyz_to_cam */
    const CameraData* camera; /* Pointer to camera database entry (includes style) */
    PictureProfile profile;  /* Picture profile from RAW (saturation, vibrance, etc.) */
} SonyARWMeta;

/* Read uint16/32 little-endian */
static inline uint16_t meta_read_u16(const uint8_t* p) {
    return p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t meta_read_u32(const uint8_t* p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* IFD entry */
typedef struct {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_offset;
} IFDEntry;

static IFDEntry parse_ifd_entry(const uint8_t* data) {
    IFDEntry e;
    e.tag = meta_read_u16(data);
    e.type = meta_read_u16(data + 2);
    e.count = meta_read_u32(data + 4);
    e.value_offset = meta_read_u32(data + 8);
    return e;
}

/* Map Sony Picture Profile (from tag 0x0237) to saturation/vibrance values

   Picture Profile values (from exiftool Sony.pm):
   0 = Off (use Creative Style)
   1 = Gamma Still - Portrait
   2 = Gamma Still - Standard/Neutral (PP2)
   3 = Gamma Still - Cinema/Neutral (PP3)
   4 = Gamma Cine1 - S-Gamut (PP4)
   5 = Gamma Cine2 - S-Gamut (PP5)
   6 = Gamma Cine1 - Cinema (PP6)
   7 = S-Log2 - S-Gamut (PP7)
   8 = Gamma Still - Vivid
   9 = S-Log3 - S-Gamut3.Cine (PP9)
   10 = S-Log3 - S-Gamut3 (PP10)
   ... and more

   Creative Style (tag 0xb020, string):
   Standard, Vivid, Neutral, Clear, Deep, Light, Portrait, Landscape, etc.

   We prioritize picture_profile over creative_style when available. */
static void map_picture_profile(PictureProfile* p)
{
    /* First check picture_profile (more specific) */
    if (p->picture_profile != 0) {
        switch (p->picture_profile) {
            case 8:  /* Gamma Still - Vivid */
                p->saturation = 1.0f;    /* Strong saturation boost (tuned to match camera JPEG) */
                p->vibrance = 0.50f;     /* Boost greens/blues */
                p->contrast = 0.10f;
                return;
            case 1:  /* Gamma Still - Portrait */
                p->saturation = 0.0f;
                p->vibrance = 0.10f;
                p->contrast = 0.0f;
                return;
            case 2:  /* Gamma Still - Standard/Neutral */
            case 3:  /* Gamma Still - Cinema/Neutral */
                p->saturation = -0.10f;
                p->vibrance = 0.0f;
                p->contrast = 0.0f;
                return;
            case 4:  /* Cine1 - log gamma, needs different handling */
            case 5:  /* Cine2 */
            case 6:  /* Cinema */
            case 7:  /* S-Log2 */
            case 9:  /* S-Log3 - S-Gamut3.Cine */
            case 10: /* S-Log3 - S-Gamut3 */
                /* Log profiles should be left neutral - they need different processing */
                p->saturation = 0.0f;
                p->vibrance = 0.0f;
                p->contrast = 0.0f;
                return;
            default:
                /* Unknown picture profile, fall through to creative_style */
                break;
        }
    }

    /* Fall back to creative_style */
    switch (p->creative_style) {
        case 1:  /* Vivid - strong saturation boost */
            p->saturation = 0.25f;
            p->vibrance = 0.20f;
            p->contrast = 0.10f;
            break;
        case 2:  /* Neutral - reduced saturation */
            p->saturation = -0.15f;
            p->vibrance = 0.0f;
            p->contrast = -0.10f;
            break;
        case 3:  /* Clear - slight boost */
            p->saturation = 0.10f;
            p->vibrance = 0.15f;
            p->contrast = 0.05f;
            break;
        case 4:  /* Deep - emphasis on dark tones */
            p->saturation = 0.15f;
            p->vibrance = 0.10f;
            p->contrast = 0.15f;
            break;
        case 5:  /* Light - bright, airy */
            p->saturation = 0.05f;
            p->vibrance = 0.10f;
            p->contrast = -0.05f;
            break;
        case 6:  /* Portrait - skin-friendly */
            p->saturation = 0.0f;
            p->vibrance = 0.10f;
            p->contrast = 0.0f;
            break;
        case 7:  /* Landscape - boosted greens/blues */
            p->saturation = 0.20f;
            p->vibrance = 0.15f;
            p->contrast = 0.10f;
            break;
        case 0:  /* Standard - neutral */
        default:
            p->saturation = 0.0f;
            p->vibrance = 0.0f;
            p->contrast = 0.0f;
            break;
    }
}

/* Sony SR2SubIFD decryption (Dave Coffin's algorithm from dcraw) */
static void decrypt_sr2(uint8_t* data, uint32_t length, uint32_t key)
{
    uint32_t pad[128] = {0};
    uint32_t p;

    for (p = 0; p < 4; p++)
        pad[p] = key = key * 48828125 + 1;
    pad[3] = pad[3] << 1 | (pad[0] ^ pad[2]) >> 31;
    for (p = 4; p < 127; p++)
        pad[p] = (pad[p-4] ^ pad[p-2]) << 1 | (pad[p-3] ^ pad[p-1]) >> 31;
    for (p = 0; p < 127; p++)
        pad[p] = ((pad[p] & 0xff) << 24) | ((pad[p] & 0xff00) << 8) |
                 ((pad[p] >> 8) & 0xff00) | ((pad[p] >> 24) & 0xff);

    uint32_t* d = (uint32_t*)data;
    p = 127;
    for (uint32_t i = 0; i < length / 4; i++) {
        p++;
        d[i] ^= pad[(p-1) & 127] = pad[p & 127] ^ pad[(p+64) & 127];
    }
}

/* Extract metadata from Sony ARW file */
int sony_arw_read_meta(const char* filename, SonyARWMeta* meta)
{
    memset(meta, 0, sizeof(*meta));

    /* Defaults for Sony ARW (fallback if camera not in database) */
    meta->black_level = 512;
    meta->white_level = 16383;  /* 14-bit max */
    meta->wb_rggb[0] = 1.0f;
    meta->wb_rggb[1] = 1.0f;
    meta->wb_rggb[2] = 1.0f;
    meta->wb_rggb[3] = 1.0f;
    meta->filters = 0x94949494;  /* RGGB default */
    meta->exposure_bias = 0.0f;  /* DT module default - no exposure compensation */

    /* Initialize picture profile defaults (neutral) */
    meta->profile.creative_style = 0;  /* Standard */
    meta->profile.picture_profile = 0;
    meta->profile.saturation = 0.0f;
    meta->profile.vibrance = 0.0f;
    meta->profile.contrast = 0.0f;

    /* Lookup camera in database - will be updated with actual model after parsing */
    const CameraData* cam = cameras_lookup("Sony", "ILCE-7M3");
    meta->camera = cam;  /* Store camera pointer for style access */
    if (cam) {
        memcpy(meta->xyz_to_cam, cam->xyz_to_cam, sizeof(meta->xyz_to_cam));
        meta->black_level = cam->black_level;
        meta->white_level = cam->white_level;
        meta->filters = cam->filters;
        cameras_compute_d65(cam->xyz_to_cam, meta->d65_coeffs);
    } else {
        /* Fallback: identity-ish D65 coefficients */
        meta->d65_coeffs[0] = 1.0f;
        meta->d65_coeffs[1] = 1.0f;
        meta->d65_coeffs[2] = 1.0f;
        meta->d65_coeffs[3] = 1.0f;
    }

    FILE* f = fopen(filename, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read first 1MB for metadata */
    size_t read_size = (file_size < 1024*1024) ? file_size : 1024*1024;
    uint8_t* data = (uint8_t*)malloc(read_size);
    if (!data) { fclose(f); return -1; }

    if (fread(data, 1, read_size, f) != read_size) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Check TIFF header */
    if (read_size < 8 || data[0] != 'I' || data[1] != 'I' ||
        data[2] != 0x2a || data[3] != 0x00) {
        free(data);
        return -1;
    }

    uint32_t ifd0_offset = meta_read_u32(data + 4);
    if (ifd0_offset + 2 > read_size) { free(data); return -1; }

    /* Parse IFD0 */
    uint16_t nentries = meta_read_u16(data + ifd0_offset);
    uint32_t offset = ifd0_offset + 2;

    uint32_t sub_ifd_offset = 0;
    uint32_t exif_ifd_offset = 0;
    uint32_t sr2_offset = 0, sr2_length = 0, sr2_key = 0;

    for (int i = 0; i < nentries; i++) {
        if (offset + 12 > read_size) break;
        IFDEntry entry = parse_ifd_entry(data + offset);

        /* Tag 0xc634 contains SR2SubIFD pointers */
        if (entry.tag == 0xc634 && entry.value_offset + 100 <= read_size) {
            uint16_t sr2_num = meta_read_u16(data + entry.value_offset);
            for (int j = 0; j < sr2_num && j < 20; j++) {
                uint32_t sr2_entry_off = entry.value_offset + 2 + j * 12;
                if (sr2_entry_off + 12 > read_size) break;
                IFDEntry se = parse_ifd_entry(data + sr2_entry_off);
                if (se.tag == 0x7200) sr2_offset = se.value_offset;
                if (se.tag == 0x7201) sr2_length = se.value_offset;
                if (se.tag == 0x7221) sr2_key = se.value_offset;
            }
        }

        if (entry.tag == 330) sub_ifd_offset = entry.value_offset;
        else if (entry.tag == 34665) exif_ifd_offset = entry.value_offset;

        offset += 12;
    }

    /* Parse EXIF IFD for MakerNotes */
    uint32_t maker_note_offset = 0;
    if (exif_ifd_offset > 0 && exif_ifd_offset + 2 <= read_size) {
        uint16_t exif_nentries = meta_read_u16(data + exif_ifd_offset);
        offset = exif_ifd_offset + 2;

        for (int i = 0; i < exif_nentries; i++) {
            if (offset + 12 > read_size) break;
            IFDEntry entry = parse_ifd_entry(data + offset);
            if (entry.tag == 37500) maker_note_offset = entry.value_offset;
            offset += 12;
        }
    }

    /* Parse Sony MakerNotes for tone curve */
    int found_sony_curve = 0;
    if (maker_note_offset > 0 && maker_note_offset + 12 <= read_size) {
        uint32_t maker_ifd_offset = maker_note_offset;
        /* Skip "SONY DSC " header if present */
        if (data[maker_note_offset] == 'S' && data[maker_note_offset + 1] == 'O')
            maker_ifd_offset += 12;

        if (maker_ifd_offset + 2 <= read_size) {
            uint16_t maker_nentries = meta_read_u16(data + maker_ifd_offset);
            uint32_t sony_tag2010_offset = 0;

            for (int i = 0; i < maker_nentries && i < 200; i++) {
                uint32_t entry_off = maker_ifd_offset + 2 + i * 12;
                if (entry_off + 12 > read_size) break;
                IFDEntry entry = parse_ifd_entry(data + entry_off);

                if (entry.tag == 0x2010)
                    sony_tag2010_offset = entry.value_offset;

                /* Creative Style: tag 0xb020 (16-byte string: "Standard", "Vivid", etc.) */
                if (entry.tag == 0xb020 && entry.count >= 1) {
                    uint32_t str_offset = entry.value_offset;
                    if (entry.count <= 4) {
                        /* Small value stored in value_offset itself */
                        /* Not expected for 16-byte string */
                    } else if (str_offset + entry.count <= read_size) {
                        /* String at offset - parse first word to identify style */
                        char style_str[20] = {0};
                        int len = (entry.count < 19) ? entry.count : 19;
                        memcpy(style_str, data + str_offset, len);

                        /* Map string to enum */
                        if (strstr(style_str, "Vivid")) meta->profile.creative_style = 1;
                        else if (strstr(style_str, "Neutral")) meta->profile.creative_style = 2;
                        else if (strstr(style_str, "Clear")) meta->profile.creative_style = 3;
                        else if (strstr(style_str, "Deep")) meta->profile.creative_style = 4;
                        else if (strstr(style_str, "Light")) meta->profile.creative_style = 5;
                        else if (strstr(style_str, "Portrait")) meta->profile.creative_style = 6;
                        else if (strstr(style_str, "Landscape")) meta->profile.creative_style = 7;
                        else if (strstr(style_str, "Standard")) meta->profile.creative_style = 0;
                        else meta->profile.creative_style = 0; /* Default to Standard */
                    }
                }
            }

            /* Parse tag 0x2010 sub-IFD for tone curve (0x7010) - note: tag 0x2010 is encrypted,
               so we use exiftool for PictureProfile instead */
            if (sony_tag2010_offset > 0 && sony_tag2010_offset + 2 <= read_size) {
                uint16_t tag2010_nentries = meta_read_u16(data + sony_tag2010_offset);

                for (int i = 0; i < tag2010_nentries && i < 100; i++) {
                    uint32_t entry_off = sony_tag2010_offset + 2 + i * 12;
                    if (entry_off + 12 > read_size) break;
                    IFDEntry entry = parse_ifd_entry(data + entry_off);

                    /* SonyToneCurve: tag 0x7010, 4 uint16 values */
                    if (entry.tag == 0x7010 && entry.count >= 4 &&
                        entry.value_offset + 8 <= read_size) {
                        /* Return raw values - decoder does >> 2 internally */
                        for (int j = 0; j < 4; j++) {
                            meta->sony_curve[j] = meta_read_u16(data + entry.value_offset + j * 2);
                        }
                        found_sony_curve = 1;
                    }
                }
            }
        }
    }

    /* Parse SubIFD for dimensions, strip offset, WB, color matrix */
    if (sub_ifd_offset > 0 && sub_ifd_offset + 2 <= read_size) {
        uint16_t sub_nentries = meta_read_u16(data + sub_ifd_offset);
        offset = sub_ifd_offset + 2;

        int found_wb = 0;

        for (int i = 0; i < sub_nentries; i++) {
            if (offset + 12 > read_size) break;
            IFDEntry entry = parse_ifd_entry(data + offset);

            /* Fallback: tone curve in SubIFD */
            if (!found_sony_curve && entry.tag == 0x7010 && entry.count >= 4 &&
                entry.value_offset + 8 <= read_size) {
                for (int j = 0; j < 4; j++) {
                    meta->sony_curve[j] = meta_read_u16(data + entry.value_offset + j * 2);
                }
                found_sony_curve = 1;
            }

            /* WB RGGB levels: tag 0x7313, 4 int16 values */
            if (!found_wb && entry.tag == 0x7313 && entry.count == 4 &&
                entry.value_offset + 8 <= read_size) {
                uint16_t r = meta_read_u16(data + entry.value_offset);
                uint16_t g1 = meta_read_u16(data + entry.value_offset + 2);
                uint16_t g2 = meta_read_u16(data + entry.value_offset + 4);
                uint16_t b = meta_read_u16(data + entry.value_offset + 6);
                if (g1 > 0) {
                    meta->wb_rggb[0] = (float)r / (float)g1;
                    meta->wb_rggb[1] = 1.0f;
                    meta->wb_rggb[2] = (float)b / (float)g1;
                    meta->wb_rggb[3] = (float)g2 / (float)g1;
                }
                found_wb = 1;
            }

            /* Color matrix: tag 0x7800, 9 int16 values, scale 1/1024 */
            if (entry.tag == 0x7800 && entry.count == 9 &&
                entry.value_offset + 18 <= read_size) {
                for (int j = 0; j < 9; j++) {
                    int16_t val = (int16_t)meta_read_u16(data + entry.value_offset + j * 2);
                    meta->color_matrix[j] = val / 1024.0f;
                }
            }

            /* CFA pattern: tag 33422 */
            if (entry.tag == 33422 && entry.value_offset + 8 <= read_size) {
                uint8_t cfa[4];
                for (int j = 0; j < 4; j++)
                    cfa[j] = data[entry.value_offset + 4 + j];
                /* Map pattern to filters code */
                if (cfa[0] == 0 && cfa[1] == 1 && cfa[2] == 1 && cfa[3] == 2)
                    meta->filters = 0x94949494;  /* RGGB */
                else if (cfa[0] == 2 && cfa[1] == 1 && cfa[2] == 1 && cfa[3] == 0)
                    meta->filters = 0x16161616;  /* BGGR */
                else if (cfa[0] == 1 && cfa[1] == 0 && cfa[2] == 2 && cfa[3] == 1)
                    meta->filters = 0x61616161;  /* GRBG */
                else if (cfa[0] == 1 && cfa[1] == 2 && cfa[2] == 0 && cfa[3] == 1)
                    meta->filters = 0x49494949;  /* GBRG */
            }

            /* Image dimensions */
            if (entry.tag == 256) {
                meta->width = (entry.type == 3) ? (entry.value_offset & 0xFFFF) : entry.value_offset;
            }
            else if (entry.tag == 257) {
                meta->height = (entry.type == 3) ? (entry.value_offset & 0xFFFF) : entry.value_offset;
            }
            else if (entry.tag == 273) {
                meta->strip_offset = entry.value_offset;
            }

            offset += 12;
        }
    }

    /* Decrypt and parse SR2SubIFD for black level */
    if (sr2_offset > 0 && sr2_length > 0 && sr2_key != 0 &&
        sr2_offset + sr2_length <= read_size) {
        uint8_t* sr2 = (uint8_t*)malloc(sr2_length);
        if (sr2) {
            memcpy(sr2, data + sr2_offset, sr2_length);
            decrypt_sr2(sr2, sr2_length, sr2_key);

            if (sr2_length >= 2) {
                uint16_t sr2_nentries = meta_read_u16(sr2);
                for (int i = 0; i < sr2_nentries && i < 200; i++) {
                    uint32_t entry_off = 2 + i * 12;
                    if (entry_off + 12 > sr2_length) break;
                    IFDEntry entry = parse_ifd_entry(sr2 + entry_off);

                    /* Black level: tag 0x7310, 4 int16 values */
                    if (entry.tag == 0x7310 && entry.count == 4) {
                        uint32_t rel_offset = entry.value_offset - sr2_offset;
                        if (rel_offset + 8 <= sr2_length) {
                            uint16_t min_black = 65535;
                            for (int j = 0; j < 4; j++) {
                                uint16_t bl = meta_read_u16(sr2 + rel_offset + j * 2);
                                if (bl < min_black) min_black = bl;
                            }
                            meta->black_level = min_black;
                        }
                    }

                    /* Color matrix from SR2 if not found in SubIFD */
                    if (entry.tag == 0x7800 && entry.count == 9 && meta->color_matrix[0] == 0) {
                        uint32_t rel_offset = entry.value_offset - sr2_offset;
                        if (rel_offset + 18 <= sr2_length) {
                            for (int j = 0; j < 9; j++) {
                                int16_t val = (int16_t)meta_read_u16(sr2 + rel_offset + j * 2);
                                meta->color_matrix[j] = val / 1024.0f;
                            }
                        }
                    }
                }
            }
            free(sr2);
        }
    }

    /* Try to get PictureProfile from exiftool (tag 0x2010 is encrypted) */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "exiftool -n -PictureProfile \"%s\" 2>/dev/null | grep -oE '[0-9]+'", filename);
    FILE* pp = popen(cmd, "r");
    if (pp) {
        int pp_value = 0;
        if (fscanf(pp, "%d", &pp_value) == 1) {
            meta->profile.picture_profile = pp_value;
            printf("sony: PictureProfile from exiftool: %d\n", pp_value);
        }
        pclose(pp);
    }

    /* Map picture profile to saturation/vibrance values */
    map_picture_profile(&meta->profile);

    /* Debug output for picture profile */
    const char* pp_names[] = {
        "Off", "Portrait", "Standard/Neutral", "Cinema/Neutral",
        "Cine1-SGamut", "Cine2-SGamut", "Cine1-Cinema", "S-Log2",
        "Vivid", "S-Log3-Cine", "S-Log3"
    };
    const char* pp_name = (meta->profile.picture_profile < 11)
        ? pp_names[meta->profile.picture_profile] : "Unknown";
    printf("sony: Picture Profile: pp=%d (%s), sat=%.2f, vib=%.2f\n",
           meta->profile.picture_profile, pp_name,
           meta->profile.saturation, meta->profile.vibrance);

    free(data);
    return 0;
}
