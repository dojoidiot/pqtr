// sony.cpp
// Core Sony .ARW (TIFF-based) RAW file utilities
// TIFF parsing and ARW2 decompression functions
// Used by prepare.cpp

#include "sony.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>

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
        std::string get_entry_string(const IFDEntry &entry, const std::vector<uint8_t> &file_data)
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
        bool decompress_arw2(
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

                            bit += 7;
                        }
                    }

                    // Write decoded pixels with stride-2 interleaving
                    // Even columns (0,2,4...) then odd columns (1,3,5...)
                    for (int i = 0; i < 16; i++, col += 2)
                    {
                        if (col < width)
                        {
                            row_out[col] = pix[i];
                        }
                    }
                    col -= (col & 1) ? 1 : 31;

                    dp += 16;
                }
            }

            return true;
        }

    } // namespace internal

} // namespace sony
