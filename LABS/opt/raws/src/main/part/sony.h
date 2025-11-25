// sony.h
// Main header for the Sony RAW decoder pipeline.

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
        // Image dimensions (sensor size from RAW)
        int width;
        int height;

        // Active area crop (removes optical black borders)
        int crop_left;
        int crop_top;
        int crop_width;   // Final image width after crop
        int crop_height;  // Final image height after crop

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

        // Embedded preview (camera-rendered JPEG)
        cv::UMat preview;       // Display-referred sRGB (CV_8UC3)
        int preview_width;
        int preview_height;

        // Camera rendering settings (what produced the preview)
        std::string creative_style;  // "Standard", "Vivid", "Portrait", etc.
        std::string dro;             // "Off", "Auto", "Lv1"-"Lv5"
        int contrast;                // -3 to +3
        int saturation;              // -3 to +3
        int sharpness;               // -3 to +3
    };

    // Sony RAW decoder with a static interface.
    class Decoder
    {
    public:
        // Decode Sony ARW file from Sink
        // Input:  sink     - Raw file bytes
        // Output: data     - Bayer data (CV_16UC1)
        //         info     - Metadata as string map
        //         metadata - Metadata as struct
        // Returns: true on success, false on error
        static bool prepare(pqtr::Sink &sink, cv::UMat &data, sony::Info &info, sony::RawMetadata &metadata);

        // Process RAW data through the full pipeline
        // Input:  bayer    - Bayer data from prepare() (CV_16UC1)
        //         metadata - Metadata from prepare()
        // Output: rgb      - Final processed RGB image (CV_32FC3, [0,1] range)
        // Returns: true on success, false on error
        static bool process(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb);

        // Process RAW data to linear RGB (stops before gamma)
        // For use with external display-referred processing
        // Input:  bayer    - Bayer data from prepare() (CV_16UC1)
        //         metadata - Metadata from prepare()
        // Output: rgb      - Linear RGB image (CV_32FC3, camera space, [0,1+] range)
        // Returns: true on success, false on error
        static bool process_linear(const cv::UMat &bayer, const sony::RawMetadata &metadata, cv::UMat &rgb);

        // Apply gamma OETF (public for external pipeline use)
        // Input:  linear   - Linear RGB (CV_32FC3)
        // Output: gamma    - Gamma-corrected RGB (CV_32FC3)
        static bool apply_gamma(const cv::UMat &linear, cv::UMat &gamma);

    private:
        // No instances - static interface only
        Decoder() = delete;

        // Internal pipeline stages (scene-referred order)
        // Stage 1-2: On Bayer (CV_16UC1 → CV_32FC1)
        static bool blc_bayer(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
        static bool wb_bayer(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
        // Stage 3: Demosaic (CV_32FC1 → CV_32FC3 RGB)
        static bool demosaic(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
        // Stage 4-5: On RGB (CV_32FC3)
        static bool color_matrix(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
        static bool crop(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
        // Display-referred (not used for scene-referred output)
        static bool gamma_oetf(const cv::UMat &input, cv::UMat &output);

        // Legacy (deprecated - kept for reference)
        static bool blc(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
        static bool wb_gain(const cv::UMat &input, cv::UMat &output, const RawMetadata &metadata);
    };

} // namespace sony
