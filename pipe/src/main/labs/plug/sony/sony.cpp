// sony.cpp
// Sony ARW RAW decoder (ARW1 and ARW2 compression)
// Clean-room implementation - no libraw, no OpenCV.
//
// Decoder::prepare() loads ARW file from memory -> Bayer buffer + metadata
//
// Compression detection (from RawSpeed ArwDecoder.cpp):
//   arw1 = (strip_byte_count * 8) != (width * height * bits_per_sample)
//
// ARW1: Huffman + delta, columns right-to-left, 12-bit output
//       Used by: A7 III (ILCE-7M3), etc.
// ARW2: Block-based 16-pixel encoding, 11-bit output
//       Used by: older cameras

#include "sony.h"
#include <iostream>
#include <vector>
#include <cstring>

namespace sony
{
    namespace internal
    {
        // Helper: Read 16-bit value (little-endian)
        uint16_t read_u16(const uint8_t *data)
        {
            return data[0] | (data[1] << 8);
        }

        // Helper: Read 32-bit value (little-endian)
        uint32_t read_u32(const uint8_t *data)
        {
            return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        }

        // Helper: Read rational value (numerator/denominator)
        float read_rational(const std::vector<uint8_t> &file_data, uint32_t offset)
        {
            if (offset + 8 > file_data.size())
                return 0.0f;
            uint32_t numerator = read_u32(&file_data[offset]);
            uint32_t denominator = read_u32(&file_data[offset + 4]);
            if (denominator == 0)
                return 0.0f;
            return static_cast<float>(numerator) / static_cast<float>(denominator);
        }

        // Parse a single IFD entry
        IFDEntry parse_ifd_entry(const uint8_t *data)
        {
            IFDEntry entry;
            entry.tag = read_u16(data);
            entry.type = read_u16(data + 2);
            entry.count = read_u32(data + 4);
            entry.value_offset = read_u32(data + 8);
            return entry;
        }

        // Get value from IFD entry (handles inline vs offset values)
        uint32_t get_entry_value(const IFDEntry &entry, const std::vector<uint8_t> &file_data)
        {
            if (entry.count == 1)
            {
                if (entry.type == TYPE_BYTE)
                    return entry.value_offset & 0xFF;
                if (entry.type == TYPE_SHORT)
                    return entry.value_offset & 0xFFFF;
                if (entry.type == TYPE_LONG)
                    return entry.value_offset;
            }

            if (entry.type == TYPE_SHORT && entry.value_offset < file_data.size() - 2)
                return read_u16(&file_data[entry.value_offset]);
            if (entry.type == TYPE_LONG && entry.value_offset < file_data.size() - 4)
                return read_u32(&file_data[entry.value_offset]);

            return entry.value_offset;
        }

        // Get string value from IFD entry
        std::string get_entry_string(const IFDEntry &entry, const std::vector<uint8_t> &file_data)
        {
            if (entry.type != TYPE_ASCII)
                return "";

            std::string result;
            if (entry.count <= 4)
            {
                const char *str = reinterpret_cast<const char *>(&entry.value_offset);
                result = std::string(str, std::min(entry.count, 4u));
            }
            else
            {
                if (entry.value_offset + entry.count <= file_data.size())
                {
                    const char *str = reinterpret_cast<const char *>(&file_data[entry.value_offset]);
                    result = std::string(str, entry.count);
                }
            }

            size_t null_pos = result.find('\0');
            if (null_pos != std::string::npos)
                result = result.substr(0, null_pos);

            return result;
        }

        // ============================================================
        // Sony ARW1 decompression (Huffman + delta, 12-bit output)
        // Used by: A7 III (ILCE-7M3), and other cameras where
        //   strip_byte_count * 8 != width * height * bits_per_sample
        // ============================================================

        // MSB bitstream reader for ARW1
        // Matches RawSpeed's BitStreamerMSB: 64-bit cache, big-endian fill
        class BitStreamMSB {
            const uint8_t* data;
            size_t size;
            size_t pos;       // byte position
            uint64_t cache;   // 64-bit cache (RawSpeed uses 64-bit)
            int fillLevel;    // bits in cache
        public:
            BitStreamMSB(const uint8_t* d, size_t s)
                : data(d), size(s), pos(0), cache(0), fillLevel(0) {}

            // Fill cache with 32 bits (4 bytes, big-endian)
            void fill(int need) {
                while (fillLevel < need && pos + 4 <= size) {
                    // Read 4 bytes as big-endian uint32
                    uint32_t chunk = (data[pos] << 24) | (data[pos+1] << 16) |
                                     (data[pos+2] << 8) | data[pos+3];
                    pos += 4;
                    // Push into low end (RawSpeed: RightInLeftOut)
                    int shift = 64 - fillLevel - 32;
                    cache |= (uint64_t)chunk << shift;
                    fillLevel += 32;
                }
            }

            // Get n bits from high end (no refill)
            uint32_t getBitsNoFill(int n) {
                if (n == 0) return 0;
                // Extract high n bits
                uint32_t result = (uint32_t)(cache >> (64 - n));
                // Shift cache left
                cache <<= n;
                fillLevel -= n;
                return result;
            }
        };

        // Sign extension for Huffman diff values
        static inline int extend_sign(int diff, int len) {
            if (len == 0) return 0;
            if (diff < (1 << (len - 1)))
                diff -= (1 << len) - 1;
            return diff;
        }

        bool decompress_arw1(
            const uint8_t *compressed_data,
            size_t compressed_size,
            uint16_t *output,
            int width,
            int height)
        {
            BitStreamMSB bits(compressed_data, compressed_size);
            int pred = 0;

            // ARW1 iterates: columns right-to-left, rows even-then-odd
            for (int col = width - 1; col >= 0; col--) {
                for (int row = 0; row < height + 1; row += 2) {
                    bits.fill(32);

                    // After even rows, switch to odd rows
                    if (row == height)
                        row = 1;

                    // Decode variable length: start with 4 - first 2 bits
                    uint32_t len = 4 - bits.getBitsNoFill(2);

                    // Special cases for length encoding
                    if (len == 3 && bits.getBitsNoFill(1))
                        len = 0;

                    if (len == 4) {
                        while (len < 17 && !bits.getBitsNoFill(1))
                            len++;
                    }

                    // Get diff value and extend sign
                    int diff = (len > 0) ? (int)bits.getBitsNoFill(len) : 0;
                    diff = extend_sign(diff, len);
                    pred += diff;

                    // Clamp to 12-bit range (RawSpeed: clampBits(pred, 12))
                    int clamped = pred;
                    if (clamped < 0) clamped = 0;
                    if (clamped > 4095) clamped = 4095;

                    output[row * width + col] = static_cast<uint16_t>(clamped);
                }
            }

            return true;
        }

        // ============================================================
        // Sony ARW2 decompression (block-based, 11-bit output)
        // Used by: older cameras where
        //   strip_byte_count * 8 == width * height * bits_per_sample
        // TODO: Add ARW2 sample file to test this path
        // ============================================================
        bool decompress_arw2(
            const uint8_t *compressed_data,
            size_t compressed_size,
            uint16_t *output,
            int width,
            int height)
        {
            int raw_width = width;
            const uint8_t *data_ptr = compressed_data;
            const uint8_t *data_end = compressed_data + compressed_size;

            for (int row = 0; row < height; row++)
            {
                const uint8_t *row_data = data_ptr;
                data_ptr += raw_width;

                if (data_ptr > data_end)
                {
                    std::cerr << "ARW2: Premature end of compressed data at row " << row << std::endl;
                    return false;
                }

                const uint8_t *dp = row_data;
                uint16_t *row_out = output + (row * width);
                int col = 0;
                int block_num = 0;

                while (col < raw_width - 30)
                {
                    uint32_t val = dp[0] | (dp[1] << 8) | (dp[2] << 16) | (dp[3] << 24);

                    uint16_t max = val & 0x7FF;
                    uint16_t min = (val >> 11) & 0x7FF;
                    uint8_t imax = (val >> 22) & 0x0F;
                    uint8_t imin = (val >> 26) & 0x0F;

                    // Debug: print first block of last row
                    if (row == height - 1 && block_num == 0) {
                        fprintf(stderr, "Last row, block 0:\n");
                        fprintf(stderr, "  Raw bytes: ");
                        for (int b = 0; b < 16; b++) fprintf(stderr, "%02x ", dp[b]);
                        fprintf(stderr, "\n");
                        fprintf(stderr, "  val = 0x%08x\n", val);
                        fprintf(stderr, "  max=%d min=%d imax=%d imin=%d\n", max, min, imax, imin);
                    }

                    int sh = 0;
                    uint16_t range = max - min;
                    while (sh < 4 && (0x80 << sh) <= range)
                        sh++;

                    uint16_t pix[16];
                    int bit = 30;

                    for (int i = 0; i < 16; i++)
                    {
                        int p;
                        const char* src = "delta";
                        if (i == imax) {
                            p = max;
                            src = "max";
                        }
                        else if (i == imin) {
                            p = min;
                            src = "min";
                        }
                        else
                        {
                            int byte_offset = bit >> 3;
                            int bit_offset = bit & 7;
                            uint16_t delta_bits = dp[byte_offset] | (dp[byte_offset + 1] << 8);
                            uint16_t delta = (delta_bits >> bit_offset) & 0x7F;
                            p = (delta << sh) + min;
                            if (p > 0x7ff) p = 0x7ff;  // clamp like RawSpeed
                            // Debug: show bit extraction
                            if (row == height - 1 && block_num == 0 && i < 4) {
                                fprintf(stderr, "  i=%d: bit=%d, byte=%d, off=%d, dp[%d]=0x%02x, dp[%d]=0x%02x, delta_bits=0x%04x, delta=%d, p=%d\n",
                                    i, bit, byte_offset, bit_offset, byte_offset, dp[byte_offset], byte_offset+1, dp[byte_offset+1], delta_bits, delta, p);
                            }
                            bit += 7;
                        }
                        // Debug: print first few pixels of last row block 0
                        if (row == height - 1 && block_num == 0 && i < 4) {
                            fprintf(stderr, "  pix[%d] = %d (%s)\n", i, p, src);
                        }
                        // DT's PPM does not have the << 1 shift
                        // Output p directly (11-bit values 0-2047)
                        pix[i] = p;
                    }

                    for (int i = 0; i < 16; i++, col += 2)
                    {
                        if (col < width) {
                            row_out[col] = pix[i];
                            // Debug: verify value written
                            if (row == height - 1 && block_num == 0 && col < 8) {
                                fprintf(stderr, "  row_out[%d] = %d\n", col, row_out[col]);
                            }
                        }
                    }

                    col -= (col & 1) ? 1 : 31;
                    dp += 16;
                    block_num++;
                }
            }

            return true;
        }

        // TIFF tag constants
        enum TIFFTag
        {
            TAG_IMAGE_WIDTH = 256,
            TAG_IMAGE_LENGTH = 257,
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
            TAG_CFA_PATTERN = 33422,
            TAG_DEFAULT_CROP_ORIGIN = 0xc61f,
            TAG_DEFAULT_CROP_SIZE = 0xc620,
            TAG_PREVIEW_IMAGE_START = 0x0201,
            TAG_PREVIEW_IMAGE_LENGTH = 0x0202
        };

        enum EXIFTag
        {
            EXIF_TAG_ISO = 34855,
            EXIF_TAG_EXPOSURE_TIME = 33434,
            EXIF_TAG_FNUMBER = 33437,
            EXIF_TAG_FOCAL_LENGTH = 37386,
            EXIF_TAG_LENS_MODEL = 42036
        };

        enum SonyMakerTag
        {
            SONY_TAG_COLOR_MATRIX = 0x7800,
            SONY_TAG_TONE_CURVE = 0x7010,
            SONY_TAG_DISTORTION_CORR_PARAMS = 0x7037,
            SONY_TAG_WB_RGGB = 0x7313,
            SONY_TAG_CONTRAST = 0x2004,
            SONY_TAG_SATURATION = 0x2005,
            SONY_TAG_SHARPNESS = 0x2006,
            SONY_TAG_CREATIVE_STYLE = 0xb020,
            SONY_TAG_DRO = 0xb04f,
            SONY_TAG_SR2_OFFSET = 0x7200,
            SONY_TAG_SR2_LENGTH = 0x7201,
            SONY_TAG_SR2_KEY = 0x7221,
            SONY_TAG_BLACK_LEVEL = 0x7310,
            SONY_TAG_LINEAR_MAX = 0x787f
        };

        // Sony SR2SubIFD decryption (Dave Coffin's algorithm from dcraw)
        void decrypt_sr2(uint8_t* data, uint32_t length, uint32_t key)
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

            uint32_t* d = reinterpret_cast<uint32_t*>(data);
            p = 127;
            for (uint32_t i = 0; i < length / 4; i++) {
                p++;
                d[i] ^= pad[(p-1) & 127] = pad[p & 127] ^ pad[(p+64) & 127];
            }
        }

    } // namespace internal

    bool Decoder::prepare(const uint8_t* data, size_t size,
                          BayerU16& bayer, Info& info, RawMetadata& metadata)
    {
        using namespace internal;

        if (size < 8)
        {
            std::cerr << "RawLoader: Invalid file size" << std::endl;
            return false;
        }

        std::vector<uint8_t> file_data(data, data + size);

        // Check TIFF header
        if (file_data[0] != 'I' || file_data[1] != 'I' || read_u16(&file_data[2]) != 0x002A)
        {
            std::cerr << "RawLoader: Not a valid TIFF file" << std::endl;
            return false;
        }

        uint32_t ifd_offset = read_u32(&file_data[4]);
        if (ifd_offset + 2 > size)
        {
            std::cerr << "RawLoader: Invalid IFD offset" << std::endl;
            return false;
        }

        uint16_t num_entries = read_u16(&file_data[ifd_offset]);

        uint32_t sub_ifd_offset = 0;
        uint32_t exif_ifd_offset = 0;
        uint32_t maker_note_offset = 0;
        uint32_t preview_offset = 0;
        uint32_t preview_length = 0;
        uint32_t sr2_offset = 0;
        uint32_t sr2_length = 0;
        uint32_t sr2_key = 0;

        // Parse IFD0
        for (int i = 0; i < num_entries; i++)
        {
            uint32_t entry_offset = ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > size) break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            if (entry.tag == 0xc634 && entry.value_offset + 100 <= size)
            {
                uint16_t sr2_num = read_u16(&file_data[entry.value_offset]);
                for (int j = 0; j < sr2_num && j < 20; j++)
                {
                    uint32_t sr2_entry_off = entry.value_offset + 2 + j * 12;
                    if (sr2_entry_off + 12 > size) break;
                    IFDEntry se = parse_ifd_entry(&file_data[sr2_entry_off]);
                    if (se.tag == SONY_TAG_SR2_OFFSET) sr2_offset = se.value_offset;
                    if (se.tag == SONY_TAG_SR2_LENGTH) sr2_length = se.value_offset;
                    if (se.tag == SONY_TAG_SR2_KEY) sr2_key = se.value_offset;
                }
            }

            switch (entry.tag)
            {
            case TAG_MAKE:
                metadata.camera_make = get_entry_string(entry, file_data);
                break;
            case TAG_MODEL:
                metadata.camera_model = get_entry_string(entry, file_data);
                break;
            case TAG_ORIENTATION:
                metadata.orientation = get_entry_value(entry, file_data);
                break;
            case TAG_SUB_IFD:
                sub_ifd_offset = get_entry_value(entry, file_data);
                break;
            case TAG_EXIF_IFD:
                exif_ifd_offset = get_entry_value(entry, file_data);
                break;
            case TAG_PREVIEW_IMAGE_START:
                preview_offset = get_entry_value(entry, file_data);
                break;
            case TAG_PREVIEW_IMAGE_LENGTH:
                preview_length = get_entry_value(entry, file_data);
                break;
            }
        }

        // Parse EXIF IFD
        if (exif_ifd_offset != 0 && exif_ifd_offset + 2 <= size)
        {
            uint16_t exif_num_entries = read_u16(&file_data[exif_ifd_offset]);

            for (int i = 0; i < exif_num_entries; i++)
            {
                uint32_t entry_offset = exif_ifd_offset + 2 + (i * 12);
                if (entry_offset + 12 > size) break;

                IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                switch (entry.tag)
                {
                case EXIF_TAG_ISO:
                    metadata.iso = static_cast<float>(get_entry_value(entry, file_data));
                    break;
                case EXIF_TAG_EXPOSURE_TIME:
                    if (entry.type == TYPE_RATIONAL)
                        metadata.shutter_speed = read_rational(file_data, entry.value_offset);
                    break;
                case EXIF_TAG_FNUMBER:
                    if (entry.type == TYPE_RATIONAL)
                        metadata.aperture = read_rational(file_data, entry.value_offset);
                    break;
                case EXIF_TAG_FOCAL_LENGTH:
                    if (entry.type == TYPE_RATIONAL)
                        metadata.focal_length = read_rational(file_data, entry.value_offset);
                    break;
                case EXIF_TAG_LENS_MODEL:
                    metadata.lens_model = get_entry_string(entry, file_data);
                    break;
                case TAG_MAKER_NOTE:
                    maker_note_offset = entry.value_offset;
                    break;
                }
            }
        }

        // Parse Sony MakerNotes
        uint16_t linearization_curve[16384] = {0};
        for (int i = 0; i < 16384; i++)
            linearization_curve[i] = i;

        bool found_sony_curve = false;
        uint16_t sony_curve_vals[4] = {0, 0, 0, 0};
        bool found_sony_wb = false;
        uint16_t wb_rggb[4] = {0, 0, 0, 0};

        if (maker_note_offset != 0 && maker_note_offset + 10 <= size)
        {
            uint32_t maker_ifd_offset = maker_note_offset;

            if (file_data[maker_note_offset] == 'S' && file_data[maker_note_offset + 1] == 'O')
                maker_ifd_offset += 12;

            if (maker_ifd_offset + 2 <= size)
            {
                uint16_t maker_num_entries = read_u16(&file_data[maker_ifd_offset]);
                uint32_t sony_tag2010_offset = 0;

                for (int i = 0; i < maker_num_entries && i < 200; i++)
                {
                    uint32_t entry_offset = maker_ifd_offset + 2 + (i * 12);
                    if (entry_offset + 12 > size) break;

                    IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                    if (entry.tag == 0x2010)
                        sony_tag2010_offset = entry.value_offset;

                    switch (entry.tag)
                    {
                    case SONY_TAG_CONTRAST:
                        metadata.contrast = static_cast<int8_t>(get_entry_value(entry, file_data));
                        break;
                    case SONY_TAG_SATURATION:
                        metadata.saturation = static_cast<int8_t>(get_entry_value(entry, file_data));
                        break;
                    case SONY_TAG_SHARPNESS:
                        metadata.sharpness = static_cast<int8_t>(get_entry_value(entry, file_data));
                        break;
                    case SONY_TAG_CREATIVE_STYLE:
                    {
                        uint32_t style_val = get_entry_value(entry, file_data);
                        switch (style_val)
                        {
                        case 1: metadata.creative_style = "Standard"; break;
                        case 2: metadata.creative_style = "Vivid"; break;
                        case 3: metadata.creative_style = "Portrait"; break;
                        case 4: metadata.creative_style = "Landscape"; break;
                        case 12: metadata.creative_style = "Neutral"; break;
                        case 13: metadata.creative_style = "Clear"; break;
                        default: metadata.creative_style = "Standard"; break;
                        }
                        break;
                    }
                    case SONY_TAG_DRO:
                    {
                        uint32_t dro_val = get_entry_value(entry, file_data);
                        switch (dro_val)
                        {
                        case 0: metadata.dro = "Off"; break;
                        case 1: metadata.dro = "Auto"; break;
                        case 2: metadata.dro = "Lv1"; break;
                        case 3: metadata.dro = "Lv2"; break;
                        case 4: metadata.dro = "Lv3"; break;
                        case 5: metadata.dro = "Lv4"; break;
                        case 6: metadata.dro = "Lv5"; break;
                        default: metadata.dro = "Auto"; break;
                        }
                        break;
                    }
                    }
                }

                if (sony_tag2010_offset != 0 && sony_tag2010_offset + 2 <= size)
                {
                    uint16_t tag2010_num_entries = read_u16(&file_data[sony_tag2010_offset]);

                    for (int i = 0; i < tag2010_num_entries && i < 100; i++)
                    {
                        uint32_t entry_offset = sony_tag2010_offset + 2 + (i * 12);
                        if (entry_offset + 12 > size) break;

                        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                        if (entry.tag == SONY_TAG_TONE_CURVE && entry.count >= 4 && entry.value_offset + 8 <= size)
                        {
                            for (int j = 0; j < 4; j++)
                                sony_curve_vals[j] = (read_u16(&file_data[entry.value_offset + j * 2]) >> 2) & 0xfff;
                            found_sony_curve = true;
                            break;
                        }
                    }
                }
            }
        }

        if (sub_ifd_offset == 0 || sub_ifd_offset + 2 > size)
        {
            std::cerr << "RawLoader: No SubIFD found" << std::endl;
            return false;
        }

        uint16_t sub_num_entries = read_u16(&file_data[sub_ifd_offset]);

        uint32_t strip_offset = 0;
        uint32_t strip_byte_count = 0;
        uint16_t compression = 1;
        uint16_t cfa_pattern[4] = {0};
        bool found_cfa = false;
        bool found_crop_origin = false;
        bool found_crop_size = false;
        int crop_origin[2] = {0, 0};
        int crop_size[2] = {0, 0};

        // Parse SubIFD
        for (int i = 0; i < sub_num_entries; i++)
        {
            uint32_t entry_offset = sub_ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > size) break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            if (entry.tag == SONY_TAG_TONE_CURVE && !found_sony_curve && entry.count >= 4 && entry.value_offset + 8 <= size)
            {
                for (int j = 0; j < 4; j++)
                    sony_curve_vals[j] = (read_u16(&file_data[entry.value_offset + j * 2]) >> 2) & 0xfff;
                found_sony_curve = true;
            }

            if (entry.tag == SONY_TAG_WB_RGGB && !found_sony_wb && entry.count == 4 && entry.value_offset + 8 <= size)
            {
                for (int j = 0; j < 4; j++)
                    wb_rggb[j] = read_u16(&file_data[entry.value_offset + j * 2]);
                found_sony_wb = true;
            }

            if (entry.tag == SONY_TAG_COLOR_MATRIX && entry.count == 9 && entry.value_offset + 18 <= size)
            {
                for (int j = 0; j < 9; j++) {
                    int16_t val = static_cast<int16_t>(read_u16(&file_data[entry.value_offset + j * 2]));
                    metadata.color_matrix[j] = val / 1024.0f;
                }
            }

            if (entry.tag == SONY_TAG_DISTORTION_CORR_PARAMS && !metadata.has_distortion_params)
            {
                if (entry.count >= 2 && entry.value_offset + entry.count * 2 <= size)
                {
                    int knot_count = static_cast<int16_t>(read_u16(&file_data[entry.value_offset]));
                    if (knot_count > 0 && knot_count <= 16 && knot_count <= static_cast<int>(entry.count) - 1)
                    {
                        metadata.distortion_knot_count = knot_count;
                        for (int j = 0; j < knot_count; j++)
                            metadata.distortion_params[j] = static_cast<int16_t>(
                                read_u16(&file_data[entry.value_offset + (j + 1) * 2]));
                        metadata.has_distortion_params = true;
                    }
                }
            }

            switch (entry.tag)
            {
            case TAG_IMAGE_WIDTH:
                metadata.width = get_entry_value(entry, file_data);
                break;
            case TAG_IMAGE_LENGTH:
                metadata.height = get_entry_value(entry, file_data);
                break;
            case TAG_COMPRESSION:
                compression = get_entry_value(entry, file_data);
                break;
            case TAG_STRIP_OFFSETS:
                strip_offset = get_entry_value(entry, file_data);
                break;
            case TAG_STRIP_BYTE_COUNTS:
                strip_byte_count = get_entry_value(entry, file_data);
                break;
            case TAG_CFA_PATTERN:
                if (entry.value_offset + 8 <= size)
                {
                    for (int j = 0; j < 4; j++)
                        cfa_pattern[j] = file_data[entry.value_offset + 4 + j];
                    found_cfa = true;
                }
                break;
            case TAG_DEFAULT_CROP_ORIGIN:
                if (entry.count == 2)
                {
                    if (entry.type == 4)
                    {
                        crop_origin[0] = read_u32(&file_data[entry.value_offset]);
                        crop_origin[1] = read_u32(&file_data[entry.value_offset + 4]);
                        found_crop_origin = true;
                    }
                    else if (entry.type == 5)
                    {
                        uint32_t num0 = read_u32(&file_data[entry.value_offset]);
                        uint32_t den0 = read_u32(&file_data[entry.value_offset + 4]);
                        uint32_t num1 = read_u32(&file_data[entry.value_offset + 8]);
                        uint32_t den1 = read_u32(&file_data[entry.value_offset + 12]);
                        crop_origin[0] = (den0 > 0) ? num0 / den0 : num0;
                        crop_origin[1] = (den1 > 0) ? num1 / den1 : num1;
                        found_crop_origin = true;
                    }
                }
                break;
            case TAG_DEFAULT_CROP_SIZE:
                if (entry.count == 2)
                {
                    if (entry.type == 4)
                    {
                        crop_size[0] = read_u32(&file_data[entry.value_offset]);
                        crop_size[1] = read_u32(&file_data[entry.value_offset + 4]);
                        found_crop_size = true;
                    }
                    else if (entry.type == 5)
                    {
                        uint32_t num0 = read_u32(&file_data[entry.value_offset]);
                        uint32_t den0 = read_u32(&file_data[entry.value_offset + 4]);
                        uint32_t num1 = read_u32(&file_data[entry.value_offset + 8]);
                        uint32_t den1 = read_u32(&file_data[entry.value_offset + 12]);
                        crop_size[0] = (den0 > 0) ? num0 / den0 : num0;
                        crop_size[1] = (den1 > 0) ? num1 / den1 : num1;
                        found_crop_size = true;
                    }
                }
                break;
            }
        }

        // Map CFA pattern to code
        if (found_cfa)
        {
            if (cfa_pattern[0] == 0 && cfa_pattern[1] == 1 && cfa_pattern[2] == 1 && cfa_pattern[3] == 2)
                metadata.bayer_pattern = 0; // RGGB
            else if (cfa_pattern[0] == 2 && cfa_pattern[1] == 1 && cfa_pattern[2] == 1 && cfa_pattern[3] == 0)
                metadata.bayer_pattern = 2; // BGGR
            else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 0 && cfa_pattern[2] == 2 && cfa_pattern[3] == 1)
                metadata.bayer_pattern = 1; // GRBG
            else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 2 && cfa_pattern[2] == 0 && cfa_pattern[3] == 1)
                metadata.bayer_pattern = 3; // GBRG
        }

        // Set WB
        if (found_sony_wb && wb_rggb[1] > 0)
        {
            metadata.wb_rggb[0] = wb_rggb[0];
            metadata.wb_rggb[1] = wb_rggb[1];
            metadata.wb_rggb[2] = wb_rggb[3];
            metadata.wb_rggb[3] = wb_rggb[2];
        }

        // Set crop
        if (found_crop_origin && found_crop_size)
        {
            metadata.crop_left = crop_origin[0];
            metadata.crop_top = crop_origin[1];
            metadata.crop_width = crop_size[0];
            metadata.crop_height = crop_size[1];
        }
        else
        {
            metadata.crop_left = 0;
            metadata.crop_top = 0;
            metadata.crop_width = metadata.width;
            metadata.crop_height = metadata.height;
        }

        // Decrypt and parse SR2SubIFD for color matrix and linear_max
        if (sr2_offset > 0 && sr2_length > 0 && sr2_key != 0 && sr2_offset + sr2_length <= size)
        {
            std::vector<uint8_t> sr2(sr2_length);
            std::memcpy(sr2.data(), &file_data[sr2_offset], sr2_length);
            decrypt_sr2(sr2.data(), sr2_length, sr2_key);

            bool found_matrix = false;
            bool found_linear_max = false;
            bool found_black = false;

            if (sr2_length >= 2)
            {
                uint16_t sr2_num_entries = read_u16(sr2.data());
                for (int i = 0; i < sr2_num_entries && i < 200; i++)
                {
                    uint32_t entry_off = 2 + i * 12;
                    if (entry_off + 12 > sr2_length) break;
                    IFDEntry entry = parse_ifd_entry(&sr2[entry_off]);

                    if (!found_matrix && entry.tag == SONY_TAG_COLOR_MATRIX && entry.count == 9)
                    {
                        uint32_t rel_offset = entry.value_offset - sr2_offset;
                        if (rel_offset + 18 <= sr2_length)
                        {
                            for (int j = 0; j < 9; j++)
                            {
                                int16_t val = static_cast<int16_t>(read_u16(&sr2[rel_offset + j * 2]));
                                metadata.color_matrix[j] = val / 1024.0f;
                            }
                            found_matrix = true;
                        }
                    }

                    if (!found_black && entry.tag == SONY_TAG_BLACK_LEVEL && entry.count == 4)
                    {
                        uint32_t rel_offset = entry.value_offset - sr2_offset;
                        if (rel_offset + 8 <= sr2_length)
                        {
                            uint16_t cblack[4];
                            for (int j = 0; j < 4; j++)
                                cblack[j] = read_u16(&sr2[rel_offset + j * 2]);
                            uint16_t min_black = cblack[0];
                            for (int j = 1; j < 4; j++)
                                if (cblack[j] < min_black) min_black = cblack[j];
                            metadata.black_level = min_black;
                            found_black = true;
                        }
                    }

                    if (!found_linear_max && entry.tag == SONY_TAG_LINEAR_MAX)
                    {
                        metadata.white_level = 16383;
                        found_linear_max = true;
                    }

                    if (found_matrix && found_linear_max && found_black) break;
                }
            }

            if (!found_black)
                metadata.black_level = 512;
        }
        else
        {
            metadata.black_level = 512;
            metadata.white_level = 16383;
        }

        if (strip_offset == 0 || strip_offset + strip_byte_count > size)
        {
            std::cerr << "RawLoader: Invalid strip data location" << std::endl;
            return false;
        }

        // Build Sony linearization curve from breakpoints
        if (found_sony_curve && (sony_curve_vals[0] > 0 || sony_curve_vals[1] > 0))
        {
            uint16_t sony_curve[6] = {0, sony_curve_vals[0], sony_curve_vals[1],
                                      sony_curve_vals[2], sony_curve_vals[3], 4095};

            for (int seg = 0; seg < 5; seg++)
            {
                for (int j = sony_curve[seg] + 1; j <= sony_curve[seg + 1]; j++)
                    linearization_curve[j] = linearization_curve[j - 1] + (1 << seg);
            }
        }

        // Decompress - detect ARW1 vs ARW2
        // RawSpeed detection: arw1 = (strip_byte_count * 8) != (width * height * bits_per_sample)
        // BUT: RawSpeed has special case (ArwDecoder.cpp lines 216-224):
        //   If multiple IFDs have MAKE="SONY" (no space), override bits_per_sample to 8
        //   This handles Sony E-550 and similar cameras
        if (compression == 32767)
        {
            // Count MAKE tags with "SONY" value
            // TODO: Proper implementation would parse all IFDs. For now, assume modern
            // Sony cameras (A7 III etc.) have this pattern and use bpp=8 for detection.
            // This makes arw1 detection return FALSE, using ARW2 path.
            uint32_t detection_bpp = 8;  // Override like RawSpeed does for multi-MAKE Sony files

            uint64_t compressed_bits = static_cast<uint64_t>(strip_byte_count) * 8;
            uint64_t expected_bits = static_cast<uint64_t>(metadata.width) * metadata.height * detection_bpp;
            bool is_arw1 = (compressed_bits != expected_bits);

            // RawSpeed: ARW1 adds 8 rows to height (ArwDecoder.cpp line 232)
            int decode_height = metadata.height;
            if (is_arw1)
                decode_height += 8;

            // Resize bayer buffer to decode height
            bayer.resize(metadata.width, decode_height, 1);

            if (is_arw1)
            {
                // ARW1: Huffman + delta encoding, columns right-to-left, 12-bit output
                // Used by A7 III (ILCE-7M3), etc.
                if (!decompress_arw1(&file_data[strip_offset], strip_byte_count,
                                     bayer.ptr(), metadata.width, decode_height))
                {
                    std::cerr << "RawLoader: ARW1 decompression failed" << std::endl;
                    return false;
                }
            }
            else
            {
                // ARW2: Block-based 16-pixel encoding, 11-bit output
                // TODO: Need ARW2 sample file to test this path
                if (!decompress_arw2(&file_data[strip_offset], strip_byte_count,
                                     bayer.ptr(), metadata.width, metadata.height))
                {
                    std::cerr << "RawLoader: ARW2 decompression failed" << std::endl;
                    return false;
                }
            }

            // Apply linearization curve
            if (found_sony_curve)
            {
                size_t total_pixels = static_cast<size_t>(metadata.width) * decode_height;
                for (size_t i = 0; i < total_pixels; i++)
                {
                    // ARW1: 12-bit output, use directly
                    // ARW2: 11-bit output, shift left to 12-bit
                    uint32_t curve_index = is_arw1 ? bayer.data[i] : (bayer.data[i] << 1);
                    if (curve_index < 16384)
                        bayer.data[i] = linearization_curve[curve_index];
                }
                if (metadata.white_level == 0 || metadata.white_level > 16383)
                    metadata.white_level = 16383;
            }
        }
        else if (compression == 1)
        {
            bayer.resize(metadata.width, metadata.height, 1);
            size_t expected_size = static_cast<size_t>(metadata.width) * metadata.height * 2;
            if (strip_byte_count < expected_size)
            {
                std::cerr << "RawLoader: Strip data too small" << std::endl;
                return false;
            }
            std::memcpy(bayer.ptr(), &file_data[strip_offset], expected_size);
        }
        else
        {
            std::cerr << "RawLoader: Unsupported compression type " << compression << std::endl;
            return false;
        }

        // Store preview JPEG bytes (no decoding)
        if (preview_offset != 0 && preview_length != 0 && preview_offset + preview_length <= size)
        {
            metadata.preview_jpeg.assign(
                &file_data[preview_offset],
                &file_data[preview_offset + preview_length]);
        }

        // Populate info map
        info["camera_make"] = metadata.camera_make;
        info["camera_model"] = metadata.camera_model;
        info["width"] = std::to_string(metadata.width);
        info["height"] = std::to_string(metadata.height);
        info["iso"] = std::to_string(metadata.iso);
        info["aperture"] = std::to_string(metadata.aperture);

        return true;
    }

} // namespace sony
