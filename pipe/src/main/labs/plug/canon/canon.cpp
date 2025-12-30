// canon.cpp
// Canon CR2 RAW decoder - clean-room from LibRaw
//
// CR2 structure:
//   - TIFF with 4 IFDs
//   - IFD[3] contains RAW as lossless JPEG
//   - Canon MakerNotes for WB, black/white levels
//
// Reference: LibRaw decoders_dcraw.cpp lossless_jpeg_load_raw()

#include "canon.h"
#include <iostream>
#include <cstring>
#include <climits>

namespace canon
{

// ============================================================================
// TIFF/CR2 parsing helpers
// ============================================================================

static uint16_t read_u16(const uint8_t* data) {
    return data[0] | (data[1] << 8);  // Little-endian
}

static uint32_t read_u32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static float read_rational(const uint8_t* data) {
    uint32_t num = read_u32(data);
    uint32_t den = read_u32(data + 4);
    return den ? static_cast<float>(num) / den : 0.0f;
}

// TIFF tag IDs
enum TIFFTag {
    TAG_IMAGE_WIDTH = 256,
    TAG_IMAGE_HEIGHT = 257,
    TAG_BITS_PER_SAMPLE = 258,
    TAG_COMPRESSION = 259,
    TAG_MAKE = 271,
    TAG_MODEL = 272,
    TAG_STRIP_OFFSETS = 273,
    TAG_ORIENTATION = 274,
    TAG_STRIP_BYTE_COUNTS = 279,
    TAG_SUB_IFD = 330,
    TAG_EXIF_IFD = 34665,
    TAG_MAKER_NOTE = 37500,

    // Canon-specific
    TAG_CANON_SENSOR_INFO = 0xe0,
    TAG_CANON_COLOR_DATA = 0x4001,
    TAG_CANON_CR2_SLICE = 0xc640,
};

struct IFDEntry {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_offset;
};

static IFDEntry parse_ifd_entry(const uint8_t* data) {
    IFDEntry e;
    e.tag = read_u16(data);
    e.type = read_u16(data + 2);
    e.count = read_u32(data + 4);
    e.value_offset = read_u32(data + 8);
    return e;
}

// Get entry value (inline if fits in 4 bytes, else offset)
static uint32_t get_entry_value(const IFDEntry& e, const uint8_t* file_data, size_t size) {
    // Type sizes: 1=byte, 2=ascii, 3=short, 4=long, 5=rational
    static const int type_size[] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};
    int tsize = (e.type < 13) ? type_size[e.type] : 0;

    if (tsize * e.count <= 4) {
        // Value is inline
        if (e.type == 3) return e.value_offset & 0xFFFF;  // SHORT
        return e.value_offset;
    }
    // Value is at offset
    if (e.value_offset + tsize * e.count > size) return 0;
    if (e.type == 3) return read_u16(file_data + e.value_offset);
    if (e.type == 4) return read_u32(file_data + e.value_offset);
    return e.value_offset;
}

static std::string get_entry_string(const IFDEntry& e, const uint8_t* file_data, size_t size) {
    if (e.type != 2) return "";
    uint32_t offset = (e.count <= 4) ? 8 : e.value_offset;  // Inline or at offset
    if (e.count > 4) offset = e.value_offset;
    if (offset + e.count > size) return "";

    // For inline strings, we need to get bytes from value_offset field
    if (e.count <= 4) {
        char buf[5] = {0};
        memcpy(buf, &e.value_offset, 4);
        return std::string(buf);
    }
    return std::string(reinterpret_cast<const char*>(file_data + offset), e.count - 1);
}

// ============================================================================
// Lossless JPEG decoder (from LibRaw dcraw.c)
// ============================================================================

// Lossless JPEG header
struct JHead {
    int bits;       // Precision (typically 14 for CR2)
    int high;       // Height
    int wide;       // Width
    int clrs;       // Components
    int psv;        // Predictor selection value
    int restart;    // Restart interval

    uint16_t* huff[4];  // Huffman tables (huff[0] = max bits, huff[c] = (len<<8)|symbol)
    uint16_t* row;      // Row buffer
    int vpred[6];       // Vertical predictors

    JHead() : bits(0), high(0), wide(0), clrs(0), psv(1), restart(0), row(nullptr) {
        memset(huff, 0, sizeof(huff));
        memset(vpred, 0, sizeof(vpred));
    }
    ~JHead() {
        for (int c = 0; c < 4; c++) free(huff[c]);
        free(row);
    }
};

// Bit reading state (from dcraw getbithuff)
struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t pos;
    uint32_t bitbuf;
    int vbits;

    BitReader(const uint8_t* d, size_t s) : data(d), size(s), pos(0), bitbuf(0), vbits(0) {}

    // Get n bits from stream (getbits from dcraw)
    uint32_t getbits(int n) {
        if (n == 0) return 0;
        if (n < 0) {  // Reset
            bitbuf = 0;
            vbits = 0;
            return 0;
        }
        while (vbits < n && pos < size) {
            uint8_t c = data[pos++];
            // zero_after_ff: skip 0x00 after 0xff
            if (c == 0xff && pos < size && data[pos] == 0x00)
                pos++;
            bitbuf = (bitbuf << 8) | c;
            vbits += 8;
        }
        // Extract top n bits (dcraw formula: bitbuf << (32-vbits) >> (32-nbits))
        uint32_t result = (bitbuf << (32 - vbits)) >> (32 - n);
        vbits -= n;
        return result;
    }

    // Decode Huffman symbol (getbithuff from dcraw)
    // Note: dcraw does gethuff(h) = getbithuff(*h, h+1), so table lookup uses h+1
    int gethuff(uint16_t* huff) {
        if (!huff) return 0;
        int nbits = huff[0];  // Max bits at huff[0]
        if (nbits > 25 || vbits < 0) return 0;

        // Fill bit buffer to have at least nbits
        while (vbits < nbits && pos < size) {
            uint8_t c = data[pos++];
            if (c == 0xff && pos < size && data[pos] == 0x00)
                pos++;
            bitbuf = (bitbuf << 8) | c;
            vbits += 8;
        }

        // Peek top nbits from buffer
        uint32_t c = (bitbuf << (32 - vbits)) >> (32 - nbits);

        // Look up in table (offset by 1 because huff[0] is max bits)
        // dcraw: huff = h+1, so huff[c] = h[c+1]
        vbits -= huff[c + 1] >> 8;  // Consume actual bit length
        return huff[c + 1] & 0xff;   // Return symbol
    }
};

// Build Huffman table from DHT data (from LibRaw make_decoder_ref)
static uint16_t* make_decoder(const uint8_t* source) {
    // First 16 bytes are counts for each code length
    const uint8_t* count = source;

    // Find max code length
    int max = 16;
    while (max > 0 && !count[max - 1]) max--;
    if (max == 0) return nullptr;

    uint16_t* huff = (uint16_t*)calloc(1 + (1 << max), sizeof(uint16_t));
    if (!huff) return nullptr;

    huff[0] = max;

    const uint8_t* symbols = source + 16;
    int h = 1;

    for (int len = 1; len <= max; len++) {
        for (int i = 0; i < count[len - 1]; i++) {
            // Fill (1 << (max-len)) entries with (len << 8 | symbol)
            for (int j = 0; j < (1 << (max - len)); j++) {
                if (h <= (1 << max))
                    huff[h++] = (len << 8) | *symbols;
            }
            symbols++;
        }
    }

    return huff;
}

// Get lossless JPEG difference value (from LibRaw ljpeg_diff)
static int ljpeg_diff(BitReader& br, uint16_t* huff) {
    int len = br.gethuff(huff);
    if (len == 16) return -32768;
    if (len == 0) return 0;

    int diff = br.getbits(len);
    // Sign extend
    if ((diff & (1 << (len - 1))) == 0)
        diff -= (1 << len) - 1;
    return diff;
}

// Parse lossless JPEG header, returns scan data offset
static size_t ljpeg_start(JHead& jh, const uint8_t* data, size_t size) {
    if (size < 2 || data[0] != 0xff || data[1] != 0xd8)
        return 0;

    size_t pos = 2;
    jh.restart = INT_MAX;

    while (pos + 4 <= size) {
        if (data[pos] != 0xff) { pos++; continue; }

        uint8_t marker = data[pos + 1];
        uint16_t len = (data[pos + 2] << 8) | data[pos + 3];
        pos += 2;

        if (pos + len > size) break;
        const uint8_t* seg = data + pos + 2;

        switch (0xff00 | marker) {
        case 0xffc0:  // SOF0 - baseline
        case 0xffc1:  // SOF1 - extended sequential
        case 0xffc3:  // SOF3 - lossless
            jh.bits = seg[0];
            jh.high = (seg[1] << 8) | seg[2];
            jh.wide = (seg[3] << 8) | seg[4];
            jh.clrs = seg[5];
            break;

        case 0xffc4:  // DHT - define Huffman table
            {
                const uint8_t* dp = seg;
                while (dp < seg + len - 2) {
                    int tc = (*dp >> 4) & 0x0f;  // Table class (0=DC, 1=AC)
                    int th = *dp & 0x0f;         // Table index
                    dp++;

                    int idx = tc * 2 + th;
                    if (idx < 4) {
                        jh.huff[idx] = make_decoder(dp);
                    }

                    // Skip table data
                    int count = 0;
                    for (int i = 0; i < 16; i++) count += dp[i];
                    dp += 16 + count;
                }
            }
            break;

        case 0xffda:  // SOS - start of scan
            jh.psv = seg[1 + seg[0] * 2];
            // Return offset to scan data
            pos += len;
            goto done;

        case 0xffdd:  // DRI - restart interval
            jh.restart = (seg[0] << 8) | seg[1];
            break;
        }

        pos += len;
    }

done:
    if (jh.bits == 0 || jh.high == 0 || jh.wide == 0 || jh.clrs == 0)
        return 0;

    // Allocate row buffer (2 rows for predictor)
    jh.row = (uint16_t*)calloc(jh.wide * jh.clrs * 2, sizeof(uint16_t));
    if (!jh.row) return 0;

    return pos;  // Return offset to scan data
}

// Decode one row of lossless JPEG (from LibRaw ljpeg_row_unrolled)
static uint16_t* ljpeg_row(int jrow, JHead& jh, BitReader& br) {
    int col, c, diff, pred;
    uint16_t* row[3];

    // Handle restart markers
    if (jh.restart != INT_MAX && jrow > 0 && (jrow * jh.wide) % jh.restart == 0) {
        // Reset predictors
        for (c = 0; c < 6; c++) jh.vpred[c] = 1 << (jh.bits - 1);
        // Skip to next restart marker
        br.getbits(-1);  // Reset bit buffer
        // Find next restart marker (0xffd0-0xffd7)
        while (br.pos < br.size - 1) {
            if (br.data[br.pos] == 0xff && (br.data[br.pos+1] & 0xf8) == 0xd0) {
                br.pos += 2;
                break;
            }
            br.pos++;
        }
    }

    // Set up row pointers (current and previous)
    for (c = 0; c < 3; c++)
        row[c] = jh.row + jh.wide * jh.clrs * ((jrow + c) & 1);

    // First column: use vertical predictor
    for (c = 0; c < jh.clrs; c++) {
        diff = ljpeg_diff(br, jh.huff[c]);
        pred = (jh.vpred[c] += diff) - diff;
        *row[0]++ = static_cast<uint16_t>(pred + diff);
        row[1]++;
    }

    // Remaining columns
    if (jrow == 0) {
        // First row: predictor is always left pixel
        for (col = 1; col < jh.wide; col++) {
            for (c = 0; c < jh.clrs; c++) {
                diff = ljpeg_diff(br, jh.huff[c]);
                pred = row[0][-jh.clrs];  // Left pixel
                *row[0]++ = static_cast<uint16_t>(pred + diff);
                row[1]++;
            }
        }
    } else {
        // Use predictor selection value (PSV)
        for (col = 1; col < jh.wide; col++) {
            for (c = 0; c < jh.clrs; c++) {
                diff = ljpeg_diff(br, jh.huff[c]);
                int left = row[0][-jh.clrs];
                int above = row[1][0];
                int aboveleft = row[1][-jh.clrs];

                switch (jh.psv) {
                case 1: pred = left; break;
                case 2: pred = above; break;
                case 3: pred = aboveleft; break;
                case 4: pred = left + above - aboveleft; break;
                case 5: pred = left + ((above - aboveleft) >> 1); break;
                case 6: pred = above + ((left - aboveleft) >> 1); break;
                case 7: pred = (left + above) >> 1; break;
                default: pred = left; break;
                }

                *row[0]++ = static_cast<uint16_t>(pred + diff);
                row[1]++;
            }
        }
    }

    // Return pointer to beginning of decoded row
    return jh.row + jh.wide * jh.clrs * (jrow & 1);
}

// ============================================================================
// Canon MakerNotes parsing
// ============================================================================

// Parse Canon MakerNotes IFD to find ColorData (0x4001)
// Returns offset to ColorData or 0 if not found
static bool parse_makernotes(const uint8_t* data, size_t size,
                              uint32_t mn_offset, uint32_t /* mn_size */,
                              RawMetadata& meta)
{
    // Canon MakerNotes are in IFD format, starting at mn_offset
    if (mn_offset + 2 > size) return false;

    uint16_t num_entries = read_u16(data + mn_offset);
    if (mn_offset + 2 + num_entries * 12 > size) return false;

    for (int i = 0; i < num_entries; i++) {
        IFDEntry e = parse_ifd_entry(data + mn_offset + 2 + i * 12);

        if (e.tag == TAG_CANON_COLOR_DATA && e.type == 3) {  // SHORT array
            // ColorData found - e.count is number of shorts
            uint32_t cd_offset = e.value_offset;
            if (cd_offset + e.count * 2 > size) continue;

            const uint16_t* cd = reinterpret_cast<const uint16_t*>(data + cd_offset);

            // EOS 40D: len=692, ColorDataSubVer at [0]=3
            // WB at [63..66], black levels at [231..234]
            if (e.count >= 692) {
                // WB RGGB at offsets 63-66
                meta.wb_rggb[0] = cd[63];  // R
                meta.wb_rggb[1] = cd[64];  // G1
                meta.wb_rggb[2] = cd[65];  // G2
                meta.wb_rggb[3] = cd[66];  // B

                // Per-channel black levels at 231-234
                // For EOS 40D these are: BlackRed, BlackGreen1, BlackGreen2, BlackBlue
                uint16_t blk[4] = {cd[231], cd[232], cd[233], cd[234]};
                meta.black_level = (blk[0] + blk[1] + blk[2] + blk[3]) / 4;

                std::cout << "Canon ColorData: WB=[" << meta.wb_rggb[0] << ","
                          << meta.wb_rggb[1] << "," << meta.wb_rggb[2] << ","
                          << meta.wb_rggb[3] << "] black=" << meta.black_level << std::endl;
                return true;
            }
            // TODO: Handle other camera models with different ColorData layouts
        }
    }
    return false;
}

// Parse EXIF IFD to find MakerNote
static bool parse_exif_ifd(const uint8_t* data, size_t size,
                           uint32_t exif_offset, RawMetadata& meta)
{
    if (exif_offset + 2 > size) return false;

    uint16_t num_entries = read_u16(data + exif_offset);
    if (exif_offset + 2 + num_entries * 12 > size) return false;

    for (int i = 0; i < num_entries; i++) {
        IFDEntry e = parse_ifd_entry(data + exif_offset + 2 + i * 12);

        if (e.tag == TAG_MAKER_NOTE && e.type == 7) {  // UNDEFINED
            // MakerNote found
            return parse_makernotes(data, size, e.value_offset, e.count, meta);
        }
    }
    return false;
}

// ============================================================================
// CR2 Decoder::prepare
// ============================================================================

bool Decoder::prepare(const uint8_t* data, size_t size,
                     BayerU16& bayer, RawMetadata& metadata)
{
    if (size < 16) return false;

    // Check CR2 header: TIFF + "CR" signature at offset 8
    if (data[0] != 'I' || data[1] != 'I' || read_u16(data + 2) != 0x002A)
        return false;

    // CR2 has "CR" at offset 8
    if (data[8] != 'C' || data[9] != 'R')
        return false;

    metadata.camera_make = "Canon";

    // Parse IFD0
    uint32_t ifd_offset = read_u32(data + 4);
    if (ifd_offset + 2 > size) return false;

    // Collect IFD offsets (CR2 has 4)
    uint32_t ifd_offsets[4] = {0};
    int num_ifds = 0;

    while (ifd_offset && num_ifds < 4) {
        ifd_offsets[num_ifds++] = ifd_offset;

        uint16_t num_entries = read_u16(data + ifd_offset);
        if (ifd_offset + 2 + num_entries * 12 + 4 > size) break;

        // Parse entries
        for (int i = 0; i < num_entries; i++) {
            IFDEntry e = parse_ifd_entry(data + ifd_offset + 2 + i * 12);

            switch (e.tag) {
            case TAG_MAKE:
                metadata.camera_make = get_entry_string(e, data, size);
                break;
            case TAG_MODEL:
                metadata.camera_model = get_entry_string(e, data, size);
                break;
            case TAG_ORIENTATION:
                metadata.orientation = get_entry_value(e, data, size);
                break;
            case TAG_EXIF_IFD:
                // Parse EXIF IFD to get MakerNotes
                parse_exif_ifd(data, size, get_entry_value(e, data, size), metadata);
                break;
            }
        }

        // Next IFD offset
        ifd_offset = read_u32(data + ifd_offset + 2 + num_entries * 12);
    }

    if (num_ifds < 4) {
        std::cerr << "CR2: Expected 4 IFDs, found " << num_ifds << std::endl;
        return false;
    }

    // IFD[3] contains the RAW data
    uint32_t raw_ifd = ifd_offsets[3];
    uint16_t num_entries = read_u16(data + raw_ifd);

    uint32_t strip_offset = 0;
    uint32_t strip_size = 0;

    for (int i = 0; i < num_entries; i++) {
        IFDEntry e = parse_ifd_entry(data + raw_ifd + 2 + i * 12);

        switch (e.tag) {
        case TAG_STRIP_OFFSETS:
            strip_offset = get_entry_value(e, data, size);
            break;
        case TAG_STRIP_BYTE_COUNTS:
            strip_size = get_entry_value(e, data, size);
            break;
        case TAG_CANON_CR2_SLICE:
            if (e.count >= 3 && e.value_offset + 6 <= size) {
                metadata.slice_count = read_u16(data + e.value_offset) + 1;
                metadata.slice_width = read_u16(data + e.value_offset + 2);
                metadata.last_slice_width = read_u16(data + e.value_offset + 4);
            }
            break;
        }
    }

    std::cout << "CR2: " << metadata.camera_model << std::endl;

    if (strip_offset == 0 || strip_size == 0) {
        std::cerr << "CR2: No RAW data found" << std::endl;
        return false;
    }

    // Parse lossless JPEG header
    JHead jh;
    const uint8_t* jpeg_data = data + strip_offset;
    size_t scan_offset = ljpeg_start(jh, jpeg_data, strip_size);
    if (scan_offset == 0) {
        std::cerr << "CR2: Failed to parse lossless JPEG" << std::endl;
        return false;
    }

    // Get dimensions from lossless JPEG (jh.wide * jh.clrs x jh.high)
    metadata.width = jh.wide * jh.clrs;
    metadata.height = jh.high;
    metadata.ljpeg_precision = jh.bits;
    metadata.white_level = (1 << jh.bits) - 1;

    std::cout << "CR2 LJpeg: " << jh.wide << "x" << jh.high
              << " " << jh.bits << "bit " << jh.clrs << "ch"
              << " -> " << metadata.width << "x" << metadata.height << std::endl;

    // Allocate output
    int raw_width = metadata.width;
    int raw_height = metadata.height;
    bayer.resize(raw_width, raw_height);

    // Initialize vertical predictors
    for (int c = 0; c < 6; c++)
        jh.vpred[c] = 1 << (jh.bits - 1);

    // Create bit reader for scan data
    BitReader br(jpeg_data + scan_offset, strip_size - scan_offset);

    // CR2 slice parameters
    int slice_count = metadata.slice_count;
    int slice_width = metadata.slice_width;
    int last_slice_width = metadata.last_slice_width;

    std::cout << "CR2 slices: " << slice_count << " x " << slice_width
              << " + " << last_slice_width << std::endl;

    // Decode row by row (from LibRaw lossless_jpeg_load_raw)
    int jwide = jh.wide * jh.clrs;
    int row = 0, col = 0;

    for (int jrow = 0; jrow < jh.high; jrow++) {
        uint16_t* rp = ljpeg_row(jrow, jh, br);

        for (int jcol = 0; jcol < jwide; jcol++) {
            uint16_t val = *rp++;

            // Handle CR2 slicing (from LibRaw)
            if (slice_count > 0) {
                int jidx = jrow * jwide + jcol;
                int i = jidx / (slice_width * raw_height);
                int j = (i >= slice_count - 1) ? 1 : 0;
                if (j) i = slice_count - 1;

                jidx -= i * (slice_width * raw_height);
                int sw = j ? last_slice_width : slice_width;
                if (sw > 0) {
                    row = jidx / sw;
                    col = jidx % sw + i * slice_width;
                }
            }

            // Store pixel
            if (row < raw_height && col < raw_width) {
                bayer.data[row * raw_width + col] = val;
            }

            // Advance position (non-sliced case)
            if (slice_count == 0) {
                if (++col >= raw_width) {
                    col = 0;
                    row++;
                }
            }
        }
    }

    std::cout << "CR2: Decoded " << raw_width << "x" << raw_height << std::endl;
    return true;
}

} // namespace canon
