// prepare.cpp
// Sony ARW2 RAW decoder - prepare() function
// Loads RAW file from memory buffer -> Bayer buffer + metadata
// Clean-room implementation - no libraw, no OpenCV.

#include "../sony.h"
#include <iostream>
#include <vector>
#include <cstring>

// stb_image for JPEG preview decoding
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include "stb_image.h"

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
            SONY_TAG_TONE_CURVE = 0x7010,
            SONY_TAG_DISTORTION_CORR_PARAMS = 0x7037,
            SONY_TAG_WB_RGGB = 0x7313,
            SONY_TAG_CONTRAST = 0x2004,
            SONY_TAG_SATURATION = 0x2005,
            SONY_TAG_SHARPNESS = 0x2006,
            SONY_TAG_CREATIVE_STYLE = 0xb020,
            SONY_TAG_DRO = 0xb04f
        };
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

        // Parse IFD0
        for (int i = 0; i < num_entries; i++)
        {
            uint32_t entry_offset = ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > size) break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

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
        bool found_sony_curve = false;
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

                        if (entry.tag == SONY_TAG_TONE_CURVE && entry.value_offset + 8 <= size)
                        {
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

            if (entry.tag == SONY_TAG_TONE_CURVE && !found_sony_curve && entry.value_offset + 8 <= size)
                found_sony_curve = true;

            if (entry.tag == SONY_TAG_WB_RGGB && !found_sony_wb && entry.count == 4 && entry.value_offset + 8 <= size)
            {
                for (int j = 0; j < 4; j++)
                    wb_rggb[j] = read_u16(&file_data[entry.value_offset + j * 2]);
                found_sony_wb = true;
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

        if (strip_offset == 0 || strip_offset + strip_byte_count > size)
        {
            std::cerr << "RawLoader: Invalid strip data location" << std::endl;
            return false;
        }

        // Allocate bayer buffer
        bayer.resize(metadata.width, metadata.height, 1);

        // Build linearization curve
        for (int i = 0; i < 4000; i++)
            linearization_curve[i] = i;
        for (int i = 4000; i < 16384; i++)
            linearization_curve[i] = i * 4 - 12000;

        // Decompress
        if (compression == 32767)
        {
            if (!decompress_arw2(&file_data[strip_offset], strip_byte_count,
                                 bayer.ptr(), metadata.width, metadata.height))
            {
                std::cerr << "RawLoader: ARW2 decompression failed" << std::endl;
                return false;
            }

            if (found_sony_curve)
            {
                size_t total_pixels = metadata.width * metadata.height;
                for (size_t i = 0; i < total_pixels; i++)
                {
                    uint32_t curve_index = bayer.data[i] << 1;
                    if (curve_index < 16384)
                        bayer.data[i] = linearization_curve[curve_index];
                }
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
