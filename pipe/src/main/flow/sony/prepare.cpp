// prepare.cpp
// Sony ARW2 RAW decoder - prepare() function
// Loads RAW file from memory buffer -> Bayer buffer + metadata
// Clean-room implementation - no libraw, no OpenCV.
//
// ============================================================
// SONY ARW2 DECODE PROCESS
// ============================================================
//
// ARW2 compression (Sony Alpha cameras ~2010-2020):
//
//   1. Parse TIFF/EXIF structure to find raw data offset
//   2. Read maker notes for:
//      - White balance (tag 0x7313 or 0x7310): R,G1,G2,B multipliers
//      - Color matrix (tag 0x7310): 3x3 camera→XYZ matrix
//      - Tone curve (tag 0x7010): 4 breakpoint values
//      - Crop region, black level, etc.
//
//   3. Decompress ARW2 data:
//      - Stride-2 interleaving (even columns, then odd columns)
//      - 11-bit base + 7-bit delta encoding
//      - Apply tone curve during decode (NOT after!)
//
//   4. Tone curve (tag 0x7010):
//      - 4 values define breakpoints at segments 0-4
//      - Each segment has slope 2^segment (1, 2, 4, 8, 16)
//      - Expands 11-bit (0-2047) to ~14-bit (0-16383)
//      - CRITICAL: Without curve, shadows are crushed
//
//   5. Output: 16-bit bayer buffer ready for BLC/demosaic
//
// Reference: dcraw.c, LibRaw sony_arw2_load_raw()
// ============================================================

#include "../sony.h"
#include <iostream>
#include <vector>
#include <cstring>

// stb_image for JPEG preview decoding
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "stb_image.h"
#pragma GCC diagnostic pop

namespace sony
{
    // TIFF tag constants
    namespace internal
    {
        enum TIFFTag
        {
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
            // SR2SubIFD tags (encrypted)
            SONY_TAG_SR2_OFFSET = 0x7200,
            SONY_TAG_SR2_LENGTH = 0x7201,
            SONY_TAG_SR2_KEY = 0x7221
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
    }

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

        // SR2SubIFD (encrypted) location
        uint32_t sr2_offset = 0;
        uint32_t sr2_length = 0;
        uint32_t sr2_key = 0;

        // Parse IFD0
        for (int i = 0; i < num_entries; i++)
        {
            uint32_t entry_offset = ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > size) break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            // Tag 0xc634 contains SR2SubIFD pointers
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
        // Initialize curve to identity
        for (int i = 0; i < 16384; i++)
            linearization_curve[i] = i;

        bool found_sony_curve = false;
        uint16_t sony_curve_vals[4] = {0, 0, 0, 0};  // 4 breakpoints from tag 0x7010
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
                            // Read 4 curve breakpoint values
                            // LibRaw does: get2() >> 2 & 0xfff to get 12-bit values
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

            // Color matrix: 9 int16 values, scale by 1/1024
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

        // Decrypt and parse SR2SubIFD for color matrix
        if (sr2_offset > 0 && sr2_length > 0 && sr2_key != 0 && sr2_offset + sr2_length <= size)
        {
            std::vector<uint8_t> sr2(sr2_length);
            std::memcpy(sr2.data(), &file_data[sr2_offset], sr2_length);
            decrypt_sr2(sr2.data(), sr2_length, sr2_key);

            if (sr2_length >= 2)
            {
                uint16_t sr2_num_entries = read_u16(sr2.data());
                for (int i = 0; i < sr2_num_entries && i < 200; i++)
                {
                    uint32_t entry_off = 2 + i * 12;
                    if (entry_off + 12 > sr2_length) break;
                    IFDEntry entry = parse_ifd_entry(&sr2[entry_off]);

                    if (entry.tag == SONY_TAG_COLOR_MATRIX && entry.count == 9)
                    {
                        // Offset is relative to file start, convert to SR2 buffer offset
                        uint32_t rel_offset = entry.value_offset - sr2_offset;
                        if (rel_offset + 18 <= sr2_length)
                        {
                            for (int j = 0; j < 9; j++)
                            {
                                int16_t val = static_cast<int16_t>(read_u16(&sr2[rel_offset + j * 2]));
                                metadata.color_matrix[j] = val / 1024.0f;
                            }
                        }
                        break;
                    }
                }
            }
        }

        if (strip_offset == 0 || strip_offset + strip_byte_count > size)
        {
            std::cerr << "RawLoader: Invalid strip data location" << std::endl;
            return false;
        }

        // Allocate bayer buffer
        bayer.resize(metadata.width, metadata.height, 1);

        // Build Sony linearization curve from breakpoints
        // LibRaw algorithm: piecewise linear with slopes 1, 2, 4, 8, 16
        if (found_sony_curve && (sony_curve_vals[0] > 0 || sony_curve_vals[1] > 0))
        {
            uint16_t sony_curve[6] = {0, sony_curve_vals[0], sony_curve_vals[1],
                                      sony_curve_vals[2], sony_curve_vals[3], 4095};

            // Build curve: each segment has slope 2^segment_index
            for (int seg = 0; seg < 5; seg++)
            {
                for (int j = sony_curve[seg] + 1; j <= sony_curve[seg + 1]; j++)
                {
                    linearization_curve[j] = linearization_curve[j - 1] + (1 << seg);
                }
            }
        }
        // else: use identity curve (already initialized)

        // Decompress
        if (compression == 32767)
        {
            if (!decompress_arw2(&file_data[strip_offset], strip_byte_count,
                                 bayer.ptr(), metadata.width, metadata.height))
            {
                std::cerr << "RawLoader: ARW2 decompression failed" << std::endl;
                return false;
            }

            // Apply linearization curve
            if (found_sony_curve)
            {
                size_t total_pixels = metadata.width * metadata.height;
                for (size_t i = 0; i < total_pixels; i++)
                {
                    uint32_t curve_index = bayer.data[i] << 1;
                    if (curve_index < 16384)
                        bayer.data[i] = linearization_curve[curve_index];
                }
                // Update white level to match curve-expanded range (14-bit)
                metadata.white_level = 16383;  // LibRaw uses 16383 (0x3fff)
            }
        }
        else if (compression == 1)
        {
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

        // Extract and decode preview JPEG
        if (preview_offset != 0 && preview_length != 0 && preview_offset + preview_length <= size)
        {
            // Store original JPEG bytes
            metadata.preview_jpeg.assign(
                &file_data[preview_offset],
                &file_data[preview_offset + preview_length]);

            // Decode to RGB for diff comparison
            int pw, ph, channels;
            unsigned char* preview_data = stbi_load_from_memory(
                &file_data[preview_offset], preview_length, &pw, &ph, &channels, 3);

            if (preview_data)
            {
                metadata.preview.resize(pw, ph, 3);
                std::memcpy(metadata.preview.ptr(), preview_data, pw * ph * 3);
                stbi_image_free(preview_data);
            }
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
