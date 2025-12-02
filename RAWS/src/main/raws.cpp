// raws.cpp
// RAWS library implementation - decodes RAW files to scene-linear RGB
// Auto-detects format and dispatches to appropriate decoder (Sony, Canon, Nikon, etc.)

#include "raws.hpp"
#include "sony.h"
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <iostream>

namespace raws {

// ============================================================
// Base curve estimation (from scene-linear data to JPEG preview)
// ============================================================

static void estimateBaseCurve(const cv::UMat& data, const cv::UMat& preview, float* curve)
{
    // Initialize all 3 channels to identity
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = i / 255.0f;

    if (data.empty() || preview.empty())
        return;

    // Resize preview to match data aspect ratio for comparison
    cv::Mat data_cpu, preview_cpu;
    data.copyTo(data_cpu);
    preview.copyTo(preview_cpu);

    // Resize data down to preview size for comparison
    cv::Mat data_small;
    cv::resize(data_cpu, data_small, preview_cpu.size(), 0, 0, cv::INTER_AREA);

    // Convert data to 8-bit sRGB-encoded for comparison
    cv::Mat data_clamped;
    cv::max(data_small, 0.0f, data_clamped);
    cv::min(data_clamped, 1.0f, data_clamped);

    // Apply sRGB transfer function (matches toDisplayView in LABS)
    cv::Mat data_srgb = data_clamped.clone();
    for (int y = 0; y < data_srgb.rows; y++)
    {
        float* ptr = data_srgb.ptr<float>(y);
        for (int x = 0; x < data_srgb.cols * 3; x++)
        {
            float v = ptr[x];
            ptr[x] = (v <= 0.0031308f)
                ? v * 12.92f
                : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
        }
    }

    cv::Mat data_8u;
    data_srgb.convertTo(data_8u, CV_8UC3, 255.0);

    // Per-channel curve estimation (BGR order in OpenCV)
    // curve layout: [B0..B255, G0..G255, R0..R255] to match OpenCV BGR
    //
    // KEY INSIGHT: Only use near-neutral pixels for curve estimation.
    // This isolates the tone curve from color grading. Saturated pixels
    // have hue-dependent transforms that pollute per-channel curves.
    std::vector<double> sum[3];
    std::vector<double> count[3];
    for (int c = 0; c < 3; c++) {
        sum[c].resize(256, 0.0);
        count[c].resize(256, 0.0);
    }

    // Use ALL pixels for curve estimation
    // The per-channel approach naturally averages out hue-dependent variations
    for (int y = 0; y < data_8u.rows; y++)
    {
        const uchar* d_ptr = data_8u.ptr<uchar>(y);
        const uchar* p_ptr = preview_cpu.ptr<uchar>(y);

        for (int x = 0; x < data_8u.cols; x++)
        {
            for (int c = 0; c < 3; c++)  // B, G, R
            {
                int bin = d_ptr[x * 3 + c];
                sum[c][bin] += p_ptr[x * 3 + c];
                count[c][bin] += 1.0;
            }
        }
    }

    // Compute curve values per channel
    for (int c = 0; c < 3; c++)
    {
        for (int i = 0; i < 256; i++)
        {
            if (count[c][i] > 0)
                curve[c * 256 + i] = static_cast<float>(sum[c][i] / count[c][i]) / 255.0f;
            else
                curve[c * 256 + i] = i / 255.0f;  // Identity fallback
        }

        // Ensure monotonicity per channel
        for (int i = 1; i < 256; i++)
        {
            if (curve[c * 256 + i] < curve[c * 256 + i - 1])
                curve[c * 256 + i] = curve[c * 256 + i - 1];
        }

        // Light smoothing per channel
        std::vector<float> smoothed(256);
        smoothed[0] = curve[c * 256 + 0];
        smoothed[255] = curve[c * 256 + 255];
        for (int i = 1; i < 255; i++)
            smoothed[i] = 0.25f * curve[c * 256 + i - 1] + 0.5f * curve[c * 256 + i] + 0.25f * curve[c * 256 + i + 1];
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = smoothed[i];

        // Force endpoints: black→black, white→white (preserve neutrals)
        curve[c * 256 + 0] = 0.0f;
        curve[c * 256 + 255] = 1.0f;
    }
}

// ============================================================
// Polynomial coefficient estimation (Camera Math)
// ============================================================

static void estimatePolyCoeffs(const cv::UMat& data, const cv::UMat& preview, float* coeffs)
{
    // Initialize to identity: R_out = R, G_out = G, B_out = B
    // coeffs layout: [R(10), G(10), B(10)]
    // Per channel: [c0, c1_R, c2_G, c3_B, c4_R², c5_G², c6_B², c7_RG, c8_RB, c9_GB]
    for (int i = 0; i < 30; i++)
        coeffs[i] = 0.0f;
    coeffs[1] = 1.0f;   // R_out from R
    coeffs[12] = 1.0f;  // G_out from G (index 10 + 2)
    coeffs[23] = 1.0f;  // B_out from B (index 20 + 3)

    if (data.empty() || preview.empty())
        return;

    // Resize data to preview size
    cv::Mat data_cpu, preview_cpu;
    data.copyTo(data_cpu);
    preview.copyTo(preview_cpu);

    cv::Mat data_small;
    if (data_cpu.size() != preview_cpu.size())
        cv::resize(data_cpu, data_small, preview_cpu.size(), 0, 0, cv::INTER_AREA);
    else
        data_small = data_cpu;

    // Convert data to gamma-encoded (polynomial works in gamma space)
    cv::Mat data_gamma;
    cv::max(data_small, 0.0f, data_gamma);
    cv::min(data_gamma, 1.0f, data_gamma);
    cv::pow(data_gamma, 1.0f / 2.2f, data_gamma);

    // Target to float [0-1]
    cv::Mat target_f;
    preview_cpu.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    // Sample pixels for least squares (random sampling for speed)
    const int num_samples = 50000;
    std::vector<std::vector<float>> samples_r, samples_g, samples_b;
    std::vector<float> targets_r, targets_g, targets_b;
    samples_r.reserve(num_samples);
    samples_g.reserve(num_samples);
    samples_b.reserve(num_samples);
    targets_r.reserve(num_samples);
    targets_g.reserve(num_samples);
    targets_b.reserve(num_samples);

    // Deterministic sampling pattern
    int step = std::max(1, (data_gamma.rows * data_gamma.cols) / num_samples);
    int idx = 0;
    for (int y = 0; y < data_gamma.rows && idx < num_samples; y++)
    {
        const float* src = data_gamma.ptr<float>(y);
        const float* tgt = target_f.ptr<float>(y);

        for (int x = 0; x < data_gamma.cols && idx < num_samples; x++)
        {
            if ((y * data_gamma.cols + x) % step == 0)
            {
                float b = src[x * 3 + 0];
                float g = src[x * 3 + 1];
                float r = src[x * 3 + 2];

                // Feature vector: 1, R, G, B, R², G², B², RG, RB, GB
                std::vector<float> features = {
                    1.0f, r, g, b,
                    r * r, g * g, b * b,
                    r * g, r * b, g * b
                };

                samples_r.push_back(features);
                samples_g.push_back(features);
                samples_b.push_back(features);

                targets_r.push_back(tgt[x * 3 + 2]);  // R
                targets_g.push_back(tgt[x * 3 + 1]);  // G
                targets_b.push_back(tgt[x * 3 + 0]);  // B

                idx++;
            }
        }
    }

    if (samples_r.empty())
        return;

    // Solve least squares for each channel using normal equations: (A^T A) x = A^T b
    auto solve_channel = [](const std::vector<std::vector<float>>& inputs,
                            const std::vector<float>& outputs,
                            float* out_coeffs)
    {
        const int n = inputs.size();
        const int k = 10;  // coefficients per channel

        // Build A^T A (k×k) and A^T b (k×1)
        cv::Mat AtA(k, k, CV_64FC1, cv::Scalar(0));
        cv::Mat Atb(k, 1, CV_64FC1, cv::Scalar(0));

        for (int i = 0; i < n; i++)
        {
            const auto& row = inputs[i];
            double y = outputs[i];

            for (int j = 0; j < k; j++)
            {
                Atb.at<double>(j, 0) += row[j] * y;
                for (int m = 0; m < k; m++)
                    AtA.at<double>(j, m) += row[j] * row[m];
            }
        }

        // Solve using SVD
        cv::Mat x;
        cv::solve(AtA, Atb, x, cv::DECOMP_SVD);

        for (int i = 0; i < k; i++)
            out_coeffs[i] = static_cast<float>(x.at<double>(i, 0));
    };

    solve_channel(samples_r, targets_r, coeffs);       // R coeffs [0-9]
    solve_channel(samples_g, targets_g, coeffs + 10);  // G coeffs [10-19]
    solve_channel(samples_b, targets_b, coeffs + 20);  // B coeffs [20-29]
}

// ============================================================
// Format detection
// ============================================================

enum class Format {
    Unknown,
    SonyARW,
    // Future: CanonCR2, CanonCR3, NikonNEF, etc.
};

static Format detectFormat(pqtr::Sink& sink)
{
    // Format detection is done inside each decoder's prepare() call
    // For now, we just check minimum size and try Sony
    // Sony's prepare() will validate the TIFF header itself

    if (sink.size() < 16) return Format::Unknown;

    // For now, assume all files are Sony ARW
    // TODO: Add format sniffing when we have multiple decoders
    return Format::SonyARW;
}

// ============================================================
// Sony decoder
// ============================================================

static Result decodeSony(pqtr::Sink& sink)
{
    Result result;

    cv::UMat bayer;
    sony::Info sonyInfo;
    sony::RawMetadata meta;

    if (!sony::Decoder::prepare(sink, bayer, sonyInfo, meta))
        return result;

    if (!sony::Decoder::process_linear(bayer, meta, result.data))
        return result;

    // Data info: scene-linear metadata
    for (const auto& kv : sonyInfo)
        result.dataInfo[kv.first] = kv.second;

    result.dataInfo["decoder"] = "raws_sony_arw2";
    result.dataInfo["width"] = std::to_string(meta.crop_width);
    result.dataInfo["height"] = std::to_string(meta.crop_height);
    result.dataInfo["camera_make"] = meta.camera_make;
    result.dataInfo["camera_model"] = meta.camera_model;
    result.dataInfo["lens_model"] = meta.lens_model;

    std::ostringstream oss;
    oss << meta.iso; result.dataInfo["iso"] = oss.str();
    oss.str(""); oss << meta.shutter_speed; result.dataInfo["shutter_speed"] = oss.str();
    oss.str(""); oss << meta.aperture; result.dataInfo["aperture"] = oss.str();
    oss.str(""); oss << meta.focal_length; result.dataInfo["focal_length"] = oss.str();
    result.dataInfo["orientation"] = std::to_string(meta.orientation);

    // Preview info: what produced the camera look
    result.preview = std::move(meta.preview);
    result.previewInfo["width"] = std::to_string(meta.preview_width);
    result.previewInfo["height"] = std::to_string(meta.preview_height);
    result.previewInfo["format"] = "srgb_8bit";
    result.previewInfo["creative_style"] = meta.creative_style;
    result.previewInfo["dro"] = meta.dro;
    result.previewInfo["contrast"] = std::to_string(meta.contrast);
    result.previewInfo["saturation"] = std::to_string(meta.saturation);
    result.previewInfo["sharpness"] = std::to_string(meta.sharpness);

    // Estimate base curve from data→preview comparison
    estimateBaseCurve(result.data, result.preview, result.baseCurve);
    result.hasBaseCurve = true;

    // Estimate polynomial coefficients (Camera Math) and serialize to dataInfo
    estimatePolyCoeffs(result.data, result.preview, result.polyCoeffs);
    result.hasPolyCoeffs = true;
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        for (int i = 0; i < Result::POLY_COEFFS_SIZE; i++)
        {
            if (i > 0) oss << ",";
            oss << result.polyCoeffs[i];
        }
        result.dataInfo["poly_coeffs"] = oss.str();
    }

    result.success = true;
    return result;
}

// ============================================================
// Public API
// ============================================================

Result decode(pqtr::Sink& sink)
{
    Format fmt = detectFormat(sink);

    switch (fmt) {
        case Format::SonyARW:
            return decodeSony(sink);

        // Future decoders:
        // case Format::CanonCR2:
        //     return decodeCanon(sink);
        // case Format::NikonNEF:
        //     return decodeNikon(sink);

        default:
            return Result{};  // success = false
    }
}

} // namespace raws
