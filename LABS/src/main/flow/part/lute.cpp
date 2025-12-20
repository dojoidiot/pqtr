// lute.cpp - Camera profile learning integration
//
// Downsamples HEAD output to match embedded JPEG, then learns LUT.
// LUT is resolution-independent: learn small, apply at any size.

#include "flow.hpp"
#include "lute.hpp"
#include <map>
#include <iostream>
#include <cmath>

namespace flow
{

    // Camera profiles (keyed by camera_make + camera_model + style)
    static std::map<std::string, lute::CameraLut> g_profiles;

    // =========================================================================
    // Bilinear downsample
    // =========================================================================

    static std::vector<float> downsample(const float *src, int src_w, int src_h,
                                          int dst_w, int dst_h)
    {
        std::vector<float> dst(dst_w * dst_h * 3);

        float scale_x = static_cast<float>(src_w) / dst_w;
        float scale_y = static_cast<float>(src_h) / dst_h;

        for (int y = 0; y < dst_h; y++)
        {
            for (int x = 0; x < dst_w; x++)
            {
                // Map to source coordinates
                float sx = (x + 0.5f) * scale_x - 0.5f;
                float sy = (y + 0.5f) * scale_y - 0.5f;

                int x0 = static_cast<int>(std::floor(sx));
                int y0 = static_cast<int>(std::floor(sy));
                int x1 = x0 + 1;
                int y1 = y0 + 1;

                // Clamp
                x0 = std::max(0, std::min(src_w - 1, x0));
                x1 = std::max(0, std::min(src_w - 1, x1));
                y0 = std::max(0, std::min(src_h - 1, y0));
                y1 = std::max(0, std::min(src_h - 1, y1));

                float fx = sx - std::floor(sx);
                float fy = sy - std::floor(sy);

                size_t idx00 = (y0 * src_w + x0) * 3;
                size_t idx10 = (y0 * src_w + x1) * 3;
                size_t idx01 = (y1 * src_w + x0) * 3;
                size_t idx11 = (y1 * src_w + x1) * 3;

                size_t dst_idx = (y * dst_w + x) * 3;

                for (int c = 0; c < 3; c++)
                {
                    float v00 = src[idx00 + c];
                    float v10 = src[idx10 + c];
                    float v01 = src[idx01 + c];
                    float v11 = src[idx11 + c];

                    float v0 = v00 * (1 - fx) + v10 * fx;
                    float v1 = v01 * (1 - fx) + v11 * fx;

                    dst[dst_idx + c] = v0 * (1 - fy) + v1 * fy;
                }
            }
        }

        return dst;
    }

    // =========================================================================
    // Get JPEG dimensions from decoded data
    // =========================================================================

    struct JpegInfo
    {
        int width;
        int height;
        std::vector<uint8_t> rgb;
    };

    static JpegInfo decodeJpeg(const uint8_t *data, size_t size)
    {
        JpegInfo info{0, 0, {}};

        // Use swap to decode, but we need dimensions too
        // swap returns just the RGB data, dimensions come from JPEG header
        // We'll parse the JPEG header to get dimensions

        // Simple JPEG header parser for dimensions
        size_t pos = 0;
        while (pos + 4 < size)
        {
            if (data[pos] != 0xFF)
            {
                pos++;
                continue;
            }

            uint8_t marker = data[pos + 1];

            // SOF markers (Start of Frame)
            if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
            {
                if (pos + 9 < size)
                {
                    info.height = (data[pos + 5] << 8) | data[pos + 6];
                    info.width = (data[pos + 7] << 8) | data[pos + 8];
                    break;
                }
            }

            // Skip to next marker
            if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7))
            {
                pos += 2;
            }
            else if (pos + 4 < size)
            {
                int len = (data[pos + 2] << 8) | data[pos + 3];
                pos += 2 + len;
            }
            else
            {
                break;
            }
        }

        if (info.width > 0 && info.height > 0)
        {
            info.rgb = swap(const_cast<uint8_t *>(data), size, 0, 0, BIN);
        }

        return info;
    }

    // =========================================================================
    // luneLearn - called after HEAD readback to learn camera profile
    // =========================================================================

    void luteLearn(const Done &head, const uint8_t *jpeg, size_t jpegSize,
                   const std::string &cameraKey)
    {
        if (jpeg == nullptr || jpegSize == 0 || cameraKey.empty())
            return;

        // Decode JPEG and get dimensions
        JpegInfo target = decodeJpeg(jpeg, jpegSize);
        if (target.rgb.empty() || target.width <= 0 || target.height <= 0)
        {
            std::cerr << "[lute] Failed to decode JPEG\n";
            return;
        }

        std::cerr << "[lute] JPEG: " << target.width << "x" << target.height
                  << ", HEAD: " << head.width << "x" << head.height << "\n";

        // Downsample HEAD to match JPEG size
        auto flat = downsample(head.rgb.data(), head.width, head.height,
                               target.width, target.height);

        std::cerr << "[lute] Downsampled HEAD to " << target.width << "x" << target.height << "\n";

        // Get or create profile
        lute::CameraLut &lut = g_profiles[cameraKey];

        // Parse camera key: Make_Model_Style
        size_t p1 = cameraKey.find('_');
        size_t p2 = (p1 != std::string::npos) ? cameraKey.find('_', p1 + 1) : std::string::npos;

        if (p1 != std::string::npos)
        {
            lut.camera_make = cameraKey.substr(0, p1);
            if (p2 != std::string::npos)
            {
                lut.camera_model = cameraKey.substr(p1 + 1, p2 - p1 - 1);
                lut.creative_style = cameraKey.substr(p2 + 1);
            }
            else
            {
                lut.camera_model = cameraKey.substr(p1 + 1);
            }
        }

        // Learn: flat (scene-linear float) → target (sRGB uint8)
        lute::tune(flat.data(), target.rgb.data(), target.width, target.height, lut);
    }

    // =========================================================================
    // luteApply - apply learned profile to image
    // =========================================================================

    void luteApply(Done &image, const std::string &cameraKey)
    {
        auto it = g_profiles.find(cameraKey);
        if (it == g_profiles.end() || !it->second.estimated)
            return;

        std::vector<float> output(image.rgb.size());
        lute::view(image.rgb.data(), output.data(), image.width, image.height, it->second);
        image.rgb = std::move(output);
    }

    // =========================================================================
    // luteDiff - compute spectral diff between HEAD and JPEG
    // =========================================================================

    // sRGB to linear
    static float srgb_to_linear(uint8_t v)
    {
        float f = v / 255.0f;
        if (f <= 0.04045f)
            return f / 12.92f;
        return std::pow((f + 0.055f) / 1.055f, 2.4f);
    }

    Done luteDiff(const Done &head, const uint8_t *jpeg, size_t jpegSize,
                  const std::string &cameraKey)
    {
        Done out;
        if (jpeg == nullptr || jpegSize == 0)
            return out;

        // Decode JPEG and get dimensions
        JpegInfo target = decodeJpeg(jpeg, jpegSize);
        if (target.rgb.empty() || target.width <= 0 || target.height <= 0)
        {
            std::cerr << "[diff] Failed to decode JPEG\n";
            return out;
        }

        std::cerr << "[diff] JPEG: " << target.width << "x" << target.height
                  << ", HEAD: " << head.width << "x" << head.height << "\n";

        // Downsample HEAD to match JPEG size
        auto flat = downsample(head.rgb.data(), head.width, head.height,
                               target.width, target.height);

        // Apply LUT if available
        if (!cameraKey.empty())
        {
            auto it = g_profiles.find(cameraKey);
            if (it != g_profiles.end() && it->second.estimated)
            {
                std::cerr << "[diff] Applying LUT: " << cameraKey << "\n";
                std::vector<float> lut_output(flat.size());
                lute::view(flat.data(), lut_output.data(), target.width, target.height, it->second);
                flat = std::move(lut_output);
            }
        }

        // Output at JPEG resolution
        out.width = target.width;
        out.height = target.height;
        out.rgb.resize(target.width * target.height * 3);

        // Compute spectral diff statistics
        // HEAD is scene-linear, JPEG is sRGB - convert JPEG to linear for comparison
        size_t pixels = static_cast<size_t>(target.width) * target.height;

        float max_diff = 0.0f;
        double sum_diff = 0.0;
        int brighter = 0, darker = 0;

        for (size_t i = 0; i < pixels; i++)
        {
            float hr = flat[i * 3 + 0], hg = flat[i * 3 + 1], hb = flat[i * 3 + 2];
            float jr = srgb_to_linear(target.rgb[i * 3 + 0]);
            float jg = srgb_to_linear(target.rgb[i * 3 + 1]);
            float jb = srgb_to_linear(target.rgb[i * 3 + 2]);

            float head_lum = 0.299f * hr + 0.587f * hg + 0.114f * hb;
            float jpeg_lum = 0.299f * jr + 0.587f * jg + 0.114f * jb;
            float lum_diff = head_lum - jpeg_lum;

            if (lum_diff > 0) brighter++;
            else darker++;

            float d = std::abs(lum_diff);
            if (d > max_diff) max_diff = d;
            sum_diff += d;
        }

        float mean_diff = static_cast<float>(sum_diff / pixels);
        float pct_brighter = 100.0f * brighter / pixels;

        std::cerr << "[diff] Max: " << max_diff << ", Mean: " << mean_diff
                  << ", HEAD brighter: " << pct_brighter << "%\n";

        // False color visualization:
        // - Gray = no difference
        // - Red = HEAD brighter than JPEG
        // - Blue = JPEG brighter than HEAD
        // Scale factor: 2x is subtle, 5x is aggressive
        float scale = 2.0f;

        for (size_t i = 0; i < pixels; i++)
        {
            float hr = flat[i * 3 + 0];
            float hg = flat[i * 3 + 1];
            float hb = flat[i * 3 + 2];

            float jr = srgb_to_linear(target.rgb[i * 3 + 0]);
            float jg = srgb_to_linear(target.rgb[i * 3 + 1]);
            float jb = srgb_to_linear(target.rgb[i * 3 + 2]);

            // Per-channel differences
            float dr = (hr - jr) * scale;
            float dg = (hg - jg) * scale;
            float db = (hb - jb) * scale;

            // Luminance of the diff (signed)
            float lum_diff = 0.299f * dr + 0.587f * dg + 0.114f * db;

            // False color: positive (HEAD > JPEG) = warm, negative = cool
            float r, g, b;
            if (lum_diff > 0)
            {
                // HEAD brighter - warm (red/yellow)
                r = 0.5f + lum_diff;
                g = 0.5f + lum_diff * 0.5f;
                b = 0.5f - lum_diff * 0.3f;
            }
            else
            {
                // JPEG brighter - cool (blue/cyan)
                r = 0.5f + lum_diff * 0.3f;
                g = 0.5f - lum_diff * 0.5f;
                b = 0.5f - lum_diff;
            }

            out.rgb[i * 3 + 0] = std::max(0.0f, std::min(1.0f, r));
            out.rgb[i * 3 + 1] = std::max(0.0f, std::min(1.0f, g));
            out.rgb[i * 3 + 2] = std::max(0.0f, std::min(1.0f, b));
        }

        return out;
    }

    // =========================================================================
    // luteSave / luteLoad - persistence
    // =========================================================================

    bool luteSave(const std::string &dir)
    {
        bool ok = true;
        for (const auto &[key, lut] : g_profiles)
        {
            std::string path = dir + "/" + key + ".json";
            if (!lute::save(lut, path))
                ok = false;
        }
        return ok;
    }

    bool luteLoad(const std::string &dir, const std::string &key)
    {
        std::string path = dir + "/" + key + ".json";
        lute::CameraLut lut;
        if (lute::load(lut, path))
        {
            g_profiles[key] = std::move(lut);
            return true;
        }
        return false;
    }

    // =========================================================================
    // luteProfile - get profile for inspection
    // =========================================================================

    const lute::CameraLut *luteProfile(const std::string &key)
    {
        auto it = g_profiles.find(key);
        return (it != g_profiles.end()) ? &it->second : nullptr;
    }

} // namespace flow
