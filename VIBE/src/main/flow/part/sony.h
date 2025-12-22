// sony.h
// Main header for the Sony RAW decoder pipeline.
// Clean-room implementation - no libraw, no OpenCV.
// Uses Dawn/WebGPU for GPU processing.

#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>

// Forward declare Dawn types (implementation includes full headers)
namespace wgpu { class Device; class Buffer; }

namespace sony
{

    // Info type (matches flow::Tree leaf data)
    using Info = std::map<std::string, std::string>;

    // Simple image buffer (CPU-side)
    template<typename T>
    struct Buffer {
        std::vector<T> data;
        int width = 0;
        int height = 0;
        int channels = 1;

        size_t size() const { return data.size(); }
        size_t bytes() const { return data.size() * sizeof(T); }
        T* ptr() { return data.data(); }
        const T* ptr() const { return data.data(); }

        void resize(int w, int h, int c = 1) {
            width = w; height = h; channels = c;
            data.resize(w * h * c);
        }
    };

    using BayerU16 = Buffer<uint16_t>;  // Raw Bayer (16-bit)
    using BayerF32 = Buffer<float>;     // Normalized Bayer (float)
    using ImageF32 = Buffer<float>;     // RGB float (3 channels)
    using ImageU8  = Buffer<uint8_t>;   // RGB 8-bit (3 channels)

    // Internal TIFF/ARW2 structures and functions
    namespace internal
    {
        enum TIFFType
        {
            TYPE_BYTE = 1,
            TYPE_ASCII = 2,
            TYPE_SHORT = 3,
            TYPE_LONG = 4,
            TYPE_RATIONAL = 5
        };

        struct IFDEntry
        {
            uint16_t tag;
            uint16_t type;
            uint32_t count;
            uint32_t value_offset;
        };

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
        int width = 0;
        int height = 0;

        // Active area crop (removes optical black borders)
        int crop_left = 0;
        int crop_top = 0;
        int crop_width = 0;
        int crop_height = 0;

        // RAW processing metadata
        int black_level = 512;
        int white_level = 15360;
        uint16_t wb_rggb[4] = {2176, 1024, 1551, 1024};
        int bayer_pattern = 0;        // 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG
        float color_matrix[9] = {     // Camera RGB -> sRGB (row-major 3x3)
            1.3125f, -0.2061f, -0.0742f,
           -0.0088f,  1.1953f, -0.1553f,
            0.0068f, -0.0400f,  1.0645f
        };
        int orientation = 1;          // EXIF orientation (1-8)

        // Camera identification
        std::string camera_make;
        std::string camera_model;

        // EXIF shooting parameters
        float iso = 0;
        float shutter_speed = 0;
        float aperture = 0;
        float focal_length = 0;
        std::string lens_model;

        // Embedded preview
        std::vector<uint8_t> preview_jpeg;  // Original JPEG bytes (for saving)
        ImageU8 preview;                     // Decoded RGB (for diff)

        // Camera rendering settings (what produced the preview)
        std::string creative_style = "Standard";
        std::string dro = "Off";
        int contrast = 0;
        int saturation = 0;
        int sharpness = 0;

        // Lens correction parameters (from Sony MakerNotes tag 0x7037)
        int16_t distortion_params[16] = {0};
        int distortion_knot_count = 0;
        bool has_distortion_params = false;
    };

    // Sony RAW decoder - GPU accelerated via Dawn/WebGPU
    class Decoder
    {
    public:
        // Decode Sony ARW file from memory buffer (CPU)
        // Input:  data     - Raw file bytes
        //         size     - Size of data in bytes
        // Output: bayer    - Bayer data (uint16)
        //         info     - Metadata as string map
        //         metadata - Metadata as struct (includes preview)
        // Returns: true on success
        static bool prepare(const uint8_t* data, size_t size,
                           BayerU16& bayer, Info& info, RawMetadata& metadata);

        // Process RAW to scene-linear RGB (GPU)
        // Pipeline: BLC -> WB -> Demosaic -> ColorMatrix -> Crop
        // Input:  bayer    - Bayer data from prepare()
        //         metadata - Metadata from prepare()
        // Output: rgb      - Scene-linear RGB (float, 3 channels)
        // Returns: true on success
        static bool process_linear(const BayerU16& bayer,
                                   const RawMetadata& metadata,
                                   ImageF32& rgb);

        // Process RAW to display-referred RGB (GPU)
        // Same as process_linear + gamma
        static bool process(const BayerU16& bayer,
                           const RawMetadata& metadata,
                           ImageF32& rgb);

    private:
        Decoder() = delete;
    };

} // namespace sony
