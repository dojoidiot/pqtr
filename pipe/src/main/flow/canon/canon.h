// canon.h
// Clean-room Canon CR2 decoder - no libraw, no rawspeed.
//
// CR2 format:
//   - TIFF container with 4 IFDs
//   - IFD[3] contains RAW data as lossless JPEG
//   - Canon MakerNotes for WB, black/white levels
//
// See: http://lclevy.free.fr/cr2/ for format documentation

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace canon
{

// Simple image buffer (matches sony::Buffer)
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

using BayerU16 = Buffer<uint16_t>;

// RAW metadata extracted from CR2
struct RawMetadata
{
    // Image dimensions
    int width = 0;
    int height = 0;

    // Active area crop
    int crop_left = 0;
    int crop_top = 0;
    int crop_width = 0;
    int crop_height = 0;

    // RAW processing metadata
    int black_level = 0;
    int white_level = 16383;        // 14-bit default
    uint16_t wb_rggb[4] = {1024, 1024, 1024, 1024};
    int bayer_pattern = 0;          // 0=RGGB (Canon default)
    float cam_xyz[9] = {0};         // Camera RGB -> XYZ
    int orientation = 1;

    // Camera identification
    std::string camera_make;
    std::string camera_model;

    // EXIF
    float iso = 0;
    float shutter_speed = 0;
    float aperture = 0;
    float focal_length = 0;
    std::string lens_model;

    // LJpeg parameters
    int ljpeg_precision = 14;       // Bits per sample
    int slice_count = 0;            // Number of slices
    int slice_width = 0;            // Width of each slice
    int last_slice_width = 0;       // Width of last slice
};

// CR2 decoder
class Decoder
{
public:
    // Decode Canon CR2 file from memory buffer
    // Returns: true on success
    static bool prepare(const uint8_t* data, size_t size,
                       BayerU16& bayer, RawMetadata& metadata);

private:
    Decoder() = delete;
};

} // namespace canon
