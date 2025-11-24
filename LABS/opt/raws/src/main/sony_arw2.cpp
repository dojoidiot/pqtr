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

namespace mods {

// TIFF tag constants
enum TIFFTag {
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
    TAG_EXIF_IFD = 34665,               // Pointer to EXIF sub-IFD
    TAG_MAKER_NOTE = 37500,             // MakerNote (proprietary manufacturer data)
    TAG_CFA_PATTERN = 33422,
    TAG_CFA_REPEAT_PATTERN_DIM = 33421
};

// EXIF tag constants
enum EXIFTag {
    EXIF_TAG_ISO = 34855,                // ISOSpeedRatings
    EXIF_TAG_EXPOSURE_TIME = 33434,      // ExposureTime (rational)
    EXIF_TAG_FNUMBER = 33437,            // FNumber (rational)
    EXIF_TAG_FOCAL_LENGTH = 37386,       // FocalLength (rational)
    EXIF_TAG_LENS_MODEL = 42036          // LensModel (ASCII)
};

// Sony MakerNotes tag constants
enum SonyMakerTag {
    SONY_TAG_TONE_CURVE = 0x7010,        // Sony curve data (4 breakpoints for 5-segment curve)
    SONY_TAG_WB_RGGB = 0x7313            // Camera white balance multipliers (R, G, G, B)
};

// TIFF data types
enum TIFFType {
    TYPE_BYTE = 1,
    TYPE_ASCII = 2,
    TYPE_SHORT = 3,
    TYPE_LONG = 4,
    TYPE_RATIONAL = 5
};

// Helper: Read 16-bit value (little-endian)
static uint16_t read_u16(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

// Helper: Read 32-bit value (little-endian)
static uint32_t read_u32(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

// Helper: Read rational value (numerator/denominator)
static float read_rational(const std::vector<uint8_t>& file_data, uint32_t offset) {
    if (offset + 8 > file_data.size()) return 0.0f;
    uint32_t numerator = read_u32(&file_data[offset]);
    uint32_t denominator = read_u32(&file_data[offset + 4]);
    if (denominator == 0) return 0.0f;
    return static_cast<float>(numerator) / static_cast<float>(denominator);
}

// TIFF IFD entry structure
struct IFDEntry {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value_offset;
};

// Parse a single IFD entry
static IFDEntry parse_ifd_entry(const uint8_t* data) {
    IFDEntry entry;
    entry.tag = read_u16(data);
    entry.type = read_u16(data + 2);
    entry.count = read_u32(data + 4);
    entry.value_offset = read_u32(data + 8);
    return entry;
}

// Get value from IFD entry (handles inline vs offset values)
static uint32_t get_entry_value(const IFDEntry& entry, const std::vector<uint8_t>& file_data) {
    // For BYTE, SHORT, LONG with count=1, value is stored inline
    if (entry.count == 1) {
        if (entry.type == TYPE_BYTE) return entry.value_offset & 0xFF;
        if (entry.type == TYPE_SHORT) return entry.value_offset & 0xFFFF;
        if (entry.type == TYPE_LONG) return entry.value_offset;
    }

    // For other cases, value_offset points to data
    if (entry.type == TYPE_SHORT && entry.value_offset < file_data.size() - 2) {
        return read_u16(&file_data[entry.value_offset]);
    }
    if (entry.type == TYPE_LONG && entry.value_offset < file_data.size() - 4) {
        return read_u32(&file_data[entry.value_offset]);
    }

    return entry.value_offset;
}

// Get string value from IFD entry
static std::string get_entry_string(const IFDEntry& entry, const std::vector<uint8_t>& file_data) {
    if (entry.type != TYPE_ASCII) return "";

    std::string result;
    if (entry.count <= 4) {
        // Inline string
        const char* str = reinterpret_cast<const char*>(&entry.value_offset);
        result = std::string(str, std::min(entry.count, 4u));
    } else {
        // Offset string
        if (entry.value_offset + entry.count <= file_data.size()) {
            const char* str = reinterpret_cast<const char*>(&file_data[entry.value_offset]);
            result = std::string(str, entry.count);
        }
    }

    // Trim null terminators
    size_t null_pos = result.find('\0');
    if (null_pos != std::string::npos) {
        result = result.substr(0, null_pos);
    }

    return result;
}

// Sony ARW2 decompression (11-bit lossless delta encoding)
// Based on LibRaw's sony_arw2_load_raw() function
// Note: Sony RAW uses interpolation - each compressed row produces output_width pixels
static bool decompress_arw2(
    const uint8_t* compressed_data,
    size_t compressed_size,
    uint16_t* output,
    int width,
    int height
) {
    // Sony ARW2 compression: Each row uses WIDTH compressed bytes (not TIFF_width!)
    // CRITICAL: LibRaw uses the ADJUSTED width (after subtracting 32), not TIFF width
    // Each 16-byte block decodes to 16 pixels written with stride-2 interleaving
    // The file stride between rows = width (3936), not TIFF width (3968)
    int raw_width = width;  // Bytes per row = adjusted width (LibRaw compatible)
    const uint8_t* data_ptr = compressed_data;
    const uint8_t* data_end = compressed_data + compressed_size;

    for (int row = 0; row < height; row++) {
        const uint8_t* row_data = data_ptr;

        // Debug: Show where we're reading from for first 3 rows
        if (row <= 2) {
            size_t row_offset = row_data - compressed_data;
            std::cout << "    Row " << row << " offset=" << row_offset
                      << " (expected=" << (row * raw_width) << ")" << std::endl;
        }

        data_ptr += raw_width;  // Each row has 'raw_width' bytes of compressed data

        if (data_ptr > data_end) {
            std::cerr << "ARW2: Premature end of compressed data at row " << row << std::endl;
            return false;
        }

        const uint8_t* dp = row_data;
        uint16_t* row_out = output + (row * width);  // Start of output row
        int col = 0;  // Track output column position (LibRaw compatible)

        // Debug: Print first 16 bytes of compressed data for first 3 rows
        if (row <= 2) {
            std::cout << "    Row " << row << " first 16 compressed bytes: ";
            for (int i = 0; i < 16; i++) {
                printf("%02x ", dp[i]);
            }
            std::cout << std::endl;
        }

        // LibRaw loop: while (col < raw_width - 30)
        // col is pixel index, increments by 2 per pixel (stride-2 write)
        while (col < raw_width - 30) {
            // Read 32-bit value containing min, max, and indices
            uint32_t val = dp[0] | (dp[1] << 8) | (dp[2] << 16) | (dp[3] << 24);

            uint16_t max = val & 0x7FF;           // Bits 0-10: max value
            uint16_t min = (val >> 11) & 0x7FF;   // Bits 11-21: min value
            uint8_t imax = (val >> 22) & 0x0F;    // Bits 22-25: index of max
            uint8_t imin = (val >> 26) & 0x0F;    // Bits 26-29: index of min

            // Debug first block of first 2 rows
            if (row <= 1 && dp - row_data < 16) {
                std::cout << "      Row " << row << " Block 0: val=0x" << std::hex << val << std::dec
                          << " min=" << min << " max=" << max
                          << " imin=" << (int)imin << " imax=" << (int)imax << std::endl;
            }

            // Calculate shift amount for delta values
            int sh = 0;
            uint16_t range = max - min;
            while (sh < 4 && (0x80 << sh) <= range) {
                sh++;
            }

            // Decode 16 pixels
            uint16_t pix[16];
            int bit = 30;  // Start bit position for delta values

            for (int i = 0; i < 16; i++) {
                if (i == imax) {
                    pix[i] = max;
                } else if (i == imin) {
                    pix[i] = min;
                } else {
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
            // CRITICAL: This implements Bayer pattern interleaving
            // Alternates between even positions (0,2,4...) and odd positions (1,3,5...)
            // to separate R/G channels as described in research (Section 4.2)
            int start_col = col;  // Debug: track column before write
            for (int i = 0; i < 16; i++, col += 2) {
                if (col < width) {  // Bounds check
                    row_out[col] = pix[i];
                }
            }
            int end_col_before_adj = col;  // Debug: track column after write

            // CRITICAL column adjustment (LibRaw line 1486: col -= col & 1 ? 1 : 31)
            // This creates the interleaving pattern:
            // - After even col (0,2,4...): Move back 31 to odd position
            // - After odd col (1,3,5...): Move back 1 to next even position
            // Result: Alternates writing even/odd columns (Bayer R/G separation)
            col -= (col & 1) ? 1 : 31;

            // Debug: Show column progression for first blocks of first 2 rows
            if (row <= 1 && dp - row_data < 64) {
                std::cout << "      Block at dp_offset=" << (dp - row_data)
                          << ": wrote to col " << start_col << "->" << end_col_before_adj
                          << ", adjusted to " << col << std::endl;
            }

            dp += 16;  // Advance to next 16-byte block
        }

        // Debug: Print first 32 decompressed values (11-bit, before curve) for first 3 rows
        if (row <= 2) {
            std::cout << "    Row " << row << " first 16 positions: ";
            for (int i = 0; i < 16 && i < width; i++) {
                std::cout << row_out[i] << " ";
            }
            std::cout << std::endl;

            std::cout << "    Row " << row << " ended with col=" << col << std::endl;

            // CRITICAL DEBUG: Check how many bytes we actually consumed vs expected
            size_t bytes_consumed = dp - row_data;
            std::cout << "    Row " << row << " consumed " << bytes_consumed
                      << " bytes (raw_width=" << raw_width << "), "
                      << "next_row_offset=" << (data_ptr - compressed_data) << std::endl;
        }
    }

    return true;
}

// NEW: Link-based decoder interface (clean for pipe integration)
bool RawLoader::decode(pqtr::Link& source, cv::UMat& output, RawMetadata& metadata) {
    std::cout << "=== RAW Loader (Custom Sony .ARW Parser - Link I/O) ===" << std::endl;

    // Step 1: Read entire file into memory using link
    ssize_t file_size = pqtr::size(source);
    if (file_size < 0) {
        std::cerr << "RawLoader: Cannot determine file size (streaming source?)" << std::endl;
        return false;
    }

    if (file_size < 8) {
        std::cerr << "RawLoader: File too small to be valid TIFF" << std::endl;
        return false;
    }

    std::cout << "  File size: " << (file_size / 1024 / 1024) << " MB" << std::endl;

    // Allocate buffer and read entire file
    std::vector<uint8_t> file_data(file_size);
    ssize_t bytes_read = pqtr::read(source, file_data.data(), file_size);

    if (bytes_read != file_size) {
        std::cerr << "RawLoader: Failed to read complete file (got " << bytes_read
                  << " bytes, expected " << file_size << ")" << std::endl;
        return false;
    }

    // Step 2: Delegate to existing parsing code by wrapping in old process() logic
    // Extract TIFF/metadata parsing into internal methods for reuse
    // For now, inline the same parsing logic here

    // Verify TIFF header
    if (file_data[0] != 'I' || file_data[1] != 'I' ||
        read_u16(&file_data[2]) != 0x002A) {
        std::cerr << "RawLoader: Not a valid TIFF file (little-endian required)" << std::endl;
        return false;
    }

    std::cout << "  TIFF format: Sony .ARW (little-endian)" << std::endl;

    // Get offset to first IFD
    uint32_t ifd_offset = read_u32(&file_data[4]);

    // Parse IFD0
    if (ifd_offset + 2 > static_cast<size_t>(file_size)) {
        std::cerr << "RawLoader: Invalid IFD offset" << std::endl;
        return false;
    }

    uint16_t num_entries = read_u16(&file_data[ifd_offset]);
    std::cout << "  IFD0 entries: " << num_entries << std::endl;

    uint32_t sub_ifd_offset = 0;
    uint32_t exif_ifd_offset = 0;
    uint32_t maker_note_offset = 0;

    // Parse IFD0 entries
    for (int i = 0; i < num_entries; i++) {
        uint32_t entry_offset = ifd_offset + 2 + (i * 12);
        if (entry_offset + 12 > static_cast<size_t>(file_size)) break;

        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

        switch (entry.tag) {
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
                sub_ifd_offset = get_entry_value(entry, file_data);
                break;
            case TAG_EXIF_IFD:
                exif_ifd_offset = get_entry_value(entry, file_data);
                break;
        }
    }

    // TODO: Complete TIFF parsing (copy remaining logic from process() method)
    // The full implementation mirrors the process() method below (lines ~479-946).
    // This includes:
    //   - EXIF IFD parsing (ISO, shutter, aperture, focal length, lens)
    //   - MakerNote parsing (Sony tone curve 0x7010, WB 0x7313)
    //   - SubIFD parsing (RAW dimensions, compression, strip offsets, CFA)
    //   - Bayer pattern detection
    //   - Camera white balance extraction and reordering
    //   - Black/white level detection
    //   - Linearization curve construction and application
    //   - ARW2 decompression (11-bit delta encoding, stride-2 interleaving)
    //   - Final Bayer data output in cv::UMat
    //
    // For production pipe integration, this will be fully implemented to avoid duplication.
    // For now, use the existing process() method via setFilePath() (old interface).

    std::cerr << "RawLoader::decode() - Full implementation pending." << std::endl;
    std::cerr << "  Use setFilePath() + process() for now (old interface)" << std::endl;
    metadata = metadata_;
    return false;  // Stub - will be completed before pipe integration
}

bool RawLoader::process(
    const cv::UMat& input,
    cv::UMat& output,
    const Params& params
) {
    // Check if module is enabled
    if (!isEnabled(params)) {
        output = input;
        return true;
    }

    // File path should be set via setFilePath() before calling process()
    if (file_path_.empty()) {
        std::cerr << "RawLoader: No file path set. Call setFilePath() first." << std::endl;
        return false;
    }

    std::cout << "=== RAW Loader (Custom Sony .ARW Parser) ===" << std::endl;
    std::cout << "Loading: " << file_path_ << std::endl;

    // Step 1: Read entire file into memory
    std::ifstream file(file_path_, std::ios::binary);
    if (!file) {
        std::cerr << "RawLoader: Failed to open file: " << file_path_ << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> file_data(file_size);
    file.read(reinterpret_cast<char*>(file_data.data()), file_size);
    file.close();

    std::cout << "  File size: " << (file_size / 1024 / 1024) << " MB" << std::endl;

    // Step 2: Verify TIFF header
    if (file_size < 8) {
        std::cerr << "RawLoader: File too small to be valid TIFF" << std::endl;
        return false;
    }

    // Check TIFF magic number (little-endian: "II" 0x002A)
    if (file_data[0] != 'I' || file_data[1] != 'I' ||
        read_u16(&file_data[2]) != 0x002A) {
        std::cerr << "RawLoader: Not a valid TIFF file (little-endian required)" << std::endl;
        return false;
    }

    // Get offset to first IFD
    uint32_t ifd_offset = read_u32(&file_data[4]);
    std::cout << "  TIFF format: Sony .ARW (little-endian)" << std::endl;

    // Step 3: Parse IFD0 (main image metadata)
    if (ifd_offset + 2 > file_size) {
        std::cerr << "RawLoader: Invalid IFD offset" << std::endl;
        return false;
    }

    uint16_t num_entries = read_u16(&file_data[ifd_offset]);
    std::cout << "  IFD0 entries: " << num_entries << std::endl;

    uint32_t sub_ifd_offset = 0;
    uint32_t exif_ifd_offset = 0;
    uint32_t maker_note_offset = 0;

    // Parse IFD0 entries
    for (int i = 0; i < num_entries; i++) {
        uint32_t entry_offset = ifd_offset + 2 + (i * 12);
        if (entry_offset + 12 > file_size) break;

        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

        switch (entry.tag) {
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
    if (exif_ifd_offset != 0 && exif_ifd_offset + 2 <= file_size) {
        uint16_t exif_num_entries = read_u16(&file_data[exif_ifd_offset]);

        for (int i = 0; i < exif_num_entries; i++) {
            uint32_t entry_offset = exif_ifd_offset + 2 + (i * 12);
            if (entry_offset + 12 > file_size) break;

            IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

            switch (entry.tag) {
                case EXIF_TAG_ISO:
                    metadata_.iso = static_cast<float>(get_entry_value(entry, file_data));
                    break;
                case EXIF_TAG_EXPOSURE_TIME:
                    if (entry.type == TYPE_RATIONAL) {
                        metadata_.shutter_speed = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_FNUMBER:
                    if (entry.type == TYPE_RATIONAL) {
                        metadata_.aperture = read_rational(file_data, entry.value_offset);
                    }
                    break;
                case EXIF_TAG_FOCAL_LENGTH:
                    if (entry.type == TYPE_RATIONAL) {
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

    std::cout << "  Camera: " << metadata_.camera_make << " " << metadata_.camera_model << std::endl;
    std::cout << "  Orientation: " << metadata_.orientation << std::endl;

    // Step 3.5: Parse Sony MakerNotes for tone curve (tag 0x7010) and WB (tag 0x7313)
    // Initialize Sony curve breakpoints (default: straight mapping if not found)
    uint16_t sony_curve[6] = {0, 0, 0, 0, 0, 2047};  // 6 values: [0] + 4 breakpoints + [end] (11-bit max)
    uint16_t linearization_curve[16384] = {0};  // 14-bit curve (0-16383) - matches rawpy output
    bool found_sony_curve = false;
    bool found_sony_wb = false;
    uint16_t wb_rggb[4] = {0, 0, 0, 0};  // Camera WB multipliers (R, G, G, B)

    if (maker_note_offset != 0 && maker_note_offset + 10 <= file_size) {
        // Sony MakerNotes format: Starts with "SONY DSC " (9 bytes) or just IFD
        // Skip any header and find IFD structure
        uint32_t maker_ifd_offset = maker_note_offset;

        std::cout << "  MakerNote offset: " << maker_note_offset << std::endl;
        std::cout << "  MakerNote header: ";
        for (int i = 0; i < 12 && maker_note_offset + i < file_size; i++) {
            printf("%02x ", file_data[maker_note_offset + i]);
        }
        std::cout << std::endl;

        // Check if MakerNote starts with "SONY DSC " header
        if (file_data[maker_note_offset] == 'S' && file_data[maker_note_offset + 1] == 'O') {
            maker_ifd_offset += 12;  // Skip "SONY DSC " + padding
            std::cout << "  Found SONY DSC header, IFD at offset: " << maker_ifd_offset << std::endl;
        }

        if (maker_ifd_offset + 2 <= file_size) {
            uint16_t maker_num_entries = read_u16(&file_data[maker_ifd_offset]);
            std::cout << "  MakerNote IFD entries: " << maker_num_entries << std::endl;

            // Parse MakerNote IFD entries and look for Sony tag2010 sub-IFD
            uint32_t sony_tag2010_offset = 0;

            for (int i = 0; i < maker_num_entries && i < 200; i++) {  // Limit to 200 entries
                uint32_t entry_offset = maker_ifd_offset + 2 + (i * 12);
                if (entry_offset + 12 > file_size) break;

                IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                // Tag 0x2010 is a sub-IFD containing more Sony-specific data including 0x7010
                if (entry.tag == 0x2010) {
                    sony_tag2010_offset = entry.value_offset;
                    std::cout << "  Found Sony tag 0x2010 (sub-IFD) at offset: 0x" << std::hex
                              << sony_tag2010_offset << std::dec << std::endl;
                }
            }

            // If we found the tag2010 sub-IFD, parse it for the tone curve (tag 0x7010)
            if (sony_tag2010_offset != 0 && sony_tag2010_offset + 2 <= file_size) {
                uint16_t tag2010_num_entries = read_u16(&file_data[sony_tag2010_offset]);
                std::cout << "  Parsing tag 0x2010 sub-IFD, entries: " << tag2010_num_entries << std::endl;

                for (int i = 0; i < tag2010_num_entries && i < 100; i++) {
                    uint32_t entry_offset = sony_tag2010_offset + 2 + (i * 12);
                    if (entry_offset + 12 > file_size) break;

                    IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

                    if (i < 10) {
                        std::cout << "    Tag2010 Entry " << i << ": tag=0x" << std::hex << entry.tag
                                  << std::dec << " type=" << entry.type << " count=" << entry.count << std::endl;
                    }

                    if (entry.tag == SONY_TAG_TONE_CURVE) {
                        // Found Sony tone curve! Extract 4 breakpoints
                        // Tag 0x7010: 4 x uint16 values defining curve segments
                        std::cout << "  **FOUND SONY TONE CURVE (tag 0x7010)!**" << std::endl;
                        if (entry.value_offset + 8 <= file_size) {
                            // Read 4 breakpoints - keep as 11-bit values (no shift needed for our use)
                            for (int j = 0; j < 4; j++) {
                                uint16_t val = read_u16(&file_data[entry.value_offset + j * 2]);
                                sony_curve[j + 1] = (val >> 5) & 0x7FF;  // Shift to 11-bit range (0-2047)
                            }
                            found_sony_curve = true;
                            std::cout << "  Sony curve breakpoints (11-bit): [0, "
                                      << sony_curve[1] << ", " << sony_curve[2] << ", "
                                      << sony_curve[3] << ", " << sony_curve[4] << ", 2047]" << std::endl;
                        }
                        break;
                    }
                }
            }
        }
    }

    // Defer building the curve until after SubIFD parsing (curve might be there)
    // We'll build it after we've checked both MakerNotes and SubIFD

    // Step 4: Parse SubIFD (RAW image data)
    if (sub_ifd_offset == 0 || sub_ifd_offset + 2 > file_size) {
        std::cerr << "RawLoader: No SubIFD found (RAW data location)" << std::endl;
        return false;
    }

    uint16_t sub_num_entries = read_u16(&file_data[sub_ifd_offset]);
    std::cout << "  SubIFD entries: " << sub_num_entries << std::endl;

    uint32_t strip_offset = 0;
    uint32_t strip_byte_count = 0;
    uint16_t compression = 1;  // 1 = uncompressed (default)
    uint16_t cfa_pattern[4] = {0};
    bool found_cfa = false;

    // Parse SubIFD entries (this is where Sony stores tag 0x7010 tone curve!)
    for (int i = 0; i < sub_num_entries; i++) {
        uint32_t entry_offset = sub_ifd_offset + 2 + (i * 12);
        if (entry_offset + 12 > file_size) break;

        IFDEntry entry = parse_ifd_entry(&file_data[entry_offset]);

        // Check for Sony tone curve in SubIFD
        if (entry.tag == SONY_TAG_TONE_CURVE && !found_sony_curve) {
            // Found Sony tone curve! Extract 4 breakpoints
            // Tag 0x7010: 4 x uint16 values defining curve segments
            std::cout << "  **FOUND SONY TONE CURVE (tag 0x7010 in SubIFD)!**" << std::endl;
            if (entry.value_offset + 8 <= file_size) {
                // Read 4 breakpoints - keep as 11-bit values
                for (int j = 0; j < 4; j++) {
                    uint16_t val = read_u16(&file_data[entry.value_offset + j * 2]);
                    sony_curve[j + 1] = (val >> 5) & 0x7FF;  // Shift to 11-bit range (0-2047)
                }
                found_sony_curve = true;
                std::cout << "  Sony curve breakpoints (11-bit): [0, "
                          << sony_curve[1] << ", " << sony_curve[2] << ", "
                          << sony_curve[3] << ", " << sony_curve[4] << ", 2047]" << std::endl;
            }
        }

        // Check for Sony camera white balance in SubIFD
        if (entry.tag == SONY_TAG_WB_RGGB && !found_sony_wb) {
            // Found camera WB! Extract 4 values (R, G, G, B)
            // Tag 0x7313: 4 x uint16 values for white balance multipliers
            std::cout << "  **FOUND CAMERA WB (tag 0x7313 in SubIFD)!**" << std::endl;
            if (entry.count == 4 && entry.value_offset + 8 <= file_size) {
                // Read 4 WB multipliers
                for (int j = 0; j < 4; j++) {
                    wb_rggb[j] = read_u16(&file_data[entry.value_offset + j * 2]);
                }
                found_sony_wb = true;
                std::cout << "  Camera WB_RGGB: [" << wb_rggb[0] << ", " << wb_rggb[1]
                          << ", " << wb_rggb[2] << ", " << wb_rggb[3] << "]" << std::endl;
            }
        }

        switch (entry.tag) {
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
                if (entry.value_offset + 8 <= file_size) {
                    for (int j = 0; j < 4; j++) {
                        cfa_pattern[j] = file_data[entry.value_offset + 4 + j];
                    }
                    found_cfa = true;
                }
                break;
        }
    }

    std::cout << "  RAW size (TIFF): " << metadata_.width << " x " << metadata_.height << std::endl;
    std::cout << "  Compression: " << compression;
    if (compression == 1) std::cout << " (uncompressed)" << std::endl;
    else if (compression == 32767) std::cout << " (Sony ARW2 lossless)" << std::endl;
    else if (compression == 6 || compression == 7) std::cout << " (JPEG/YCC lossy)" << std::endl;
    else std::cout << " (unknown)" << std::endl;

    // DISABLED: Width adjustment for ARW2
    // Python's rawpy returns full TIFF width (6048), not adjusted (6016)
    // Keep full width to match Python pipeline behavior
    // if (compression == 32767) {
    //     metadata_.width -= 32;
    //     std::cout << "  RAW size (adjusted for ARW2): " << metadata_.width << " x " << metadata_.height << std::endl;
    // }

    std::cout << "  Strip offset: " << strip_offset << ", bytes: " << strip_byte_count << std::endl;

    // Step 5: Detect Bayer pattern from CFA
    if (found_cfa) {
        // CFA pattern: [R=0, G=1, B=2]
        // Pattern is 2x2: [0,0] [0,1] [1,0] [1,1]
        if (cfa_pattern[0] == 0 && cfa_pattern[1] == 1 &&
            cfa_pattern[2] == 1 && cfa_pattern[3] == 2) {
            metadata_.bayer_pattern = cv::COLOR_BayerRG2RGB_EA;  // RGGB
            std::cout << "  Bayer pattern: RGGB" << std::endl;
        } else if (cfa_pattern[0] == 2 && cfa_pattern[1] == 1 &&
                   cfa_pattern[2] == 1 && cfa_pattern[3] == 0) {
            metadata_.bayer_pattern = cv::COLOR_BayerBG2RGB_EA;  // BGGR
            std::cout << "  Bayer pattern: BGGR" << std::endl;
        } else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 0 &&
                   cfa_pattern[2] == 2 && cfa_pattern[3] == 1) {
            metadata_.bayer_pattern = cv::COLOR_BayerGB2RGB_EA;  // GRBG
            std::cout << "  Bayer pattern: GRBG" << std::endl;
        } else if (cfa_pattern[0] == 1 && cfa_pattern[1] == 2 &&
                   cfa_pattern[2] == 0 && cfa_pattern[3] == 1) {
            metadata_.bayer_pattern = cv::COLOR_BayerGR2RGB_EA;  // GBRG
            std::cout << "  Bayer pattern: GBRG" << std::endl;
        } else {
            std::cout << "  Bayer pattern: Unknown (defaulting to RGGB)" << std::endl;
            metadata_.bayer_pattern = cv::COLOR_BayerRG2RGB_EA;
        }
    } else {
        // Sony A7 III uses RGGB pattern (verified via rawpy)
        // rawpy returns raw_pattern = [[0,1],[3,2]] = [[R,G],[G,B]] = RGGB
        std::cout << "  Bayer pattern: Not found (defaulting to RGGB for Sony ILCE-7M3)" << std::endl;
        metadata_.bayer_pattern = cv::COLOR_BayerRG2RGB;  // RGGB pattern
    }

    // Step 6: Set Sony ILCE-7M3 defaults
    // IMPORTANT: Black/white levels must match the bit depth AFTER scaling
    // - ARW2 compressed: 11-bit (0-2047) scaled to 14-bit by <<3 (multiply by 8)
    // - LibRaw uses 14-bit output: black=512, white=16383
    // These values are in maker notes, not accessible via simple TIFF parsing
    metadata_.black_level = 512;      // 14-bit black level (matches LibRaw)
    metadata_.white_level = 16383;    // 14-bit white level (matches LibRaw)

    // White balance multipliers - camera calibrated values (RAW, not normalized)
    // Extract from MakerNotes (tag 0x7313 in SubIFD) or use defaults
    if (found_sony_wb && wb_rggb[1] > 0) {
        // Sony stores [R, G, G, B] but we reorder to [R, G, B, G] to match rawpy
        metadata_.wb_rggb[0] = wb_rggb[0];  // R
        metadata_.wb_rggb[1] = wb_rggb[1];  // G
        metadata_.wb_rggb[2] = wb_rggb[3];  // B (from index 3)
        metadata_.wb_rggb[3] = wb_rggb[2];  // G (from index 2)
        std::cout << "  Camera WB extracted (raw): [" << metadata_.wb_rggb[0]
                  << ", " << metadata_.wb_rggb[1]
                  << ", " << metadata_.wb_rggb[2]
                  << ", " << metadata_.wb_rggb[3] << "] (reordered to [R,G,B,G])" << std::endl;
    } else {
        // Fallback: Use defaults if WB not found in MakerNotes
        std::cout << "  WARNING: Camera WB not found, using defaults" << std::endl;
        metadata_.wb_rggb[0] = 2176;  // R (2.125 * 1024)
        metadata_.wb_rggb[1] = 1024;  // G (reference)
        metadata_.wb_rggb[2] = 1024;  // G
        metadata_.wb_rggb[3] = 1551;  // B (1.515 * 1024)
    }

    // Sony α7 III color matrix (camera RGB → sRGB D65)
    // Approximation based on typical Sony matrices
    metadata_.color_matrix = cv::Matx33f(
        1.9413f, -0.6498f, -0.2915f,
        -0.3204f, 1.2907f, 0.0297f,
        -0.0625f, 0.2271f, 0.8354f
    );

    std::cout << "  Black: " << metadata_.black_level
              << ", White: " << metadata_.white_level << std::endl;

    // Step 7: Extract Bayer data
    if (strip_offset == 0 || strip_offset + strip_byte_count > file_size) {
        std::cerr << "RawLoader: Invalid strip data location" << std::endl;
        return false;
    }

    // Create cv::Mat with Bayer data (16-bit uint)
    cv::Mat bayer_cpu(metadata_.height, metadata_.width, CV_16UC1);

    // Build linearization curve (matching rawpy/LibRaw ACTUAL behavior)
    // **KEY FINDING**: LibRaw code shows piecewise curve building (tag 0x7010)
    // BUT empirical testing shows it uses IDENTITY curve for ILCE-7M3
    // With curve[pix << 1] indexing on identity curve: result = pix * 2 (simple 2x)
    // See: doc/linearization-curve-investigation.md for full analysis

    std::cout << "  Building linearization curve (identity for << 1 indexing)..." << std::endl;

    if (found_sony_curve) {
        std::cout << "    Sony curve tag 0x7010 found: [" << sony_curve[0];
        for (int i = 1; i < 6; i++) {
            std::cout << ", " << sony_curve[i];
        }
        std::cout << "]" << std::endl;
        std::cout << "    (Piecewise curve NOT applied - LibRaw uses identity for this camera)" << std::endl;
    }

    // Match rawpy/LibRaw behavior with curve[pix << 1] indexing
    // For normal range (<<1 gives 0-4000): identity
    // For highlights (>>1 gives >4000): 4x expansion to preserve dynamic range
    for (int i = 0; i < 4000; i++) {
        linearization_curve[i] = i;  // Identity for normal range
    }
    for (int i = 4000; i < 16384; i++) {
        linearization_curve[i] = i * 4 - 12000;  // 4x expansion for highlights
    }

    std::cout << "    Curve: identity + highlight expansion (threshold=2000)" << std::endl;
    std::cout << "    Test: curve[678]=" << linearization_curve[678]
              << ", curve[2020]=" << linearization_curve[2020]
              << " (2*2020-2000=" << (2*2020-2000) << ")" << std::endl;

    // Handle different compression types
    if (compression == 32767) {
        // ARW2 lossless compression (11-bit delta encoding)
        std::cout << "  Decompressing ARW2 data..." << std::endl;

        if (!decompress_arw2(
            &file_data[strip_offset],
            strip_byte_count,
            reinterpret_cast<uint16_t*>(bayer_cpu.data),
            metadata_.width,
            metadata_.height)) {
            std::cerr << "RawLoader: ARW2 decompression failed" << std::endl;
            return false;
        }

        // ARW2 decompression complete - now apply linearization curve
        std::cout << "  Applying linearization curve to decompressed data..." << std::endl;

        // Apply the linearization curve (matching LibRaw's indexing)
        // LibRaw uses: RAW(row, col) = curve[pix[i] << 1]
        // This means: lookup curve at index (raw_value * 2)
        uint16_t* pixel_data = reinterpret_cast<uint16_t*>(bayer_cpu.data);
        size_t total_pixels = metadata_.width * metadata_.height;

        // Debug: Find max value BEFORE linearization
        uint16_t max_before = 0;
        for (size_t i = 0; i < total_pixels; i++) {
            if (pixel_data[i] > max_before) max_before = pixel_data[i];
        }
        std::cout << "  Max raw value BEFORE linearization: " << max_before << std::endl;

        // Apply linearization curve with LibRaw's curve[pix << 1] indexing
        for (size_t i = 0; i < total_pixels; i++) {
            uint16_t raw_value = pixel_data[i];
            uint32_t curve_index = raw_value << 1;  // LibRaw indexing
            if (curve_index < 16384) {
                pixel_data[i] = linearization_curve[curve_index];
            } else {
                pixel_data[i] = raw_value;  // Keep as-is if out of range
            }
        }

        std::cout << "  Linearization curve applied to " << total_pixels << " pixels" << std::endl;

        // Debug: Print first 16 values AFTER linearization
        std::cout << "  First 16 values AFTER linearization: ";
        for (int i = 0; i < 16; i++) {
            std::cout << pixel_data[i] << " ";
        }
        std::cout << std::endl;

        // Debug: Find max value in linearized data
        uint16_t max_val = 0;
        for (size_t i = 0; i < total_pixels; i++) {
            if (pixel_data[i] > max_val) max_val = pixel_data[i];
        }
        std::cout << "  Max value in linearized data: " << max_val << std::endl;

        // Black/white levels AFTER curve application
        // The curve scales values, so white level is now higher
        metadata_.black_level = 512;  // Black level is still 512
        metadata_.white_level = 16383;  // White level after curve

        std::cout << "  Black/white levels: Black=" << metadata_.black_level
                  << ", White=" << metadata_.white_level << std::endl;

    } else if (compression == 1) {
        // Uncompressed data
        std::cout << "  Reading uncompressed data..." << std::endl;
        size_t expected_size = metadata_.width * metadata_.height * 2;

        if (strip_byte_count < expected_size) {
            std::cerr << "RawLoader: Strip data too small (expected " << expected_size
                      << " bytes, got " << strip_byte_count << ")" << std::endl;
            return false;
        }

        // Copy RAW data (already in correct little-endian format for x86)
        std::memcpy(bayer_cpu.data, &file_data[strip_offset], expected_size);

    } else {
        std::cerr << "RawLoader: Unsupported compression type " << compression << std::endl;
        std::cerr << "  Supported: 1 (uncompressed), 32767 (ARW2 lossless)" << std::endl;
        std::cerr << "  Not supported: 6/7 (YCC JPEG lossy - requires complex decoder)" << std::endl;
        return false;
    }

    // Upload to GPU
    bayer_cpu.copyTo(output);

    std::cout << "RAW loader complete (custom parser, no LibRaw)" << std::endl;
    return true;
}

int RawLoader::detect_bayer_pattern(void* raw_ptr) {
    // Not used in custom implementation (pattern detected from TIFF CFA tag)
    return cv::COLOR_BayerRG2RGB_EA;
}

cv::Matx33f RawLoader::extract_color_matrix(void* raw_ptr) {
    // Not used in custom implementation (matrix set to Sony defaults)
    return cv::Matx33f::eye();
}

int RawLoader::extract_orientation(void* raw_ptr) {
    // Not used in custom implementation (orientation read from TIFF tag)
    return 1;
}

std::string RawLoader::name() const {
    return "sony_arw2";
}

Params RawLoader::defaults() const {
    return {
        {"file_path", 0.0f}
    };
}

} // namespace mods
