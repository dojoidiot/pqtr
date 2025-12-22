// swap.cpp - Universal format conversion
//
// Single function: swap(data, size, w, h, from, into)

#include "flow.hpp"
#include <cmath>
#include <algorithm>

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

// Linear to sRGB gamma
static uint8_t linear_to_srgb(float v)
{
    v = std::max(0.0f, std::min(1.0f, v));
    if (v <= 0.0031308f)
        v = v * 12.92f;
    else
        v = 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

// sRGB to Linear
static float srgb_to_linear(uint8_t v)
{
    float f = v / 255.0f;
    if (f <= 0.04045f)
        return f / 12.92f;
    return std::pow((f + 0.055f) / 1.055f, 2.4f);
}

// -------------------------------------------------------------------------
// swap - universal format conversion
// -------------------------------------------------------------------------

std::vector<uint8_t> swap(const void *data, int w, int h, Swap from, Swap into)
{
    if (!data)
        return {};

    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    const float *floats = static_cast<const float *>(data);

    // Decode compressed -> BIN (w = byte size)
    if (into == BIN)
    {
        if (from == PNG)
        {
            auto result = decodePng(bytes, static_cast<size_t>(w));
            return std::move(result.rgb);
        }
        if (from == JPG)
        {
            auto result = decodeJpeg(bytes, static_cast<size_t>(w));
            return std::move(result.rgb);
        }
        return {};
    }

    // Decode compressed -> LIN (w = byte size)
    if (into == LIN)
    {
        ImageResult result;
        if (from == PNG)
            result = decodePng(bytes, static_cast<size_t>(w));
        else if (from == JPG)
            result = decodeJpeg(bytes, static_cast<size_t>(w));
        else
            return {};

        if (result.rgb.empty())
            return {};

        // Convert sRGB uint8 to linear float
        size_t pixels = result.rgb.size() / 3;
        std::vector<uint8_t> linBytes(pixels * 3 * sizeof(float));
        float *linFloats = reinterpret_cast<float *>(linBytes.data());

        for (size_t i = 0; i < pixels * 3; ++i)
            linFloats[i] = srgb_to_linear(result.rgb[i]);

        return linBytes;
    }

    // Encode BIN -> compressed (w, h = dimensions)
    if (from == BIN)
    {
        if (w <= 0 || h <= 0)
            return {};
        if (into == PNG)
            return encodePng(bytes, w, h);
        if (into == JPG)
            return encodeJpeg(bytes, w, h);
        return {};
    }

    // Encode LIN -> compressed (w, h = dimensions)
    if (from == LIN)
    {
        if (w <= 0 || h <= 0)
            return {};

        size_t pixels = static_cast<size_t>(w) * h;
        std::vector<uint8_t> rgb8(pixels * 3);

        for (size_t i = 0; i < pixels * 3; ++i)
            rgb8[i] = linear_to_srgb(floats[i]);

        if (into == PNG)
            return encodePng(rgb8.data(), w, h);
        if (into == JPG)
            return encodeJpeg(rgb8.data(), w, h);
        return {};
    }

    return {};
}

} // namespace flow
