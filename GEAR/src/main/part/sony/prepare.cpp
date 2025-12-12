// prepare.cpp
// Sony ARW2 RAW decoder - prepare() function
// Loads RAW file from Sink → Bayer UMat + metadata

#include "../sony.h"
#include <tool.hpp>
#include <iostream>
#include <vector>
#include <cstring>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace sony
{
    // TIFF tag constants (internal to prepare.cpp)
    namespace internal
    {
        enum TIFFTag
        {
            TAG_IMAGE_WIDTH = 256,
            TAG_IMAGE_LENGTH = 257,
            TAG_BITS_PER_SAMPLE = 258,
            TAG_COMPRESSION = 259,
            TAG_PHOTOMETRIC = 262,
            TAG_MAKE = 271,
            TAG_MODEL = 272,
            TAG_STRIP_OFFSETS = 273,
            TAG_ORIENTATION = 274,
            TAG_SAMPLES_PER_PIXEL = 277,
            TAG_ROWS_PER_STRIP = 278,
            TAG_STRIP_BYTE_COUNTS = 279,
            TAG_SOFTWARE = 305,
            TAG_DATETIME = 306,
            TAG_SUB_IFD = 330,
            TAG_EXIF_IFD = 34665,
            TAG_MAKER_NOTE = 37500,
            TAG_CFA_PATTERN = 33422,
            TAG_CFA_REPEAT_PATTERN_DIM = 33421,
            // DNG crop tags
            TAG_DEFAULT_CROP_ORIGIN = 0xc61f,
            TAG_DEFAULT_CROP_SIZE = 0xc620,
            // Preview image (IFD0)
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
            SONY_TAG_DISTORTION_CORR_PARAMS = 0x7037,  // Lens distortion correction (int16s[17])
            SONY_TAG_SR2_SUBIFD_OFFSET = 0x7200,      // SR2SubIFD offset (encrypted IFD)
            SONY_TAG_SR2_SUBIFD_LENGTH = 0x7201,      // SR2SubIFD length
            SONY_TAG_SR2_SUBIFD_KEY = 0x7221,         // SR2SubIFD decryption key
            SONY_TAG_WB_RGGB = 0x7313,
            SONY_TAG_COLOR_MATRIX = 0x7800,           // Color matrix (int16s[9], /1024) - in SR2SubIFD
            // Style metadata (in MakerNotes)
            SONY_TAG_CONTRAST = 0x2004,
            SONY_TAG_SATURATION = 0x2005,
            SONY_TAG_SHARPNESS = 0x2006,
            SONY_TAG_CREATIVE_STYLE = 0xb020,
            SONY_TAG_DRO = 0xb04f
        };

        // Sony SR2SubIFD decryption (Dave Coffin's algorithm from dcraw)
        void decrypt_sr2(uint8_t* data, uint32_t length, uint32_t key)
        {
            uint32_t pad[128] = {0};  // Initialize to zero (dcraw uses static)
            uint32_t p;

            // Initialize pad array
            for (p = 0; p < 4; p++)
                pad[p] = key = key * 48828125 + 1;
            pad[3] = pad[3] << 1 | (pad[0] ^ pad[2]) >> 31;
            for (p = 4; p < 127; p++)
                pad[p] = (pad[p-4] ^ pad[p-2]) << 1 | (pad[p-3] ^ pad[p-1]) >> 31;
            // Convert to big-endian (htonl equivalent)
            for (p = 0; p < 127; p++)
                pad[p] = ((pad[p] & 0xff) << 24) | ((pad[p] & 0xff00) << 8) |
                         ((pad[p] >> 8) & 0xff00) | ((pad[p] >> 24) & 0xff);

            // Decrypt data
            uint32_t* d = reinterpret_cast<uint32_t*>(data);
            p = 127;  // Start at 127 (after initialization loops)
            for (uint32_t i = 0; i < length / 4; i++)
            {
                p++;
                d[i] ^= pad[(p-1) & 127] = pad[p & 127] ^ pad[(p+64) & 127];
            }
        }
    }

    bool Decoder::prepare(pqtr::Sink &source, cv::UMat &output, Info &info, RawMetadata &metadata)
    {
        using namespace internal;

        int file_size = source.size();

        if (file_size < 8)
        {
            std::cerr << "RawLoader: Invalid file size" << std::endl;
            return false;
        }

        // Read entire file from sink into buffer
        std::vector<uint8_t> file_data(file_size);
        char *data_ptr = nullptr;
        int bytes_read = source.take(data_ptr, file_size);

        if (bytes_read != file_size)
        {
            if (data_ptr)
                delete[] data_ptr;
            std::cerr << "RawLoader: Read error (got " << bytes_read << " expected " << file_size << ")" << std::endl;
            return false;
        }

        if (!data_ptr)
        {
            std::cerr << "RawLoader: Null data pointer" << std::endl;
            return false;
        }

        // Copy from Sink buffer to our vector
        memcpy(file_data.data(), data_ptr, bytes_read);
        delete[] data_ptr;

        // Check for little-endian TIFF header ('II') and magic number (42)
        if (file_data[0] != 'I' || file_data[1] != 'I' || read_u16(&file_data[2]) != 0x002A)
        {
            std::cerr << "RawLoader: Not a valid TIFF file" << std::endl;
            return false;
        }

        uint32_t ifd_offset = read_u32(&file_data[4]);
        if (ifd_offset + 2 > static_cast<size_t>(file_size))
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

        // SR2SubIFD (encrypted IFD containing ColorMatrix) - will be populated from DNGPrivateData
        uint32_t sr2_offset = 0;
        uint32_t sr2_length = 0;
        uint32_t sr2_key = 0;

        // Parse IFD0 entries
        for (int i = 0; i < num_entries; i++)
        {
            uint32_t entry_offset = ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > static_cast<size_t>(file_size))
                break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            // DNGPrivateData (0xc634) points to SR2 IFD structure directly
            if (entry.tag == 0xc634)
            {
                // The SR2 IFD starts directly at this offset
                uint32_t sr2_ifd_offset = entry.value_offset;

                if (sr2_ifd_offset + 100 <= static_cast<size_t>(file_size))
                {
                    uint16_t sr2_dir_entries = read_u16(&file_data[sr2_ifd_offset]);

                    for (int j = 0; j < sr2_dir_entries && j < 20; j++)
                    {
                        uint32_t sr2_entry_offset = sr2_ifd_offset + 2 + (j * 12);
                        if (sr2_entry_offset + 12 > static_cast<size_t>(file_size))
                            break;
                        IFDEntry sr2_entry = parse_ifd_entry(&file_data[sr2_entry_offset]);

                        if (sr2_entry.tag == SONY_TAG_SR2_SUBIFD_OFFSET)
                            sr2_offset = sr2_entry.value_offset;
                        if (sr2_entry.tag == SONY_TAG_SR2_SUBIFD_LENGTH)
                            sr2_length = sr2_entry.value_offset;
                        if (sr2_entry.tag == SONY_TAG_SR2_SUBIFD_KEY)
                            sr2_key = sr2_entry.value_offset;
                    }
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

        // Parse EXIF IFD (shooting parameters)
        if (exif_ifd_offset != 0 && exif_ifd_offset + 2 <= static_cast<size_t>(file_size))
        {
            uint16_t exif_num_entries = read_u16(&file_data[exif_ifd_offset]);

            for (int i = 0; i < exif_num_entries; i++)
            {
                uint32_t entry_offset = exif_ifd_offset + 2 + (i * 12);
                if (entry_offset + 12 > static_cast<size_t>(file_size))
                    break;

                IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                switch (entry.tag)
                {
                case EXIF_TAG_ISO:
                    metadata.iso = static_cast<float>(get_entry_value(entry, file_data));
                    break;
                case EXIF_TAG_EXPOSURE_TIME:
                    if (entry.type == TYPE_RATIONAL)
                    {
                        metadata.shutter_speed = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_FNUMBER:
                    if (entry.type == TYPE_RATIONAL)
                    {
                        metadata.aperture = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_FOCAL_LENGTH:
                    if (entry.type == TYPE_RATIONAL)
                    {
                        metadata.focal_length = read_rational(file_data, entry.value_offset);
                    }
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

        // Parse Sony MakerNotes for tone curve (tag 0x7010) and WB (tag 0x7313)
        uint16_t linearization_curve[16384] = {0};
        bool found_sony_curve = false;
        bool found_sony_wb = false;
        uint16_t wb_rggb[4] = {0, 0, 0, 0};

        // Color matrix from tag 0x7800 (int16s[9], values /1024)
        bool found_color_matrix = false;
        int16_t color_matrix_raw[9] = {0};

        // Initialize style metadata defaults
        metadata.creative_style = "Standard";
        metadata.dro = "Off";
        metadata.contrast = 0;
        metadata.saturation = 0;
        metadata.sharpness = 0;

        // Initialize distortion params
        metadata.has_distortion_params = false;
        metadata.distortion_knot_count = 0;
        memset(metadata.distortion_params, 0, sizeof(metadata.distortion_params));

        if (maker_note_offset != 0 && maker_note_offset + 10 <= static_cast<size_t>(file_size))
        {
            uint32_t maker_ifd_offset = maker_note_offset;

            // Sony MakerNote can start with a header like "SONY CAM" (12 bytes)
            if (file_data[maker_note_offset] == 'S' && file_data[maker_note_offset + 1] == 'O')
            {
                maker_ifd_offset += 12;
            }

            if (maker_ifd_offset + 2 <= static_cast<size_t>(file_size))
            {
                uint16_t maker_num_entries = read_u16(&file_data[maker_ifd_offset]);

                uint32_t sony_tag2010_offset = 0;

                for (int i = 0; i < maker_num_entries && i < 200; i++)
                {
                    uint32_t entry_offset = maker_ifd_offset + 2 + (i * 12);
                    if (entry_offset + 12 > static_cast<size_t>(file_size))
                        break;

                    IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                    // Tag 0x2010 is a Sony-specific sub-IFD with more metadata
                    if (entry.tag == 0x2010)
                    {
                        sony_tag2010_offset = entry.value_offset;
                    }

                    // Style metadata from MakerNotes
                    // Sony stores these as signed values: -3 to +3 with 0 = Normal
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
                        case 5: metadata.creative_style = "Sunset"; break;
                        case 6: metadata.creative_style = "Night View"; break;
                        case 7: metadata.creative_style = "Autumn Leaves"; break;
                        case 8: metadata.creative_style = "Black & White"; break;
                        case 9: metadata.creative_style = "Sepia"; break;
                        case 12: metadata.creative_style = "Neutral"; break;
                        case 13: metadata.creative_style = "Clear"; break;
                        case 14: metadata.creative_style = "Deep"; break;
                        case 15: metadata.creative_style = "Light"; break;
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

                if (sony_tag2010_offset != 0 && sony_tag2010_offset + 2 <= static_cast<size_t>(file_size))
                {
                    uint16_t tag2010_num_entries = read_u16(&file_data[sony_tag2010_offset]);

                    for (int i = 0; i < tag2010_num_entries && i < 100; i++)
                    {
                        uint32_t entry_offset = sony_tag2010_offset + 2 + (i * 12);
                        if (entry_offset + 12 > static_cast<size_t>(file_size))
                            break;

                        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                        if (entry.tag == SONY_TAG_TONE_CURVE)
                        {
                            if (entry.value_offset + 8 <= static_cast<size_t>(file_size))
                            {
                                found_sony_curve = true;
                            }
                            break;
                        }
                    }
                }
            }
        }

        if (sub_ifd_offset == 0 || sub_ifd_offset + 2 > static_cast<size_t>(file_size))
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

        // Crop metadata from DNG tags
        bool found_crop_origin = false;
        bool found_crop_size = false;
        int crop_origin[2] = {0, 0};  // left, top
        int crop_size[2] = {0, 0};    // width, height

        // Parse SubIFD entries
        for (int i = 0; i < sub_num_entries; i++)
        {
            uint32_t entry_offset = sub_ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > static_cast<size_t>(file_size))
                break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            if (entry.tag == SONY_TAG_TONE_CURVE && !found_sony_curve)
            {
                if (entry.value_offset + 8 <= static_cast<size_t>(file_size))
                {
                    found_sony_curve = true;
                }
            }

            if (entry.tag == SONY_TAG_WB_RGGB && !found_sony_wb)
            {
                if (entry.count == 4 && entry.value_offset + 8 <= static_cast<size_t>(file_size))
                {
                    for (int j = 0; j < 4; j++)
                    {
                        wb_rggb[j] = read_u16(&file_data[entry.value_offset + j * 2]);
                    }
                    found_sony_wb = true;
                }
            }

            // Distortion correction params: tag 0x7037, int16s[N+1]
            // First value is knot count (1-16), followed by that many coefficients
            if (entry.tag == SONY_TAG_DISTORTION_CORR_PARAMS && !metadata.has_distortion_params)
            {
                if (entry.count >= 2 && entry.value_offset + entry.count * 2 <= static_cast<size_t>(file_size))
                {
                    // First value is the knot count
                    int knot_count = static_cast<int16_t>(read_u16(&file_data[entry.value_offset]));
                    if (knot_count > 0 && knot_count <= 16 && knot_count <= static_cast<int>(entry.count) - 1)
                    {
                        metadata.distortion_knot_count = knot_count;
                        for (int j = 0; j < knot_count; j++)
                        {
                            metadata.distortion_params[j] = static_cast<int16_t>(
                                read_u16(&file_data[entry.value_offset + (j + 1) * 2]));
                        }
                        metadata.has_distortion_params = true;
                    }
                }
            }

            // Color matrix: tag 0x7800, int16s[9] (3x3 matrix, values /1024)
            // Note: This tag is in SR2SubIFD (encrypted), not here. Kept for fallback.
            if (entry.tag == SONY_TAG_COLOR_MATRIX && !found_color_matrix)
            {
                if (entry.count == 9 && entry.value_offset + 18 <= static_cast<size_t>(file_size))
                {
                    for (int j = 0; j < 9; j++)
                    {
                        color_matrix_raw[j] = static_cast<int16_t>(
                            read_u16(&file_data[entry.value_offset + j * 2]));
                    }
                    found_color_matrix = true;
                }
            }

            // SR2SubIFD info (encrypted IFD containing ColorMatrix at 0x7800)
            if (entry.tag == SONY_TAG_SR2_SUBIFD_OFFSET)
                sr2_offset = get_entry_value(entry, file_data);
            if (entry.tag == SONY_TAG_SR2_SUBIFD_LENGTH)
                sr2_length = get_entry_value(entry, file_data);
            if (entry.tag == SONY_TAG_SR2_SUBIFD_KEY)
                sr2_key = get_entry_value(entry, file_data);

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
                if (entry.value_offset + 8 <= static_cast<size_t>(file_size))
                {
                    for (int j = 0; j < 4; j++)
                    {
                        cfa_pattern[j] = file_data[entry.value_offset + 4 + j];
                    }
                    found_cfa = true;
                }
                break;
            case TAG_DEFAULT_CROP_ORIGIN:
                // DNG DefaultCropOrigin - can be LONG (type 4) or RATIONAL (type 5)
                if (entry.count == 2)
                {
                    if (entry.type == 4) // LONG
                    {
                        crop_origin[0] = read_u32(&file_data[entry.value_offset]);
                        crop_origin[1] = read_u32(&file_data[entry.value_offset + 4]);
                        found_crop_origin = true;
                    }
                    else if (entry.type == 5) // RATIONAL (num/denom pairs)
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
                // DNG DefaultCropSize - can be LONG (type 4) or RATIONAL (type 5)
                if (entry.count == 2)
                {
                    if (entry.type == 4) // LONG
                    {
                        crop_size[0] = read_u32(&file_data[entry.value_offset]);
                        crop_size[1] = read_u32(&file_data[entry.value_offset + 4]);
                        found_crop_size = true;
                    }
                    else if (entry.type == 5) // RATIONAL
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

        if (found_cfa)
        {
            // These numeric codes are custom to this project. They are mapped to
            // standard OpenCV ColorConversionCodes in the demosaic module.
            if (cfa_pattern[0] == 0 && cfa_pattern[1] == 1 && // R G
                cfa_pattern[2] == 1 && cfa_pattern[3] == 2)   // G B
            {
                metadata.bayer_pattern = 46; // RGGB
            }
            else if (cfa_pattern[0] == 2 && cfa_pattern[1] == 1 && // B G
                     cfa_pattern[2] == 1 && cfa_pattern[3] == 0)   // G R
            {
                metadata.bayer_pattern = 48; // BGGR (Note: custom code 44 was incorrect)
            }
            else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 0 && // G R
                     cfa_pattern[2] == 2 && cfa_pattern[3] == 1)   // B G
            {
                metadata.bayer_pattern = 47; // GRBG
            }
            else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 2 && // G B
                     cfa_pattern[2] == 0 && cfa_pattern[3] == 1)   // R G
            {
                metadata.bayer_pattern = 49; // GBRG (Note: custom code 45 was incorrect)
            }
            else
            {
                metadata.bayer_pattern = 46; // Default to RGGB
            }
        }
        else
        {
            metadata.bayer_pattern = 46; // Default to RGGB
        }

        // Set black/white levels for Sony ARW2 sensors.
        // After linearization with <<1 indexing:
        //   - Optical black ~190 raw → curve[380] → 380 output
        //   - White clip ~4095 raw → curve[8190] → 17220 output
        // Using 380 as black level (matches observed sensor floor after curve)
        // Using 17220 as white level (curve maximum)
        metadata.black_level = 380;
        metadata.white_level = 17220;

        if (found_sony_wb && wb_rggb[1] > 0)
        {
            metadata.wb_rggb[0] = wb_rggb[0];
            metadata.wb_rggb[1] = wb_rggb[1];
            metadata.wb_rggb[2] = wb_rggb[3]; // Swap G2 and B, as per MakerNote format
            metadata.wb_rggb[3] = wb_rggb[2];
        }
        else
        {
            // Fallback WB multipliers for daylight
            metadata.wb_rggb[0] = 2176;
            metadata.wb_rggb[1] = 1024;
            metadata.wb_rggb[2] = 1551;
            metadata.wb_rggb[3] = 1024;
        }

        // Parse SR2SubIFD for ColorMatrix (tag 0x7800)
        // SR2SubIFD is encrypted; decrypt using Dave Coffin's algorithm
        if (sr2_offset > 0 && sr2_length > 0 && sr2_key != 0 && !found_color_matrix)
        {
            if (sr2_offset + sr2_length <= static_cast<size_t>(file_size))
            {
                // Copy and decrypt SR2SubIFD data
                std::vector<uint8_t> sr2_data(sr2_length);
                memcpy(sr2_data.data(), &file_data[sr2_offset], sr2_length);
                decrypt_sr2(sr2_data.data(), sr2_length, sr2_key);

                // Parse decrypted IFD
                if (sr2_length >= 2)
                {
                    uint16_t sr2_num_entries = read_u16(sr2_data.data());
                    for (int i = 0; i < sr2_num_entries && i < 200; i++)
                    {
                        uint32_t entry_offset = 2 + (i * 12);
                        if (entry_offset + 12 > sr2_length)
                            break;

                        IFDEntry entry = parse_ifd_entry(&sr2_data[entry_offset]);

                        if (entry.tag == SONY_TAG_COLOR_MATRIX && entry.count == 9)
                        {
                            // Offset in IFD entry is file offset; convert to relative
                            uint32_t rel_offset = entry.value_offset - sr2_offset;
                            if (rel_offset + 18 <= sr2_length)
                            {
                                for (int j = 0; j < 9; j++)
                                {
                                    color_matrix_raw[j] = static_cast<int16_t>(
                                        read_u16(&sr2_data[rel_offset + j * 2]));
                                }
                                found_color_matrix = true;
                            }
                            break;
                        }
                    }
                }
            }
        }

        // Color matrix: camera RGB → linear sRGB
        // Read from Sony metadata tag 0x7800 (SR2SubIFD "ColorMatrix")
        // Values are fixed-point /1024, 3x3 row-major
        if (found_color_matrix)
        {
            metadata.color_matrix = cv::Matx33f(
                color_matrix_raw[0] / 1024.0f, color_matrix_raw[1] / 1024.0f, color_matrix_raw[2] / 1024.0f,
                color_matrix_raw[3] / 1024.0f, color_matrix_raw[4] / 1024.0f, color_matrix_raw[5] / 1024.0f,
                color_matrix_raw[6] / 1024.0f, color_matrix_raw[7] / 1024.0f, color_matrix_raw[8] / 1024.0f);
        }
        else
        {
            // Fallback: hardcoded matrix (from DSC00458.ARW)
            metadata.color_matrix = cv::Matx33f(
                1344.0f / 1024.0f, -211.0f / 1024.0f,  -76.0f / 1024.0f,
                  -9.0f / 1024.0f, 1224.0f / 1024.0f, -159.0f / 1024.0f,
                   7.0f / 1024.0f,  -41.0f / 1024.0f, 1090.0f / 1024.0f);
        }

        // Active area crop - removes optical black borders
        // Read from DNG DefaultCropOrigin/DefaultCropSize tags
        if (found_crop_origin && found_crop_size)
        {
            metadata.crop_left = crop_origin[0];
            metadata.crop_top = crop_origin[1];
            metadata.crop_width = crop_size[0];
            metadata.crop_height = crop_size[1];
        }
        else
        {
            // Fallback: no crop (use full sensor)
            metadata.crop_left = 0;
            metadata.crop_top = 0;
            metadata.crop_width = metadata.width;
            metadata.crop_height = metadata.height;
        }

        if (strip_offset == 0 || strip_offset + strip_byte_count > static_cast<size_t>(file_size))
        {
            std::cerr << "RawLoader: Invalid strip data location" << std::endl;
            return false;
        }

        cv::Mat bayer_cpu(metadata.height, metadata.width, CV_16UC1);

        // Sony ARW2 linearization curve (clean-room implementation)
        // Maps 11-bit decoded values to expanded 14-bit linear values.
        //
        // LibRaw uses <<1 indexing: output = curve[raw_value << 1]
        // The curve has 8192 entries:
        //   indices 0-1999: identity (curve[i] = i)
        //   indices 2000-4095: expansion (2000→2000, 4095→17220)
        //   indices 4096+: identity (overflow)
        //
        // With <<1 indexing, raw_value maps to curve index as:
        //   raw_value 0-999 → indices 0-1998 → identity
        //   raw_value 1000-2047 → indices 2000-4094 → expansion
        //   raw_value 2048+ → indices 4096+ → overflow
        //
        // Key points from LibRaw CSV:
        //   (2000,2000), (2500,3000), (3000,4800), (3500,7900),
        //   (4000,15700), (4050,16500), (4090,17140), (4095,17220)

        // Region 0-1999: identity
        for (int i = 0; i < 2000; i++)
        {
            linearization_curve[i] = i;
        }

        // Region 2000-4095: piecewise linear expansion
        constexpr int knee_count = 8;
        const int knee_x[knee_count] = {2000, 2500, 3000, 3500, 4000, 4050, 4090, 4095};
        const int knee_y[knee_count] = {2000, 3000, 4800, 7900, 15700, 16500, 17140, 17220};

        int seg = 0;
        for (int i = 2000; i <= 4095; i++)
        {
            while (seg < knee_count - 2 && i >= knee_x[seg + 1])
                seg++;

            float t = (float)(i - knee_x[seg]) / (knee_x[seg + 1] - knee_x[seg]);
            linearization_curve[i] = (uint16_t)(knee_y[seg] + t * (knee_y[seg + 1] - knee_y[seg]));
        }

        // Region 4096+: identity (overflow protection)
        for (int i = 4096; i < 16384; i++)
        {
            linearization_curve[i] = i;
        }

        // 32767 is the proprietary compression code for Sony ARW2
        if (compression == 32767)
        {
            if (!decompress_arw2(
                    &file_data[strip_offset],
                    strip_byte_count,
                    reinterpret_cast<uint16_t *>(bayer_cpu.data),
                    metadata.width,
                    metadata.height))
            {
                std::cerr << "RawLoader: ARW2 decompression failed" << std::endl;
                return false;
            }

            // Apply linearization curve to all ARW2 decoded pixels
            // NOTE: This curve is integral to ARW2 decoding, not optional.
            // The ARW2 decompressor outputs 11-bit values (0-~2047 typical).
            // The linearization curve maps these to 14-bit linear values.
            // LibRaw uses <<1 indexing: output = curve[raw_value << 1]
            {
                uint16_t *pixel_data = reinterpret_cast<uint16_t *>(bayer_cpu.data);
                size_t total_pixels = metadata.width * metadata.height;

                for (size_t i = 0; i < total_pixels; i++)
                {
                    uint16_t raw_value = pixel_data[i];
                    // <<1 indexing matches LibRaw's curve format
                    uint32_t curve_index = raw_value << 1;
                    if (curve_index < 16384)
                    {
                        pixel_data[i] = linearization_curve[curve_index];
                    }
                }
            }
        }
        else if (compression == 1) // Uncompressed
        {
            size_t expected_size = static_cast<size_t>(metadata.width) * metadata.height * 2;

            if (strip_byte_count < expected_size)
            {
                std::cerr << "RawLoader: Strip data too small" << std::endl;
                return false;
            }

            std::memcpy(bayer_cpu.data, &file_data[strip_offset], expected_size);
        }
        else
        {
            std::cerr << "RawLoader: Unsupported compression type " << compression << std::endl;
            return false;
        }

        bayer_cpu.copyTo(output);

        // Extract embedded preview JPEG
        if (preview_offset != 0 && preview_length != 0 &&
            preview_offset + preview_length <= static_cast<size_t>(file_size))
        {
            // Decode JPEG from raw bytes
            std::vector<uint8_t> jpeg_data(
                file_data.begin() + preview_offset,
                file_data.begin() + preview_offset + preview_length);

            cv::Mat preview_cpu = cv::imdecode(jpeg_data, cv::IMREAD_COLOR);
            if (!preview_cpu.empty())
            {
                // Keep as BGR (OpenCV convention)
                preview_cpu.copyTo(metadata.preview);
                metadata.preview_width = preview_cpu.cols;
                metadata.preview_height = preview_cpu.rows;
            }
            else
            {
                std::cerr << "RawLoader: Failed to decode embedded preview JPEG" << std::endl;
                metadata.preview_width = 0;
                metadata.preview_height = 0;
            }
        }
        else
        {
            metadata.preview_width = 0;
            metadata.preview_height = 0;
        }

        // Populate info map for external consumers (e.g., embedding in PNG)
        info["camera_make"] = metadata.camera_make;
        info["camera_model"] = metadata.camera_model;
        info["width"] = std::to_string(metadata.width);
        info["height"] = std::to_string(metadata.height);
        info["black_level"] = std::to_string(metadata.black_level);
        info["white_level"] = std::to_string(metadata.white_level);
        info["bayer_pattern"] = std::to_string(metadata.bayer_pattern);
        info["iso"] = std::to_string(metadata.iso);
        info["shutter_speed"] = std::to_string(metadata.shutter_speed);
        info["aperture"] = std::to_string(metadata.aperture);
        info["focal_length"] = std::to_string(metadata.focal_length);
        info["lens_model"] = metadata.lens_model;

        return true;
    }

} // namespace sony
