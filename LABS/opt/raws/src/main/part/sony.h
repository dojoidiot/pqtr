// sony_arw2_gold.h
// Gold standard Sony ARW2 decoder
// Static interface: Sink → UMat + Info

#pragma once

#include <sink.hpp>
#include <opencv2/core.hpp>
#include <map>
#include <string>
#include <vector>

namespace sony
{

    // Info type (matches pipe::Info)
    using Info = std::map<std::string, std::string>;

    // Internal TIFF/ARW2 structures and functions
    namespace internal
    {
        // TIFF data types
        enum TIFFType
        {
            TYPE_BYTE = 1,
            TYPE_ASCII = 2,
            TYPE_SHORT = 3,
            TYPE_LONG = 4,
            TYPE_RATIONAL = 5
        };

        // TIFF IFD entry structure
        struct IFDEntry
        {
            uint16_t tag;
            uint16_t type;
            uint32_t count;
            uint32_t value_offset;
        };

        // Helper functions
        uint16_t read_u16(const uint8_t *data);
        uint32_t read_u32(const uint8_t *data);
        float read_rational(const std::vector<uint8_t> &file_data, uint32_t offset);
        IFDEntry parse_ifd_entry(const uint8_t *data);
        uint32_t get_entry_value(const IFDEntry &entry, const std::vector<uint8_t> &file_data);
        std::string get_entry_string(const IFDEntry &entry, const std::vector<uint8_t> &file_data);
        bool decompress_arw2(const uint8_t *compressed_data, size_t compressed_size, uint16_t *output, int width, int height);
    }

    struct RawMetadata
    {
        // Image dimensions
        int width;
        int height;

        // RAW processing metadata
        int black_level;
        int white_level;
        uint16_t wb_rggb[4];      // Raw WB multipliers from camera [R, G, G, B]
        int bayer_pattern;        // OpenCV Bayer pattern code
        cv::Matx33f color_matrix; // Camera RGB → sRGB matrix
        int orientation;          // EXIF orientation (1-8)

        // Camera identification
        std::string camera_make;
        std::string camera_model;

        // EXIF shooting parameters
        float iso;              // ISO speed (e.g., 100, 400, 3200)
        float shutter_speed;    // Shutter speed in seconds (e.g., 1/250 = 0.004)
        float aperture;         // F-number (e.g., 2.8, 5.6, 11.0)
        float focal_length;     // Focal length in mm (e.g., 50.0, 85.0)
        std::string lens_model; // Lens description
    };

    // Gold standard decoder for Sony ARW2 files
    // Single static method: prepare() decodes RAW data from Sink
    class arw2_gold
    {
    public:
        // Decode Sony ARW2 file from Sink
        // Input:  sink - Raw file bytes (from Tool::read)
        // Output: data     - Bayer data (uint16, single channel)
        //         info     - Metadata as string map (sony::Info)
        //         metadata - Metadata as struct (sony::RawMetadata)
        // Returns: true on success, false on error
        static bool prepare(pqtr::Sink &sink, cv::UMat &data, sony::Info &info, sony::RawMetadata &metadata);

        // Process RAW data through pipeline modules
        // Input:  bayer    - Bayer data from prepare() (uint16, single channel)
        //         metadata - Metadata from prepare()
        // Output: rgb      - Processed RGB image (float32, 3 channels, [0,1] range)
        // Pipeline: Bayer → BLC → WB → Demosaic → Color Matrix → Gamma (sRGB OETF)
        // Returns: true on success, false on error
        static bool process(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb);

    private:
        // No instances - static interface only
        arw2_gold() = delete;
    };

} // namespace sony
