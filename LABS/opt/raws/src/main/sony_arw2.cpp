// sony_arw2.cpp
// Custom Sony .ARW (TIFF-based) RAW file loader
// NO external dependencies (LibRaw removed)
// Outputs Bayer data + metadata for pipeline

#include "sony_arw2.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <stdexcept>

namespace mods
{

    // TIFF tag constants
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
        TAG_EXIF_IFD = 34665,   // Pointer to EXIF sub-IFD
        TAG_MAKER_NOTE = 37500, // MakerNote (proprietary manufacturer data)
        TAG_CFA_PATTERN = 33422,
        TAG_CFA_REPEAT_PATTERN_DIM = 33421
    };

    // EXIF tag constants
    enum EXIFTag
    {
        EXIF_TAG_ISO = 34855,           // ISOSpeedRatings
        EXIF_TAG_EXPOSURE_TIME = 33434, // ExposureTime (rational)
        EXIF_TAG_FNUMBER = 33437,       // FNumber (rational)
        EXIF_TAG_FOCAL_LENGTH = 37386,  // FocalLength (rational)
        EXIF_TAG_LENS_MODEL = 42036     // LensModel (ASCII)
    };

    // Sony MakerNotes tag constants
    enum SonyMakerTag
    {
        SONY_TAG_TONE_CURVE = 0x7010, // Sony curve data (4 breakpoints for 5-segment curve)
        SONY_TAG_WB_RGGB = 0x7313     // Camera white balance multipliers (R, G, G, B)
    };

    // TIFF data types
    enum TIFFType
    {
        TYPE_BYTE = 1,
        TYPE_ASCII = 2,
        TYPE_SHORT = 3,
        TYPE_LONG = 4,
        TYPE_RATIONAL = 5
    };

    // Helper: Read 16-bit value (little-endian)
    static uint16_t read_u16(const uint8_t *data)
    {
        return data[0] | (data[1] << 8);
    }

    // Helper: Read 32-bit value (little-endian)
    static uint32_t read_u32(const uint8_t *data)
    {
        return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    }

    // Helper: Read rational value (numerator/denominator)
    static float read_rational(const std::vector<uint8_t> &file_data, uint32_t offset)
    {
        if (offset + 8 > file_data.size())
            return 0.0f;
        uint32_t numerator = read_u32(&file_data[offset]);
        uint32_t denominator = read_u32(&file_data[offset + 4]);
        if (denominator == 0)
            return 0.0f;
        return static_cast<float>(numerator) / static_cast<float>(denominator);
    }

    // TIFF IFD entry structure
    struct IFDEntry
    {
        uint16_t tag;
        uint16_t type;
        uint32_t count;
        uint32_t value_offset;
    };

    // Parse a single IFD entry
    static IFDEntry parse_ifd_entry(const uint8_t *data)
    {
        IFDEntry entry;
        entry.tag = read_u16(data);
        entry.type = read_u16(data + 2);
        entry.count = read_u32(data + 4);
        entry.value_offset = read_u32(data + 8);
        return entry;
    }

    // Get value from IFD entry (handles inline vs offset values)
    static uint32_t get_entry_value(const IFDEntry &entry, const std::vector<uint8_t> &file_data)
    {
        // For BYTE, SHORT, LONG with count=1, value is stored inline
        if (entry.count == 1)
        {
            if (entry.type == TYPE_BYTE)
                return entry.value_offset & 0xFF;
            if (entry.type == TYPE_SHORT)
                return entry.value_offset & 0xFFFF;
            if (entry.type == TYPE_LONG)
                return entry.value_offset;
        }

        // For other cases, value_offset points to data
        if (entry.type == TYPE_SHORT && entry.value_offset < file_data.size() - 2)
        {
            return read_u16(&file_data[entry.value_offset]);
        }
        if (entry.type == TYPE_LONG && entry.value_offset < file_data.size() - 4)
        {
            return read_u32(&file_data[entry.value_offset]);
        }

        return entry.value_offset;
    }

    // Get string value from IFD entry
    static std::string get_entry_string(const IFDEntry &entry, const std::vector<uint8_t> &file_data)
    {
        if (entry.type != TYPE_ASCII)
            return "";

        std::string result;
        if (entry.count <= 4)
        {
            // Inline string
            const char *str = reinterpret_cast<const char *>(&entry.value_offset);
            result = std::string(str, std::min(entry.count, 4u));
        }
        else
        {
            // Offset string
            if (entry.value_offset + entry.count <= file_data.size())
            {
                const char *str = reinterpret_cast<const char *>(&file_data[entry.value_offset]);
                result = std::string(str, entry.count);
            }
        }

        // Trim null terminators
        size_t null_pos = result.find('\0');
        if (null_pos != std::string::npos)
        {
            result = result.substr(0, null_pos);
        }

        return result;
    }

    // Sony ARW2 decompression (11-bit lossless delta encoding)
    // Based on LibRaw's sony_arw2_load_raw() function
    // Note: Sony RAW uses interpolation - each compressed row produces output_width pixels
    static bool decompress_arw2(
        const uint8_t *compressed_data,
        size_t compressed_size,
        uint16_t *output,
        int width,
        int height)
    {
        // Sony ARW2 compression: Each row uses WIDTH compressed bytes (not TIFF_width!)
        // CRITICAL: LibRaw uses the ADJUSTED width (after subtracting 32), not TIFF width
        // Each 16-byte block decodes to 16 pixels written with stride-2 interleaving
        // The file stride between rows = width (3936), not TIFF width (3968)
        int raw_width = width; // Bytes per row = adjusted width (LibRaw compatible)
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

            while (col < raw_width - 30)
            {
                uint32_t val = dp[0] | (dp[1] << 8) | (dp[2] << 16) | (dp[3] << 24);

                uint16_t max = val & 0x7FF;
                uint16_t min = (val >> 11) & 0x7FF;
                uint8_t imax = (val >> 22) & 0x0F;
                uint8_t imin = (val >> 26) & 0x0F;

                // Calculate shift amount for delta values
                int sh = 0;
                uint16_t range = max - min;
                while (sh < 4 && (0x80 << sh) <= range)
                {
                    sh++;
                }

                // Decode 16 pixels
                uint16_t pix[16];
                int bit = 30; // Start bit position for delta values

                for (int i = 0; i < 16; i++)
                {
                    if (i == imax)
                    {
                        pix[i] = max;
                    }
                    else if (i == imin)
                    {
                        pix[i] = min;
                    }
                    else
                    {
                        // Extract 7-bit delta value
                        int byte_offset = bit >> 3;
                        int bit_offset = bit & 7;

                        uint16_t delta_bits = dp[byte_offset] | (dp[byte_offset + 1] << 8);
                        uint16_t delta = (delta_bits >> bit_offset) & 0x7F;

                        pix[i] = (delta << sh) + min;
                        // DON'T clamp - LibRaw/rawpy allow values > 2047 for extreme highlights
                        // if (pix[i] > 0x7FF) {
                        //     pix[i] = 0x7FF;
                        // }

                        bit += 7;
                    }
                }

                // Write decoded pixels with stride-2 (LibRaw compatible)
                // Implements Bayer pattern interleaving:
                // Alternates between even positions (0,2,4...) and odd positions (1,3,5...)
                for (int i = 0; i < 16; i++, col += 2)
                {
                    if (col < width)
                    {
                        row_out[col] = pix[i];
                    }
                }

                // Column adjustment (LibRaw: col -= col & 1 ? 1 : 31)
                // After even col: move back 31 to odd position
                // After odd col: move back 1 to next even position
                col -= (col & 1) ? 1 : 31;

                dp += 16;
            }
        }

        return true;
    }

    // Link-based decoder interface (for pipe integration)
    bool RawLoader::decode(pqtr::Link &source, cv::UMat &output, RawMetadata &metadata)
    {
        ssize_t file_size = pqtr::size(source);
        if (file_size < 8)
        {
            std::cerr << "RawLoader: Invalid file size" << std::endl;
            return false;
        }

        std::vector<uint8_t> file_data(file_size);
        ssize_t bytes_read = pqtr::read(source, file_data.data(), file_size);

        if (bytes_read != file_size)
        {
            std::cerr << "RawLoader: Read error" << std::endl;
            return false;
        }

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

        for (int i = 0; i < num_entries; i++)
        {
            uint32_t entry_offset = ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > static_cast<size_t>(file_size))
                break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            switch (entry.tag)
            {
            case TAG_MAKE:
                metadata_.camera_make = get_entry_string(entry, file_data);
                break;
            case TAG_MODEL:
                metadata_.camera_model = get_entry_string(entry, file_data);
                break;
            case TAG_ORIENTATION:
                metadata_.orientation = get_entry_value(entry, file_data);
                break;
            }
        }

        // TODO: Complete implementation with full TIFF parsing from process() method
        std::cerr << "RawLoader::decode() - Stub implementation" << std::endl;
        metadata = metadata_;
        return false;
    }

    bool RawLoader::process(
        const cv::UMat &input,
        cv::UMat &output,
        const Params &params)
    {
        // Check if module is enabled
        if (!isEnabled(params))
        {
            output = input;
            return true;
        }

        // File path should be set via setFilePath() before calling process()
        if (file_path_.empty())
        {
            std::cerr << "RawLoader: No file path set. Call setFilePath() first." << std::endl;
            return false;
        }

        std::ifstream file(file_path_, std::ios::binary);
        if (!file)
        {
            std::cerr << "RawLoader: Failed to open file" << std::endl;
            return false;
        }

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> file_data(file_size);
        file.read(reinterpret_cast<char *>(file_data.data()), file_size);
        file.close();

        // Step 2: Verify TIFF header
        if (file_size < 8)
        {
            std::cerr << "RawLoader: File too small to be valid TIFF" << std::endl;
            return false;
        }

        // Check TIFF magic number (little-endian: "II" 0x002A)
        if (file_data[0] != 'I' || file_data[1] != 'I' ||
            read_u16(&file_data[2]) != 0x002A)
        {
            std::cerr << "RawLoader: Not a valid TIFF file (little-endian required)" << std::endl;
            return false;
        }

        uint32_t ifd_offset = read_u32(&file_data[4]);

        if (ifd_offset + 2 > file_size)
        {
            std::cerr << "RawLoader: Invalid IFD offset" << std::endl;
            return false;
        }

        uint16_t num_entries = read_u16(&file_data[ifd_offset]);

        uint32_t sub_ifd_offset = 0;
        uint32_t exif_ifd_offset = 0;
        uint32_t maker_note_offset = 0;

        // Parse IFD0 entries
        for (int i = 0; i < num_entries; i++)
        {
            uint32_t entry_offset = ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > file_size)
                break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            switch (entry.tag)
            {
            case TAG_MAKE:
                metadata_.camera_make = get_entry_string(entry, file_data);
                break;
            case TAG_MODEL:
                metadata_.camera_model = get_entry_string(entry, file_data);
                break;
            case TAG_ORIENTATION:
                metadata_.orientation = get_entry_value(entry, file_data);
                break;
            case TAG_SUB_IFD:
                // SubIFD contains the RAW image data
                sub_ifd_offset = get_entry_value(entry, file_data);
                break;
            case TAG_EXIF_IFD:
                // EXIF IFD contains shooting parameters
                exif_ifd_offset = get_entry_value(entry, file_data);
                break;
            }
        }

        // Parse EXIF IFD (shooting parameters)
        if (exif_ifd_offset != 0 && exif_ifd_offset + 2 <= file_size)
        {
            uint16_t exif_num_entries = read_u16(&file_data[exif_ifd_offset]);

            for (int i = 0; i < exif_num_entries; i++)
            {
                uint32_t entry_offset = exif_ifd_offset + 2 + (i * 12);
                if (entry_offset + 12 > file_size)
                    break;

                IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                switch (entry.tag)
                {
                case EXIF_TAG_ISO:
                    metadata_.iso = static_cast<float>(get_entry_value(entry, file_data));
                    break;
                case EXIF_TAG_EXPOSURE_TIME:
                    if (entry.type == TYPE_RATIONAL)
                    {
                        metadata_.shutter_speed = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_FNUMBER:
                    if (entry.type == TYPE_RATIONAL)
                    {
                        metadata_.aperture = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_FOCAL_LENGTH:
                    if (entry.type == TYPE_RATIONAL)
                    {
                        metadata_.focal_length = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_LENS_MODEL:
                    metadata_.lens_model = get_entry_string(entry, file_data);
                    break;
                case TAG_MAKER_NOTE:
                    // MakerNote contains Sony-specific data including tone curve
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

        if (maker_note_offset != 0 && maker_note_offset + 10 <= file_size)
        {
            uint32_t maker_ifd_offset = maker_note_offset;

            if (file_data[maker_note_offset] == 'S' && file_data[maker_note_offset + 1] == 'O')
            {
                maker_ifd_offset += 12;
            }

            if (maker_ifd_offset + 2 <= file_size)
            {
                uint16_t maker_num_entries = read_u16(&file_data[maker_ifd_offset]);

                // Parse MakerNote IFD entries and look for Sony tag2010 sub-IFD
                uint32_t sony_tag2010_offset = 0;

                for (int i = 0; i < maker_num_entries && i < 200; i++)
                { // Limit to 200 entries
                    uint32_t entry_offset = maker_ifd_offset + 2 + (i * 12);
                    if (entry_offset + 12 > file_size)
                        break;

                    IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                    if (entry.tag == 0x2010)
                    {
                        sony_tag2010_offset = entry.value_offset;
                    }
                }

                if (sony_tag2010_offset != 0 && sony_tag2010_offset + 2 <= file_size)
                {
                    uint16_t tag2010_num_entries = read_u16(&file_data[sony_tag2010_offset]);

                    for (int i = 0; i < tag2010_num_entries && i < 100; i++)
                    {
                        uint32_t entry_offset = sony_tag2010_offset + 2 + (i * 12);
                        if (entry_offset + 12 > file_size)
                            break;

                        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                        if (entry.tag == SONY_TAG_TONE_CURVE)
                        {
                            if (entry.value_offset + 8 <= file_size)
                            {
                                found_sony_curve = true;
                            }
                            break;
                        }
                    }
                }
            }
        }

        if (sub_ifd_offset == 0 || sub_ifd_offset + 2 > file_size)
        {
            std::cerr << "RawLoader: No SubIFD found" << std::endl;
            return false;
        }

        uint16_t sub_num_entries = read_u16(&file_data[sub_ifd_offset]);

        uint32_t strip_offset = 0;
        uint32_t strip_byte_count = 0;
        uint16_t compression = 1; // 1 = uncompressed (default)
        uint16_t cfa_pattern[4] = {0};
        bool found_cfa = false;

        // Parse SubIFD entries (this is where Sony stores tag 0x7010 tone curve!)
        for (int i = 0; i < sub_num_entries; i++)
        {
            uint32_t entry_offset = sub_ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > file_size)
                break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            if (entry.tag == SONY_TAG_TONE_CURVE && !found_sony_curve)
            {
                if (entry.value_offset + 8 <= file_size)
                {
                    found_sony_curve = true;
                }
            }

            if (entry.tag == SONY_TAG_WB_RGGB && !found_sony_wb)
            {
                if (entry.count == 4 && entry.value_offset + 8 <= file_size)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        wb_rggb[j] = read_u16(&file_data[entry.value_offset + j * 2]);
                    }
                    found_sony_wb = true;
                }
            }

            switch (entry.tag)
            {
            case TAG_IMAGE_WIDTH:
                metadata_.width = get_entry_value(entry, file_data);
                break;
            case TAG_IMAGE_LENGTH:
                metadata_.height = get_entry_value(entry, file_data);
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
                // CFA pattern (Bayer arrangement)
                if (entry.value_offset + 8 <= file_size)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        cfa_pattern[j] = file_data[entry.value_offset + 4 + j];
                    }
                    found_cfa = true;
                }
                break;
            }
        }

        if (found_cfa)
        {
            if (cfa_pattern[0] == 0 && cfa_pattern[1] == 1 &&
                cfa_pattern[2] == 1 && cfa_pattern[3] == 2)
            {
                metadata_.bayer_pattern = cv::COLOR_BayerRG2RGB_EA;
            }
            else if (cfa_pattern[0] == 2 && cfa_pattern[1] == 1 &&
                     cfa_pattern[2] == 1 && cfa_pattern[3] == 0)
            {
                metadata_.bayer_pattern = cv::COLOR_BayerBG2RGB_EA;
            }
            else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 0 &&
                     cfa_pattern[2] == 2 && cfa_pattern[3] == 1)
            {
                metadata_.bayer_pattern = cv::COLOR_BayerGB2RGB_EA;
            }
            else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 2 &&
                     cfa_pattern[2] == 0 && cfa_pattern[3] == 1)
            {
                metadata_.bayer_pattern = cv::COLOR_BayerGR2RGB_EA;
            }
            else
            {
                metadata_.bayer_pattern = cv::COLOR_BayerRG2RGB_EA;
            }
        }
        else
        {
            metadata_.bayer_pattern = cv::COLOR_BayerRG2RGB;
        }

        metadata_.black_level = 512;
        metadata_.white_level = 16383;

        if (found_sony_wb && wb_rggb[1] > 0)
        {
            metadata_.wb_rggb[0] = wb_rggb[0];
            metadata_.wb_rggb[1] = wb_rggb[1];
            metadata_.wb_rggb[2] = wb_rggb[3];
            metadata_.wb_rggb[3] = wb_rggb[2];
        }
        else
        {
            metadata_.wb_rggb[0] = 2176;
            metadata_.wb_rggb[1] = 1024;
            metadata_.wb_rggb[2] = 1024;
            metadata_.wb_rggb[3] = 1551;
        }

        metadata_.color_matrix = cv::Matx33f(
            1.9413f, -0.6498f, -0.2915f,
            -0.3204f, 1.2907f, 0.0297f,
            -0.0625f, 0.2271f, 0.8354f);

        if (strip_offset == 0 || strip_offset + strip_byte_count > file_size)
        {
            std::cerr << "RawLoader: Invalid strip data location" << std::endl;
            return false;
        }

        cv::Mat bayer_cpu(metadata_.height, metadata_.width, CV_16UC1);

        for (int i = 0; i < 4000; i++)
        {
            linearization_curve[i] = i;
        }
        for (int i = 4000; i < 16384; i++)
        {
            linearization_curve[i] = i * 4 - 12000;
        }

        if (compression == 32767)
        {
            if (!decompress_arw2(
                    &file_data[strip_offset],
                    strip_byte_count,
                    reinterpret_cast<uint16_t *>(bayer_cpu.data),
                    metadata_.width,
                    metadata_.height))
            {
                std::cerr << "RawLoader: ARW2 decompression failed" << std::endl;
                return false;
            }

            uint16_t *pixel_data = reinterpret_cast<uint16_t *>(bayer_cpu.data);
            size_t total_pixels = metadata_.width * metadata_.height;

            for (size_t i = 0; i < total_pixels; i++)
            {
                uint16_t raw_value = pixel_data[i];
                uint32_t curve_index = raw_value << 1;
                if (curve_index < 16384)
                {
                    pixel_data[i] = linearization_curve[curve_index];
                }
                else
                {
                    pixel_data[i] = raw_value;
                }
            }

            metadata_.black_level = 512;
            metadata_.white_level = 16383;
        }
        else if (compression == 1)
        {
            size_t expected_size = metadata_.width * metadata_.height * 2;

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
        return true;
    }

    int RawLoader::detect_bayer_pattern(void *raw_ptr)
    {
        // Not used in custom implementation (pattern detected from TIFF CFA tag)
        return cv::COLOR_BayerRG2RGB_EA;
    }

    cv::Matx33f RawLoader::extract_color_matrix(void *raw_ptr)
    {
        // Not used in custom implementation (matrix set to Sony defaults)
        return cv::Matx33f::eye();
    }

    int RawLoader::extract_orientation(void *raw_ptr)
    {
        // Not used in custom implementation (orientation read from TIFF tag)
        return 1;
    }

    std::string RawLoader::name() const
    {
        return "sony_arw2";
    }

    Params RawLoader::defaults() const
    {
        return {
            {"file_path", 0.0f}};
    }

} // namespace mods
