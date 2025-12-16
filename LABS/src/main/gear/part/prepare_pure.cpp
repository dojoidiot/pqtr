// prepare_pure.cpp
// Sony ARW2 RAW decoder - OpenCV-free implementation
// Returns BayerBuffer + Metadata + Preview (all simple types)

#include <gear/sony_pure.h>
#include <tool.hpp>
#include <pipe.hpp>
#include <iostream>
#include <cstring>

namespace sony {
namespace pure {

namespace internal {

// Helper: Read 16-bit value (little-endian)
uint16_t read_u16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

// Helper: Read 32-bit value (little-endian)
uint32_t read_u32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Helper: Read rational value (numerator/denominator)
float read_rational(const std::vector<uint8_t>& file_data, uint32_t offset) {
    if (offset + 8 > file_data.size()) return 0.0f;
    uint32_t numerator = read_u32(&file_data[offset]);
    uint32_t denominator = read_u32(&file_data[offset + 4]);
    if (denominator == 0) return 0.0f;
    return static_cast<float>(numerator) / static_cast<float>(denominator);
}

// Parse a single IFD entry
IFDEntry parse_ifd_entry(const uint8_t* data) {
    IFDEntry entry;
    entry.tag = read_u16(data);
    entry.type = read_u16(data + 2);
    entry.count = read_u32(data + 4);
    entry.value_offset = read_u32(data + 8);
    return entry;
}

// Get value from IFD entry
uint32_t get_entry_value(const IFDEntry& entry, const std::vector<uint8_t>& file_data) {
    if (entry.count == 1) {
        if (entry.type == TYPE_BYTE) return entry.value_offset & 0xFF;
        if (entry.type == TYPE_SHORT) return entry.value_offset & 0xFFFF;
        if (entry.type == TYPE_LONG) return entry.value_offset;
    }
    if (entry.type == TYPE_SHORT && entry.value_offset < file_data.size() - 2)
        return read_u16(&file_data[entry.value_offset]);
    if (entry.type == TYPE_LONG && entry.value_offset < file_data.size() - 4)
        return read_u32(&file_data[entry.value_offset]);
    return entry.value_offset;
}

// Get string value from IFD entry
std::string get_entry_string(const IFDEntry& entry, const std::vector<uint8_t>& file_data) {
    if (entry.type != TYPE_ASCII) return "";

    std::string result;
    if (entry.count <= 4) {
        const char* str = reinterpret_cast<const char*>(&entry.value_offset);
        result = std::string(str, std::min(entry.count, 4u));
    } else {
        if (entry.value_offset + entry.count <= file_data.size()) {
            const char* str = reinterpret_cast<const char*>(&file_data[entry.value_offset]);
            result = std::string(str, entry.count);
        }
    }
    size_t null_pos = result.find('\0');
    if (null_pos != std::string::npos) result = result.substr(0, null_pos);
    return result;
}

// Sony ARW2 decompression
bool decompress_arw2(const uint8_t* compressed_data, size_t compressed_size,
                     uint16_t* output, int width, int height) {
    int raw_width = width;
    const uint8_t* data_ptr = compressed_data;
    const uint8_t* data_end = compressed_data + compressed_size;

    for (int row = 0; row < height; row++) {
        const uint8_t* row_data = data_ptr;
        data_ptr += raw_width;

        if (data_ptr > data_end) {
            std::cerr << "ARW2: Premature end at row " << row << std::endl;
            return false;
        }

        const uint8_t* dp = row_data;
        uint16_t* row_out = output + (row * width);
        int col = 0;

        while (col < raw_width - 30) {
            uint32_t val = dp[0] | (dp[1] << 8) | (dp[2] << 16) | (dp[3] << 24);

            uint16_t max = val & 0x7FF;
            uint16_t min = (val >> 11) & 0x7FF;
            uint8_t imax = (val >> 22) & 0x0F;
            uint8_t imin = (val >> 26) & 0x0F;

            int sh = 0;
            uint16_t range = max - min;
            while (sh < 4 && (0x80 << sh) <= range) sh++;

            uint16_t pix[16];
            int bit = 30;

            for (int i = 0; i < 16; i++) {
                if (i == imax) {
                    pix[i] = max;
                } else if (i == imin) {
                    pix[i] = min;
                } else {
                    int byte_offset = bit >> 3;
                    int bit_offset = bit & 7;
                    uint16_t delta_bits = dp[byte_offset] | (dp[byte_offset + 1] << 8);
                    uint16_t delta = (delta_bits >> bit_offset) & 0x7F;
                    pix[i] = (delta << sh) + min;
                    bit += 7;
                }
            }

            for (int i = 0; i < 16; i++, col += 2) {
                if (col < width) row_out[col] = pix[i];
            }
            col -= (col & 1) ? 1 : 31;
            dp += 16;
        }
    }
    return true;
}

// Sony SR2SubIFD decryption (Dave Coffin's algorithm)
void decrypt_sr2(uint8_t* data, uint32_t length, uint32_t key) {
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

// TIFF tag constants
enum TIFFTag {
    TAG_IMAGE_WIDTH = 256,
    TAG_IMAGE_LENGTH = 257,
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

enum EXIFTag {
    EXIF_TAG_ISO = 34855,
    EXIF_TAG_EXPOSURE_TIME = 33434,
    EXIF_TAG_FNUMBER = 33437,
    EXIF_TAG_FOCAL_LENGTH = 37386,
    EXIF_TAG_LENS_MODEL = 42036
};

enum SonyMakerTag {
    SONY_TAG_DISTORTION_CORR_PARAMS = 0x7037,
    SONY_TAG_SR2_SUBIFD_OFFSET = 0x7200,
    SONY_TAG_SR2_SUBIFD_LENGTH = 0x7201,
    SONY_TAG_SR2_SUBIFD_KEY = 0x7221,
    SONY_TAG_WB_RGGB = 0x7313,
    SONY_TAG_COLOR_MATRIX = 0x7800,
    SONY_TAG_CONTRAST = 0x2004,
    SONY_TAG_SATURATION = 0x2005,
    SONY_TAG_SHARPNESS = 0x2006,
    SONY_TAG_CREATIVE_STYLE = 0xb020,
    SONY_TAG_DRO = 0xb04f
};

Result decode(pqtr::Sink& source) {
    using namespace internal;

    Result result;
    result.success = false;
    Metadata& meta = result.metadata;

    int file_size = source.size();
    if (file_size < 8) {
        result.error = "Invalid file size";
        return result;
    }

    // Read entire file
    std::vector<uint8_t> file_data(file_size);
    char* data_ptr = nullptr;
    int bytes_read = source.take(data_ptr, file_size);

    if (bytes_read != file_size || !data_ptr) {
        if (data_ptr) delete[] data_ptr;
        result.error = "Read error";
        return result;
    }

    memcpy(file_data.data(), data_ptr, bytes_read);
    delete[] data_ptr;

    // Check TIFF header
    if (file_data[0] != 'I' || file_data[1] != 'I' || read_u16(&file_data[2]) != 0x002A) {
        result.error = "Not a valid TIFF file";
        return result;
    }

    uint32_t ifd_offset = read_u32(&file_data[4]);
    if (ifd_offset + 2 > static_cast<size_t>(file_size)) {
        result.error = "Invalid IFD offset";
        return result;
    }

    uint16_t num_entries = read_u16(&file_data[ifd_offset]);

    uint32_t sub_ifd_offset = 0, exif_ifd_offset = 0, maker_note_offset = 0;
    uint32_t preview_offset = 0, preview_length = 0;
    uint32_t sr2_offset = 0, sr2_length = 0, sr2_key = 0;

    // Parse IFD0
    for (int i = 0; i < num_entries; i++) {
        uint32_t entry_offset = ifd_offset + 2 + (i * 12);
        if (entry_offset + 12 > static_cast<size_t>(file_size)) break;

        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

        // DNGPrivateData (0xc634) contains SR2 IFD
        if (entry.tag == 0xc634) {
            uint32_t sr2_ifd_offset = entry.value_offset;
            if (sr2_ifd_offset + 100 <= static_cast<size_t>(file_size)) {
                uint16_t sr2_dir_entries = read_u16(&file_data[sr2_ifd_offset]);
                for (int j = 0; j < sr2_dir_entries && j < 20; j++) {
                    uint32_t sr2_entry_offset = sr2_ifd_offset + 2 + (j * 12);
                    if (sr2_entry_offset + 12 > static_cast<size_t>(file_size)) break;
                    IFDEntry sr2_entry = parse_ifd_entry(&file_data[sr2_entry_offset]);
                    if (sr2_entry.tag == SONY_TAG_SR2_SUBIFD_OFFSET) sr2_offset = sr2_entry.value_offset;
                    if (sr2_entry.tag == SONY_TAG_SR2_SUBIFD_LENGTH) sr2_length = sr2_entry.value_offset;
                    if (sr2_entry.tag == SONY_TAG_SR2_SUBIFD_KEY) sr2_key = sr2_entry.value_offset;
                }
            }
        }

        switch (entry.tag) {
        case TAG_MAKE: meta.camera_make = get_entry_string(entry, file_data); break;
        case TAG_MODEL: meta.camera_model = get_entry_string(entry, file_data); break;
        case TAG_ORIENTATION: meta.orientation = get_entry_value(entry, file_data); break;
        case TAG_SUB_IFD: sub_ifd_offset = get_entry_value(entry, file_data); break;
        case TAG_EXIF_IFD: exif_ifd_offset = get_entry_value(entry, file_data); break;
        case TAG_PREVIEW_IMAGE_START: preview_offset = get_entry_value(entry, file_data); break;
        case TAG_PREVIEW_IMAGE_LENGTH: preview_length = get_entry_value(entry, file_data); break;
        }
    }

    // Parse EXIF IFD
    if (exif_ifd_offset != 0 && exif_ifd_offset + 2 <= static_cast<size_t>(file_size)) {
        uint16_t exif_num = read_u16(&file_data[exif_ifd_offset]);
        for (int i = 0; i < exif_num; i++) {
            uint32_t entry_offset = exif_ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > static_cast<size_t>(file_size)) break;
            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            switch (entry.tag) {
            case EXIF_TAG_ISO: meta.iso = static_cast<float>(get_entry_value(entry, file_data)); break;
            case EXIF_TAG_EXPOSURE_TIME:
                if (entry.type == internal::TYPE_RATIONAL)
                    meta.shutter_speed = read_rational(file_data, entry.value_offset);
                break;
            case EXIF_TAG_FNUMBER:
                if (entry.type == internal::TYPE_RATIONAL)
                    meta.aperture = read_rational(file_data, entry.value_offset);
                break;
            case EXIF_TAG_FOCAL_LENGTH:
                if (entry.type == internal::TYPE_RATIONAL)
                    meta.focal_length = read_rational(file_data, entry.value_offset);
                break;
            case EXIF_TAG_LENS_MODEL: meta.lens_model = get_entry_string(entry, file_data); break;
            case TAG_MAKER_NOTE: maker_note_offset = entry.value_offset; break;
            }
        }
    }

    // Initialize style defaults
    meta.creative_style = "Standard";
    meta.dro = "Off";
    meta.contrast = meta.saturation = meta.sharpness = 0;
    meta.has_distortion_params = false;
    meta.distortion_knot_count = 0;
    memset(meta.distortion_params, 0, sizeof(meta.distortion_params));

    // Parse Sony MakerNotes
    uint16_t wb_rggb[4] = {0, 0, 0, 0};
    bool found_wb = false;
    int16_t color_matrix_raw[9] = {0};
    bool found_color_matrix = false;

    if (maker_note_offset != 0 && maker_note_offset + 10 <= static_cast<size_t>(file_size)) {
        uint32_t maker_ifd_offset = maker_note_offset;
        if (file_data[maker_note_offset] == 'S' && file_data[maker_note_offset + 1] == 'O')
            maker_ifd_offset += 12;

        if (maker_ifd_offset + 2 <= static_cast<size_t>(file_size)) {
            uint16_t maker_num = read_u16(&file_data[maker_ifd_offset]);
            for (int i = 0; i < maker_num && i < 200; i++) {
                uint32_t entry_offset = maker_ifd_offset + 2 + (i * 12);
                if (entry_offset + 12 > static_cast<size_t>(file_size)) break;
                IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                switch (entry.tag) {
                case SONY_TAG_CONTRAST: meta.contrast = static_cast<int8_t>(get_entry_value(entry, file_data)); break;
                case SONY_TAG_SATURATION: meta.saturation = static_cast<int8_t>(get_entry_value(entry, file_data)); break;
                case SONY_TAG_SHARPNESS: meta.sharpness = static_cast<int8_t>(get_entry_value(entry, file_data)); break;
                case SONY_TAG_CREATIVE_STYLE: {
                    uint32_t val = get_entry_value(entry, file_data);
                    switch (val) {
                    case 1: meta.creative_style = "Standard"; break;
                    case 2: meta.creative_style = "Vivid"; break;
                    case 3: meta.creative_style = "Portrait"; break;
                    case 4: meta.creative_style = "Landscape"; break;
                    case 12: meta.creative_style = "Neutral"; break;
                    case 13: meta.creative_style = "Clear"; break;
                    default: meta.creative_style = "Standard"; break;
                    }
                    break;
                }
                case SONY_TAG_DRO: {
                    uint32_t val = get_entry_value(entry, file_data);
                    switch (val) {
                    case 0: meta.dro = "Off"; break;
                    case 1: meta.dro = "Auto"; break;
                    case 2: case 3: case 4: case 5: case 6:
                        meta.dro = "Lv" + std::to_string(val - 1); break;
                    default: meta.dro = "Auto"; break;
                    }
                    break;
                }
                }
            }
        }
    }

    // Parse SubIFD
    if (sub_ifd_offset == 0 || sub_ifd_offset + 2 > static_cast<size_t>(file_size)) {
        result.error = "No SubIFD found";
        return result;
    }

    uint16_t sub_num = read_u16(&file_data[sub_ifd_offset]);
    uint32_t strip_offset = 0, strip_byte_count = 0;
    uint16_t compression = 1;
    uint16_t cfa_pattern[4] = {0};
    bool found_cfa = false;
    int crop_origin[2] = {0, 0}, crop_size[2] = {0, 0};
    bool found_crop_origin = false, found_crop_size = false;

    for (int i = 0; i < sub_num; i++) {
        uint32_t entry_offset = sub_ifd_offset + 2 + (i * 12);
        if (entry_offset + 12 > static_cast<size_t>(file_size)) break;
        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

        if (entry.tag == SONY_TAG_WB_RGGB && !found_wb) {
            if (entry.count == 4 && entry.value_offset + 8 <= static_cast<size_t>(file_size)) {
                for (int j = 0; j < 4; j++)
                    wb_rggb[j] = read_u16(&file_data[entry.value_offset + j * 2]);
                found_wb = true;
            }
        }

        if (entry.tag == SONY_TAG_DISTORTION_CORR_PARAMS && !meta.has_distortion_params) {
            if (entry.count >= 2 && entry.value_offset + entry.count * 2 <= static_cast<size_t>(file_size)) {
                int knot_count = static_cast<int16_t>(read_u16(&file_data[entry.value_offset]));
                if (knot_count > 0 && knot_count <= 16 && knot_count <= static_cast<int>(entry.count) - 1) {
                    meta.distortion_knot_count = knot_count;
                    for (int j = 0; j < knot_count; j++)
                        meta.distortion_params[j] = static_cast<int16_t>(read_u16(&file_data[entry.value_offset + (j + 1) * 2]));
                    meta.has_distortion_params = true;
                }
            }
        }

        if (entry.tag == SONY_TAG_SR2_SUBIFD_OFFSET) sr2_offset = get_entry_value(entry, file_data);
        if (entry.tag == SONY_TAG_SR2_SUBIFD_LENGTH) sr2_length = get_entry_value(entry, file_data);
        if (entry.tag == SONY_TAG_SR2_SUBIFD_KEY) sr2_key = get_entry_value(entry, file_data);

        switch (entry.tag) {
        case TAG_IMAGE_WIDTH: meta.width = get_entry_value(entry, file_data); break;
        case TAG_IMAGE_LENGTH: meta.height = get_entry_value(entry, file_data); break;
        case TAG_COMPRESSION: compression = get_entry_value(entry, file_data); break;
        case TAG_STRIP_OFFSETS: strip_offset = get_entry_value(entry, file_data); break;
        case TAG_STRIP_BYTE_COUNTS: strip_byte_count = get_entry_value(entry, file_data); break;
        case TAG_CFA_PATTERN:
            if (entry.value_offset + 8 <= static_cast<size_t>(file_size)) {
                for (int j = 0; j < 4; j++) cfa_pattern[j] = file_data[entry.value_offset + 4 + j];
                found_cfa = true;
            }
            break;
        case TAG_DEFAULT_CROP_ORIGIN:
            if (entry.count == 2) {
                if (entry.type == 4) {
                    crop_origin[0] = read_u32(&file_data[entry.value_offset]);
                    crop_origin[1] = read_u32(&file_data[entry.value_offset + 4]);
                    found_crop_origin = true;
                } else if (entry.type == 5) {
                    crop_origin[0] = read_u32(&file_data[entry.value_offset]) / read_u32(&file_data[entry.value_offset + 4]);
                    crop_origin[1] = read_u32(&file_data[entry.value_offset + 8]) / read_u32(&file_data[entry.value_offset + 12]);
                    found_crop_origin = true;
                }
            }
            break;
        case TAG_DEFAULT_CROP_SIZE:
            if (entry.count == 2) {
                if (entry.type == 4) {
                    crop_size[0] = read_u32(&file_data[entry.value_offset]);
                    crop_size[1] = read_u32(&file_data[entry.value_offset + 4]);
                    found_crop_size = true;
                } else if (entry.type == 5) {
                    crop_size[0] = read_u32(&file_data[entry.value_offset]) / read_u32(&file_data[entry.value_offset + 4]);
                    crop_size[1] = read_u32(&file_data[entry.value_offset + 8]) / read_u32(&file_data[entry.value_offset + 12]);
                    found_crop_size = true;
                }
            }
            break;
        }
    }

    // Bayer pattern
    if (found_cfa) {
        if (cfa_pattern[0] == 0 && cfa_pattern[1] == 1 && cfa_pattern[2] == 1 && cfa_pattern[3] == 2)
            meta.bayer_pattern = 46; // RGGB
        else if (cfa_pattern[0] == 2 && cfa_pattern[1] == 1 && cfa_pattern[2] == 1 && cfa_pattern[3] == 0)
            meta.bayer_pattern = 48; // BGGR
        else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 0 && cfa_pattern[2] == 2 && cfa_pattern[3] == 1)
            meta.bayer_pattern = 47; // GRBG
        else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 2 && cfa_pattern[2] == 0 && cfa_pattern[3] == 1)
            meta.bayer_pattern = 49; // GBRG
        else
            meta.bayer_pattern = 46;
    } else {
        meta.bayer_pattern = 46;
    }

    // Black/white levels for Sony ARW2
    meta.black_level = 380;
    meta.white_level = 17220;

    // White balance
    if (found_wb && wb_rggb[1] > 0) {
        meta.wb_rggb[0] = wb_rggb[0];
        meta.wb_rggb[1] = wb_rggb[1];
        meta.wb_rggb[2] = wb_rggb[3];
        meta.wb_rggb[3] = wb_rggb[2];
    } else {
        meta.wb_rggb[0] = 2176; meta.wb_rggb[1] = 1024;
        meta.wb_rggb[2] = 1551; meta.wb_rggb[3] = 1024;
    }

    // Parse SR2SubIFD for ColorMatrix
    if (sr2_offset > 0 && sr2_length > 0 && sr2_key != 0 && !found_color_matrix) {
        if (sr2_offset + sr2_length <= static_cast<size_t>(file_size)) {
            std::vector<uint8_t> sr2_data(sr2_length);
            memcpy(sr2_data.data(), &file_data[sr2_offset], sr2_length);
            decrypt_sr2(sr2_data.data(), sr2_length, sr2_key);

            if (sr2_length >= 2) {
                uint16_t sr2_num = read_u16(sr2_data.data());
                for (int i = 0; i < sr2_num && i < 200; i++) {
                    uint32_t entry_offset = 2 + (i * 12);
                    if (entry_offset + 12 > sr2_length) break;
                    IFDEntry entry = parse_ifd_entry(&sr2_data[entry_offset]);

                    if (entry.tag == SONY_TAG_COLOR_MATRIX && entry.count == 9) {
                        uint32_t rel_offset = entry.value_offset - sr2_offset;
                        if (rel_offset + 18 <= sr2_length) {
                            for (int j = 0; j < 9; j++)
                                color_matrix_raw[j] = static_cast<int16_t>(read_u16(&sr2_data[rel_offset + j * 2]));
                            found_color_matrix = true;
                        }
                        break;
                    }
                }
            }
        }
    }

    // Color matrix
    if (found_color_matrix) {
        for (int i = 0; i < 9; i++)
            meta.color_matrix[i] = color_matrix_raw[i] / 1024.0f;
    } else {
        // Fallback matrix
        meta.color_matrix[0] = 1344.0f / 1024.0f; meta.color_matrix[1] = -211.0f / 1024.0f; meta.color_matrix[2] = -76.0f / 1024.0f;
        meta.color_matrix[3] = -9.0f / 1024.0f;   meta.color_matrix[4] = 1224.0f / 1024.0f; meta.color_matrix[5] = -159.0f / 1024.0f;
        meta.color_matrix[6] = 7.0f / 1024.0f;    meta.color_matrix[7] = -41.0f / 1024.0f;  meta.color_matrix[8] = 1090.0f / 1024.0f;
    }

    // Crop
    if (found_crop_origin && found_crop_size) {
        meta.crop_left = crop_origin[0];
        meta.crop_top = crop_origin[1];
        meta.crop_width = crop_size[0];
        meta.crop_height = crop_size[1];
    } else {
        meta.crop_left = meta.crop_top = 0;
        meta.crop_width = meta.width;
        meta.crop_height = meta.height;
    }

    // Validate strip
    if (strip_offset == 0 || strip_offset + strip_byte_count > static_cast<size_t>(file_size)) {
        result.error = "Invalid strip data";
        return result;
    }

    // Allocate Bayer buffer
    result.bayer.width = meta.width;
    result.bayer.height = meta.height;
    result.bayer.data.resize(meta.width * meta.height);

    // ARW2 linearization curve
    uint16_t curve[16384];
    for (int i = 0; i < 2000; i++) curve[i] = i;

    constexpr int knee_count = 8;
    const int knee_x[knee_count] = {2000, 2500, 3000, 3500, 4000, 4050, 4090, 4095};
    const int knee_y[knee_count] = {2000, 3000, 4800, 7900, 15700, 16500, 17140, 17220};
    int seg = 0;
    for (int i = 2000; i <= 4095; i++) {
        while (seg < knee_count - 2 && i >= knee_x[seg + 1]) seg++;
        float t = (float)(i - knee_x[seg]) / (knee_x[seg + 1] - knee_x[seg]);
        curve[i] = (uint16_t)(knee_y[seg] + t * (knee_y[seg + 1] - knee_y[seg]));
    }
    for (int i = 4096; i < 16384; i++) curve[i] = i;

    // Decompress
    if (compression == 32767) {
        if (!decompress_arw2(&file_data[strip_offset], strip_byte_count,
                             result.bayer.data.data(), meta.width, meta.height)) {
            result.error = "ARW2 decompression failed";
            return result;
        }
        // Apply linearization
        for (size_t i = 0; i < result.bayer.data.size(); i++) {
            uint32_t idx = result.bayer.data[i] << 1;
            if (idx < 16384) result.bayer.data[i] = curve[idx];
        }
    } else if (compression == 1) {
        size_t expected = static_cast<size_t>(meta.width) * meta.height * 2;
        if (strip_byte_count < expected) {
            result.error = "Strip data too small";
            return result;
        }
        memcpy(result.bayer.data.data(), &file_data[strip_offset], expected);
    } else {
        result.error = "Unsupported compression: " + std::to_string(compression);
        return result;
    }

    // Extract preview JPEG
    if (preview_offset != 0 && preview_length != 0 &&
        preview_offset + preview_length <= static_cast<size_t>(file_size)) {

        auto jpeg = pipe::decodeJpeg(&file_data[preview_offset], preview_length);

        if (!jpeg.rgb.empty()) {
            result.preview.width = jpeg.width;
            result.preview.height = jpeg.height;
            result.preview.data = std::move(jpeg.rgb);
            meta.preview_width = jpeg.width;
            meta.preview_height = jpeg.height;
        } else {
            meta.preview_width = meta.preview_height = 0;
        }
    } else {
        meta.preview_width = meta.preview_height = 0;
    }

    result.success = true;
    return result;
}

} // namespace pure
} // namespace sony
