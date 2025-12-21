// sony.cpp - Sony ARW2 decoder
//
// Pure (OpenCV-free) implementation for WASM compatibility.
// Decodes Sony ARW files to BayerBuffer + metadata.

#include "gear.hpp"
#include "pipe.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace gear {
namespace sony {

namespace {

// ============================================================
// TIFF/IFD parsing utilities
// ============================================================

constexpr uint16_t TYPE_BYTE = 1;
constexpr uint16_t TYPE_ASCII = 2;
constexpr uint16_t TYPE_SHORT = 3;
constexpr uint16_t TYPE_LONG = 4;
constexpr uint16_t TYPE_RATIONAL = 5;

struct IFDEntry {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_offset;
};

uint16_t read_u16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

uint32_t read_u32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

float read_rational(const std::vector<uint8_t>& file_data, uint32_t offset) {
    if (offset + 8 > file_data.size()) return 0.0f;
    uint32_t num = read_u32(&file_data[offset]);
    uint32_t den = read_u32(&file_data[offset + 4]);
    return den == 0 ? 0.0f : static_cast<float>(num) / static_cast<float>(den);
}

IFDEntry parse_ifd_entry(const uint8_t* data) {
    IFDEntry e;
    e.tag = read_u16(data);
    e.type = read_u16(data + 2);
    e.count = read_u32(data + 4);
    e.value_offset = read_u32(data + 8);
    return e;
}

uint32_t get_entry_value(const IFDEntry& e, const std::vector<uint8_t>& f) {
    if (e.count == 1) {
        if (e.type == TYPE_BYTE) return e.value_offset & 0xFF;
        if (e.type == TYPE_SHORT) return e.value_offset & 0xFFFF;
        if (e.type == TYPE_LONG) return e.value_offset;
    }
    if (e.type == TYPE_SHORT && e.value_offset < f.size() - 2)
        return read_u16(&f[e.value_offset]);
    if (e.type == TYPE_LONG && e.value_offset < f.size() - 4)
        return read_u32(&f[e.value_offset]);
    return e.value_offset;
}

std::string get_entry_string(const IFDEntry& e, const std::vector<uint8_t>& f) {
    if (e.type != TYPE_ASCII) return "";
    std::string result;
    if (e.count <= 4) {
        const char* str = reinterpret_cast<const char*>(&e.value_offset);
        result = std::string(str, std::min(e.count, 4u));
    } else if (e.value_offset + e.count <= f.size()) {
        const char* str = reinterpret_cast<const char*>(&f[e.value_offset]);
        result = std::string(str, e.count);
    }
    size_t null_pos = result.find('\0');
    if (null_pos != std::string::npos) result = result.substr(0, null_pos);
    return result;
}

// ============================================================
// Sony ARW2 decompression
// ============================================================

bool decompress_arw2(const uint8_t* compressed, size_t comp_size,
                     uint16_t* output, int width, int height) {
    const uint8_t* ptr = compressed;
    const uint8_t* end = compressed + comp_size;

    for (int row = 0; row < height; row++) {
        const uint8_t* row_data = ptr;
        ptr += width;
        if (ptr > end) return false;

        const uint8_t* dp = row_data;
        uint16_t* row_out = output + row * width;
        int col = 0;

        while (col < width - 30) {
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
                    int byte_off = bit >> 3;
                    int bit_off = bit & 7;
                    uint16_t delta = ((dp[byte_off] | (dp[byte_off + 1] << 8)) >> bit_off) & 0x7F;
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

// ============================================================
// Tag constants
// ============================================================

enum TIFFTag {
    TAG_IMAGE_WIDTH = 256, TAG_IMAGE_LENGTH = 257, TAG_COMPRESSION = 259,
    TAG_MAKE = 271, TAG_MODEL = 272, TAG_STRIP_OFFSETS = 273,
    TAG_ORIENTATION = 274, TAG_STRIP_BYTE_COUNTS = 279, TAG_SUB_IFD = 330,
    TAG_EXIF_IFD = 34665, TAG_MAKER_NOTE = 37500, TAG_CFA_PATTERN = 33422,
    TAG_DEFAULT_CROP_ORIGIN = 0xc61f, TAG_DEFAULT_CROP_SIZE = 0xc620,
    TAG_PREVIEW_IMAGE_START = 0x0201, TAG_PREVIEW_IMAGE_LENGTH = 0x0202
};

enum EXIFTag {
    EXIF_ISO = 34855, EXIF_EXPOSURE = 33434, EXIF_FNUMBER = 33437,
    EXIF_FOCAL = 37386, EXIF_LENS = 42036
};

enum SonyTag {
    SONY_DISTORTION = 0x7037, SONY_SR2_OFFSET = 0x7200, SONY_SR2_LENGTH = 0x7201,
    SONY_SR2_KEY = 0x7221, SONY_WB_RGGB = 0x7313, SONY_COLOR_MATRIX = 0x7800,
    SONY_CONTRAST = 0x2004, SONY_SATURATION = 0x2005, SONY_SHARPNESS = 0x2006,
    SONY_CREATIVE_STYLE = 0xb020, SONY_DRO = 0xb04f
};

} // anonymous namespace

// ============================================================
// Main decode function
// ============================================================

pipe::Flow decode(const char* raw_data, size_t raw_size) {
    pipe::Flow out;

    if (raw_size < 16) {
        out.info.text("error", "File too small");
        return out;
    }

    std::vector<uint8_t> f(raw_size);
    std::memcpy(f.data(), raw_data, raw_size);

    // Check TIFF header
    if (f[0] != 'I' || f[1] != 'I' || read_u16(&f[2]) != 0x002A) {
        out.info.text("error", "Not a valid TIFF file");
        return out;
    }

    uint32_t ifd_off = read_u32(&f[4]);
    if (ifd_off + 2 > raw_size) {
        out.info.text("error", "Invalid IFD offset");
        return out;
    }

    // Metadata
    std::string make, model, lens, style = "Standard", dro_str = "Off";
    int orientation = 1;
    int8_t contrast = 0, saturation = 0, sharpness = 0;
    float iso = 0, shutter = 0, aperture = 0, focal = 0;
    uint16_t wb_rggb[4] = {2176, 1024, 1024, 1551};
    float color_matrix[9] = {1.3125f, -0.206f, -0.074f, -0.009f, 1.195f, -0.155f, 0.007f, -0.04f, 1.064f};
    bool found_wb = false;
    int16_t distortion[16] = {0};
    int distortion_count = 0;
    bool has_distortion = false;

    uint32_t sub_ifd = 0, exif_ifd = 0, maker_note = 0;
    uint32_t preview_off = 0, preview_len = 0;
    uint32_t sr2_off = 0, sr2_len = 0, sr2_key = 0;

    // Parse IFD0
    uint16_t num = read_u16(&f[ifd_off]);
    for (int i = 0; i < num; i++) {
        uint32_t off = ifd_off + 2 + i * 12;
        if (off + 12 > raw_size) break;
        IFDEntry e = parse_ifd_entry(&f[off]);

        if (e.tag == 0xc634 && e.value_offset + 100 <= raw_size) {
            uint16_t sr2_num = read_u16(&f[e.value_offset]);
            for (int j = 0; j < sr2_num && j < 20; j++) {
                uint32_t sr2_off2 = e.value_offset + 2 + j * 12;
                if (sr2_off2 + 12 > raw_size) break;
                IFDEntry se = parse_ifd_entry(&f[sr2_off2]);
                if (se.tag == SONY_SR2_OFFSET) sr2_off = se.value_offset;
                if (se.tag == SONY_SR2_LENGTH) sr2_len = se.value_offset;
                if (se.tag == SONY_SR2_KEY) sr2_key = se.value_offset;
            }
        }

        switch (e.tag) {
        case TAG_MAKE: make = get_entry_string(e, f); break;
        case TAG_MODEL: model = get_entry_string(e, f); break;
        case TAG_ORIENTATION: orientation = get_entry_value(e, f); break;
        case TAG_SUB_IFD: sub_ifd = get_entry_value(e, f); break;
        case TAG_EXIF_IFD: exif_ifd = get_entry_value(e, f); break;
        case TAG_PREVIEW_IMAGE_START: preview_off = get_entry_value(e, f); break;
        case TAG_PREVIEW_IMAGE_LENGTH: preview_len = get_entry_value(e, f); break;
        }
    }

    // Parse EXIF
    if (exif_ifd && exif_ifd + 2 <= raw_size) {
        uint16_t exif_num = read_u16(&f[exif_ifd]);
        for (int i = 0; i < exif_num; i++) {
            uint32_t off = exif_ifd + 2 + i * 12;
            if (off + 12 > raw_size) break;
            IFDEntry e = parse_ifd_entry(&f[off]);
            switch (e.tag) {
            case EXIF_ISO: iso = static_cast<float>(get_entry_value(e, f)); break;
            case EXIF_EXPOSURE: if (e.type == TYPE_RATIONAL) shutter = read_rational(f, e.value_offset); break;
            case EXIF_FNUMBER: if (e.type == TYPE_RATIONAL) aperture = read_rational(f, e.value_offset); break;
            case EXIF_FOCAL: if (e.type == TYPE_RATIONAL) focal = read_rational(f, e.value_offset); break;
            case EXIF_LENS: lens = get_entry_string(e, f); break;
            case TAG_MAKER_NOTE: maker_note = e.value_offset; break;
            }
        }
    }

    // Parse MakerNotes
    if (maker_note && maker_note + 10 <= raw_size) {
        uint32_t mk_off = maker_note;
        if (f[maker_note] == 'S' && f[maker_note + 1] == 'O') mk_off += 12;
        if (mk_off + 2 <= raw_size) {
            uint16_t mk_num = read_u16(&f[mk_off]);
            for (int i = 0; i < mk_num && i < 200; i++) {
                uint32_t off = mk_off + 2 + i * 12;
                if (off + 12 > raw_size) break;
                IFDEntry e = parse_ifd_entry(&f[off]);
                switch (e.tag) {
                case SONY_CONTRAST: contrast = static_cast<int8_t>(get_entry_value(e, f)); break;
                case SONY_SATURATION: saturation = static_cast<int8_t>(get_entry_value(e, f)); break;
                case SONY_SHARPNESS: sharpness = static_cast<int8_t>(get_entry_value(e, f)); break;
                case SONY_CREATIVE_STYLE: {
                    uint32_t v = get_entry_value(e, f);
                    switch (v) {
                    case 2: style = "Vivid"; break;
                    case 3: style = "Portrait"; break;
                    case 4: style = "Landscape"; break;
                    case 12: style = "Neutral"; break;
                    case 13: style = "Clear"; break;
                    }
                    break;
                }
                case SONY_DRO: {
                    uint32_t v = get_entry_value(e, f);
                    if (v == 1) dro_str = "Auto";
                    else if (v >= 2 && v <= 6) dro_str = "Lv" + std::to_string(v - 1);
                    break;
                }
                }
            }
        }
    }

    // Parse SubIFD
    if (!sub_ifd || sub_ifd + 2 > raw_size) {
        out.info.text("error", "No SubIFD found");
        return out;
    }

    int width = 0, height = 0;
    uint32_t strip_off = 0, strip_len = 0;
    uint16_t compression = 1;
    int bayer_pattern = 46;
    int crop_left = 0, crop_top = 0, crop_width = 0, crop_height = 0;

    uint16_t sub_num = read_u16(&f[sub_ifd]);
    for (int i = 0; i < sub_num; i++) {
        uint32_t off = sub_ifd + 2 + i * 12;
        if (off + 12 > raw_size) break;
        IFDEntry e = parse_ifd_entry(&f[off]);

        if (e.tag == SONY_WB_RGGB && !found_wb && e.count == 4 && e.value_offset + 8 <= raw_size) {
            for (int j = 0; j < 4; j++) wb_rggb[j] = read_u16(&f[e.value_offset + j * 2]);
            found_wb = true;
        }

        if (e.tag == SONY_DISTORTION && !has_distortion && e.count >= 2 && e.value_offset + e.count * 2 <= raw_size) {
            int kc = static_cast<int16_t>(read_u16(&f[e.value_offset]));
            if (kc > 0 && kc <= 16) {
                distortion_count = kc;
                for (int j = 0; j < kc; j++)
                    distortion[j] = static_cast<int16_t>(read_u16(&f[e.value_offset + (j + 1) * 2]));
                has_distortion = true;
            }
        }

        if (e.tag == SONY_SR2_OFFSET) sr2_off = get_entry_value(e, f);
        if (e.tag == SONY_SR2_LENGTH) sr2_len = get_entry_value(e, f);
        if (e.tag == SONY_SR2_KEY) sr2_key = get_entry_value(e, f);

        switch (e.tag) {
        case TAG_IMAGE_WIDTH: width = get_entry_value(e, f); break;
        case TAG_IMAGE_LENGTH: height = get_entry_value(e, f); break;
        case TAG_COMPRESSION: compression = get_entry_value(e, f); break;
        case TAG_STRIP_OFFSETS: strip_off = get_entry_value(e, f); break;
        case TAG_STRIP_BYTE_COUNTS: strip_len = get_entry_value(e, f); break;
        case TAG_CFA_PATTERN:
            if (e.value_offset + 8 <= raw_size) {
                uint8_t p[4];
                for (int j = 0; j < 4; j++) p[j] = f[e.value_offset + 4 + j];
                if (p[0] == 2 && p[3] == 0) bayer_pattern = 48;
                else if (p[0] == 1 && p[1] == 0) bayer_pattern = 47;
                else if (p[0] == 1 && p[1] == 2) bayer_pattern = 49;
            }
            break;
        case TAG_DEFAULT_CROP_ORIGIN:
            if (e.count == 2 && e.type == 4) {
                crop_left = read_u32(&f[e.value_offset]);
                crop_top = read_u32(&f[e.value_offset + 4]);
            }
            break;
        case TAG_DEFAULT_CROP_SIZE:
            if (e.count == 2 && e.type == 4) {
                crop_width = read_u32(&f[e.value_offset]);
                crop_height = read_u32(&f[e.value_offset + 4]);
            }
            break;
        }
    }

    if (crop_width == 0) { crop_width = width; crop_height = height; }

    // Parse SR2SubIFD for color matrix
    if (sr2_off > 0 && sr2_len > 0 && sr2_key != 0 && sr2_off + sr2_len <= raw_size) {
        std::vector<uint8_t> sr2(sr2_len);
        std::memcpy(sr2.data(), &f[sr2_off], sr2_len);
        decrypt_sr2(sr2.data(), sr2_len, sr2_key);

        if (sr2_len >= 2) {
            uint16_t sr2_num = read_u16(sr2.data());
            for (int i = 0; i < sr2_num && i < 200; i++) {
                uint32_t off = 2 + i * 12;
                if (off + 12 > sr2_len) break;
                IFDEntry e = parse_ifd_entry(&sr2[off]);
                if (e.tag == SONY_COLOR_MATRIX && e.count == 9) {
                    uint32_t rel = e.value_offset - sr2_off;
                    if (rel + 18 <= sr2_len) {
                        for (int j = 0; j < 9; j++)
                            color_matrix[j] = static_cast<int16_t>(read_u16(&sr2[rel + j * 2])) / 1024.0f;
                    }
                    break;
                }
            }
        }
    }

    // Validate strip
    if (!strip_off || strip_off + strip_len > raw_size) {
        out.info.text("error", "Invalid strip data");
        return out;
    }

    // Decode Bayer data
    std::vector<uint16_t> bayer(width * height);

    if (compression == 32767) {
        if (!decompress_arw2(&f[strip_off], strip_len, bayer.data(), width, height)) {
            out.info.text("error", "ARW2 decompression failed");
            return out;
        }
        // Linearization curve
        uint16_t curve[16384];
        for (int i = 0; i < 2000; i++) curve[i] = i;
        constexpr int knee_x[] = {2000, 2500, 3000, 3500, 4000, 4050, 4090, 4095};
        constexpr int knee_y[] = {2000, 3000, 4800, 7900, 15700, 16500, 17140, 17220};
        int seg = 0;
        for (int i = 2000; i <= 4095; i++) {
            while (seg < 6 && i >= knee_x[seg + 1]) seg++;
            float t = (float)(i - knee_x[seg]) / (knee_x[seg + 1] - knee_x[seg]);
            curve[i] = (uint16_t)(knee_y[seg] + t * (knee_y[seg + 1] - knee_y[seg]));
        }
        for (int i = 4096; i < 16384; i++) curve[i] = i;
        for (size_t i = 0; i < bayer.size(); i++) {
            uint32_t idx = bayer[i] << 1;
            if (idx < 16384) bayer[i] = curve[idx];
        }
    } else if (compression == 1) {
        std::memcpy(bayer.data(), &f[strip_off], width * height * 2);
    } else {
        out.info.text("error", "Unsupported compression");
        return out;
    }

    // Build output BayerBuffer with crop
    auto* buf = new BayerBuffer();
    buf->width = crop_width;
    buf->height = crop_height;
    buf->black_level = 380;
    buf->white_level = 17220;
    buf->data.resize(crop_width * crop_height);

    for (int y = 0; y < crop_height; y++) {
        const uint16_t* src = bayer.data() + (crop_top + y) * width + crop_left;
        uint16_t* dst = buf->data.data() + y * crop_width;
        std::memcpy(dst, src, crop_width * sizeof(uint16_t));
    }

    // Extract preview JPEG
    if (preview_off && preview_len && preview_off + preview_len <= raw_size) {
        auto jpeg = pipe::decodeJpeg(&f[preview_off], preview_len);
        if (!jpeg.rgb.empty()) {
            buf->preview = std::move(jpeg.rgb);
            buf->preview_width = jpeg.width;
            buf->preview_height = jpeg.height;
        }
    }

    // Build output
    out.data = buf;
    out.info.text("gear_decoder", "sony_arw2");
    out.info.text("gear_make", make);
    out.info.text("gear_model", model);
    out.info.text("gear_lens", lens);
    out.info.text("gear_creative_style", style);
    out.info.text("gear_dro", dro_str);

    out.info.dial("gear_width", static_cast<float>(crop_width));
    out.info.dial("gear_height", static_cast<float>(crop_height));
    out.info.dial("gear_black_level", 380.0f);
    out.info.dial("gear_white_level", 17220.0f);
    out.info.dial("gear_orientation", static_cast<float>(orientation));
    out.info.dial("gear_bayer_pattern", static_cast<float>(bayer_pattern));

    out.info.dial("gear_iso", iso);
    out.info.dial("gear_shutter", shutter);
    out.info.dial("gear_aperture", aperture);
    out.info.dial("gear_focal_length", focal);
    out.info.dial("gear_contrast", static_cast<float>(contrast));
    out.info.dial("gear_saturation", static_cast<float>(saturation));
    out.info.dial("gear_sharpness", static_cast<float>(sharpness));

    // White balance (normalized)
    float wb_g = (wb_rggb[1] + wb_rggb[2]) / 2.0f;
    if (wb_g > 0) {
        out.info.dial("gear_wb_r", wb_rggb[0] / wb_g);
        out.info.dial("gear_wb_g", 1.0f);
        out.info.dial("gear_wb_b", wb_rggb[3] / wb_g);
    }

    out.info.data("gear_color_matrix", color_matrix, 9);

    out.info.dial("gear_crop_left", static_cast<float>(crop_left));
    out.info.dial("gear_crop_top", static_cast<float>(crop_top));
    out.info.dial("gear_crop_width", static_cast<float>(crop_width));
    out.info.dial("gear_crop_height", static_cast<float>(crop_height));

    if (has_distortion) {
        float dp[16];
        for (int i = 0; i < distortion_count; i++) dp[i] = static_cast<float>(distortion[i]);
        out.info.data("gear_distortion", dp, distortion_count);
    }

    out.info.dial("gear_preview_width", static_cast<float>(buf->preview_width));
    out.info.dial("gear_preview_height", static_cast<float>(buf->preview_height));

    return out;
}

} // namespace sony
} // namespace gear
