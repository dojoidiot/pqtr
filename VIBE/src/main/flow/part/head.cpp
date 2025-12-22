// head.cpp - Task implementation for HEAD stage (RAW -> scene-linear RGB)
//
// Wraps sony::Decoder::process_linear() for GPU processing

#include "flow.hpp"
#include "sony.h"

#include <cmath>
#include <cstring>
#include <algorithm>

namespace flow
{

// Forward declare warp (lens distortion correction)
void warp(float *rgb, int w, int h, const float *params, int count);

// =========================================================================
// EXIF orientation handling
// =========================================================================
// Orientation values:
//   1 = Normal (0°)
//   2 = Flip horizontal
//   3 = Rotate 180°
//   4 = Flip vertical
//   5 = Rotate 90° CW + flip horizontal
//   6 = Rotate 90° CW
//   7 = Rotate 90° CCW + flip horizontal
//   8 = Rotate 90° CCW

static void apply_orientation(sony::ImageF32 &img, int orientation)
{
    if (orientation <= 1 || orientation > 8)
        return;  // Normal or invalid

    int w = img.width;
    int h = img.height;
    size_t pixels = static_cast<size_t>(w) * h;

    // Allocate temp buffer for transforms that need it
    std::vector<float> tmp(pixels * 3);
    float *src = img.ptr();

    if (orientation == 2)  // Flip horizontal
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w / 2; ++x)
            {
                int left = (y * w + x) * 3;
                int right = (y * w + (w - 1 - x)) * 3;
                std::swap(src[left + 0], src[right + 0]);
                std::swap(src[left + 1], src[right + 1]);
                std::swap(src[left + 2], src[right + 2]);
            }
        }
    }
    else if (orientation == 3)  // Rotate 180°
    {
        for (size_t i = 0; i < pixels / 2; ++i)
        {
            size_t j = pixels - 1 - i;
            std::swap(src[i * 3 + 0], src[j * 3 + 0]);
            std::swap(src[i * 3 + 1], src[j * 3 + 1]);
            std::swap(src[i * 3 + 2], src[j * 3 + 2]);
        }
    }
    else if (orientation == 4)  // Flip vertical
    {
        for (int y = 0; y < h / 2; ++y)
        {
            int top = y * w * 3;
            int bot = (h - 1 - y) * w * 3;
            for (int x = 0; x < w * 3; ++x)
                std::swap(src[top + x], src[bot + x]);
        }
    }
    else if (orientation == 6)  // Rotate 90° CW
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int src_idx = (y * w + x) * 3;
                int dst_idx = (x * h + (h - 1 - y)) * 3;
                tmp[dst_idx + 0] = src[src_idx + 0];
                tmp[dst_idx + 1] = src[src_idx + 1];
                tmp[dst_idx + 2] = src[src_idx + 2];
            }
        }
        img.resize(h, w, 3);  // Swap dimensions
        std::memcpy(img.ptr(), tmp.data(), pixels * 3 * sizeof(float));
    }
    else if (orientation == 8)  // Rotate 90° CCW
    {
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                int src_idx = (y * w + x) * 3;
                int dst_idx = ((w - 1 - x) * h + y) * 3;
                tmp[dst_idx + 0] = src[src_idx + 0];
                tmp[dst_idx + 1] = src[src_idx + 1];
                tmp[dst_idx + 2] = src[src_idx + 2];
            }
        }
        img.resize(h, w, 3);  // Swap dimensions
        std::memcpy(img.ptr(), tmp.data(), pixels * 3 * sizeof(float));
    }
    // TODO: orientations 5 and 7 (rotate + flip combos)
}

// =========================================================================
// TaskImpl - internal state for GPU processing job
// =========================================================================

struct TaskImpl
{
    // Input (from prepare)
    sony::BayerU16 bayer;
    sony::RawMetadata metadata;

    // Reference for diff (optional)
    std::vector<uint8_t> reference_rgb8;
    int reference_width = 0;
    int reference_height = 0;

    // Output (after post)
    sony::ImageF32 rgb;
    bool posted = false;

    TaskImpl(const uint16_t *data, int w, int h, const sony::RawMetadata &meta)
        : metadata(meta)
    {
        bayer.resize(w, h, 1);
        std::memcpy(bayer.ptr(), data, w * h * sizeof(uint16_t));
    }

    void setReference(const uint8_t *data, size_t size, int w, int h)
    {
        reference_rgb8.assign(data, data + size);
        reference_width = w;
        reference_height = h;
    }
};

// =========================================================================
// Task - PIMPL wrapper
// =========================================================================

Task::Task(TaskImpl *impl) : impl_(impl) {}

Task::~Task()
{
    delete impl_;
}

Task::Task(Task &&other) noexcept : impl_(other.impl_)
{
    other.impl_ = nullptr;
}

Task &Task::operator=(Task &&other) noexcept
{
    if (this != &other)
    {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

void Task::post()
{
    if (!impl_ || impl_->posted)
        return;

    // Run GPU pipeline
    sony::Decoder::process_linear(impl_->bayer, impl_->metadata, impl_->rgb);

    // Apply lens distortion correction if parameters available
    if (impl_->metadata.has_distortion_params && impl_->metadata.distortion_knot_count > 0)
    {
        // Convert int16 params to float
        float params[16];
        for (int i = 0; i < impl_->metadata.distortion_knot_count; ++i)
            params[i] = static_cast<float>(impl_->metadata.distortion_params[i]);

        warp(impl_->rgb.ptr(), impl_->rgb.width, impl_->rgb.height,
             params, impl_->metadata.distortion_knot_count);
    }

    // Apply EXIF orientation (rotation/flip)
    apply_orientation(impl_->rgb, impl_->metadata.orientation);

    impl_->posted = true;
}

Done Task::done()
{
    Done result;
    if (!impl_ || !impl_->posted)
        return result;

    result.width = impl_->rgb.width;
    result.height = impl_->rgb.height;
    result.rgb.resize(impl_->rgb.size());
    std::memcpy(result.rgb.data(), impl_->rgb.ptr(), impl_->rgb.bytes());

    return result;
}

// sRGB to linear conversion
static float srgb_to_linear(uint8_t v)
{
    float f = v / 255.0f;
    if (f <= 0.04045f)
        return f / 12.92f;
    return std::pow((f + 0.055f) / 1.055f, 2.4f);
}

// Downsample image to target size
static std::vector<float> downsample(const float *src, int sw, int sh,
                                      int dw, int dh)
{
    std::vector<float> dst(dw * dh * 3);
    float x_scale = static_cast<float>(sw) / dw;
    float y_scale = static_cast<float>(sh) / dh;

    for (int y = 0; y < dh; ++y)
    {
        for (int x = 0; x < dw; ++x)
        {
            int sx = static_cast<int>(x * x_scale);
            int sy = static_cast<int>(y * y_scale);
            sx = std::min(sx, sw - 1);
            sy = std::min(sy, sh - 1);

            int src_idx = (sy * sw + sx) * 3;
            int dst_idx = (y * dw + x) * 3;
            dst[dst_idx + 0] = src[src_idx + 0];
            dst[dst_idx + 1] = src[src_idx + 1];
            dst[dst_idx + 2] = src[src_idx + 2];
        }
    }
    return dst;
}

Done Task::diff()
{
    Done result;
    if (!impl_ || !impl_->posted)
        return result;

    if (impl_->reference_rgb8.empty() || impl_->reference_width <= 0)
        return result;

    int rw = impl_->reference_width;
    int rh = impl_->reference_height;
    int orientation = impl_->metadata.orientation;

    // Convert reference to linear
    size_t pixels = static_cast<size_t>(rw) * rh;
    std::vector<float> ref_linear(pixels * 3);
    for (size_t i = 0; i < pixels * 3; ++i)
        ref_linear[i] = srgb_to_linear(impl_->reference_rgb8[i]);

    // Apply orientation to reference (to match HEAD output)
    int outW = rw, outH = rh;
    std::vector<float> ref_rotated;
    if (orientation == 6 || orientation == 8)
    {
        outW = rh; outH = rw;
        ref_rotated.resize(outW * outH * 3);
        for (int y = 0; y < rh; ++y)
        {
            for (int x = 0; x < rw; ++x)
            {
                int srcIdx = (y * rw + x) * 3;
                int dstIdx;
                if (orientation == 6)  // 90° CW
                    dstIdx = (x * outW + (outW - 1 - y)) * 3;
                else  // 90° CCW
                    dstIdx = ((outH - 1 - x) * outW + y) * 3;
                ref_rotated[dstIdx + 0] = ref_linear[srcIdx + 0];
                ref_rotated[dstIdx + 1] = ref_linear[srcIdx + 1];
                ref_rotated[dstIdx + 2] = ref_linear[srcIdx + 2];
            }
        }
    }
    else
    {
        ref_rotated = std::move(ref_linear);
    }

    // Downsample stage to reference size
    auto stage = downsample(impl_->rgb.ptr(), impl_->rgb.width, impl_->rgb.height, outW, outH);

    // Compute false-color diff
    result.width = outW;
    result.height = outH;
    size_t outPixels = static_cast<size_t>(outW) * outH;
    result.rgb.resize(outPixels * 3);

    for (size_t i = 0; i < outPixels; ++i)
    {
        float sr = stage[i * 3 + 0], sg = stage[i * 3 + 1], sb = stage[i * 3 + 2];
        float rr = ref_rotated[i * 3 + 0], rg = ref_rotated[i * 3 + 1], rb = ref_rotated[i * 3 + 2];

        // Luminance diff
        float s_lum = 0.299f * sr + 0.587f * sg + 0.114f * sb;
        float r_lum = 0.299f * rr + 0.587f * rg + 0.114f * rb;
        float lum_diff = s_lum - r_lum;

        // False color: red = stage brighter, blue = ref brighter
        float scale = 3.0f;
        float r = 0.5f + lum_diff * scale;
        float g = 0.5f - std::abs(lum_diff) * scale * 0.5f;
        float b = 0.5f - lum_diff * scale;

        result.rgb[i * 3 + 0] = std::max(0.0f, std::min(1.0f, r));
        result.rgb[i * 3 + 1] = std::max(0.0f, std::min(1.0f, g));
        result.rgb[i * 3 + 2] = std::max(0.0f, std::min(1.0f, b));
    }

    return result;
}

void *Task::buff() const
{
    // TODO: return GPU buffer pointer when we expose it
    return nullptr;
}

int Task::width() const
{
    return impl_ ? impl_->metadata.width : 0;
}

int Task::height() const
{
    return impl_ ? impl_->metadata.height : 0;
}

// =========================================================================
// Factory functions (called from FlowImpl)
// =========================================================================

// Create HEAD task from Bayer data + metadata
Task makeHeadTask(const uint16_t *data, int w, int h,
                  const sony::RawMetadata &metadata,
                  const uint8_t *view, size_t viewSize,
                  int viewWidth, int viewHeight)
{
    auto *impl = new TaskImpl(data, w, h, metadata);

    // Set reference if available
    if (view && viewSize > 0 && viewWidth > 0 && viewHeight > 0)
        impl->setReference(view, viewSize, viewWidth, viewHeight);

    return Task(impl);
}

} // namespace flow
