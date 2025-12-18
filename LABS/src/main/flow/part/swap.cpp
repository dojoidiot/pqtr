// swap.cpp - Format conversion between BIN (raw RGB), PNG, JPG
//
// BIN->PNG/JPG: data is w*h*3 RGB, size ignored
// PNG/JPG->BIN: data is compressed, size is data length, w/h ignored

#include "flow.hpp"

namespace flow
{
    // Forward declarations from png.cpp and jpg.cpp
    std::vector<uint8_t> encodePng(const uint8_t *rgb, int width, int height);
    std::vector<uint8_t> encodeJpeg(const uint8_t *rgb, int width, int height, int quality = 85);

    struct ImageResult
    {
        int width;
        int height;
        std::vector<uint8_t> rgb;
    };

    ImageResult decodePng(const uint8_t *data, size_t size);
    ImageResult decodeJpeg(const uint8_t *data, size_t size);

    // -------------------------------------------------------------------------
    // swap
    // -------------------------------------------------------------------------

    std::vector<uint8_t> swap(uint8_t *data, size_t size, int w, int h, Swap to)
    {
        if (!data)
            return {};

        switch (to)
        {
        case PNG:
            if (w <= 0 || h <= 0)
                return {};
            return encodePng(data, w, h);

        case JPG:
            if (w <= 0 || h <= 0)
                return {};
            return encodeJpeg(data, w, h);

        case BIN:
        {
            if (size == 0)
                return {};
            // Try PNG first, then JPG
            auto png = decodePng(data, size);
            if (!png.rgb.empty())
                return std::move(png.rgb);

            auto jpg = decodeJpeg(data, size);
            if (!jpg.rgb.empty())
                return std::move(jpg.rgb);

            return {};
        }
        }

        return {};
    }

} // namespace flow
