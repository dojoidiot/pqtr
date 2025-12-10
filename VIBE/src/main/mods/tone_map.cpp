// tone_map.cpp - VIBE
// Tone Mapping Module - Filmic HDR to SDR compression (7 dials)
// Luminance-preserving: applies curve to L, scales RGB proportionally

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{

bool tone_map(const View& in, View& out,
    Dial contrast_dial, Dial highlights_dial, Dial shadows_dial,
    Dial toe_pivot_dial, Dial shoulder_pivot_dial,
    Dial white_point_dial, Dial black_point_dial)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::tone_map] invalid input\n";
        return false;
    }

    contrast_dial = std::clamp(contrast_dial, 0.0f, 1.0f);
    highlights_dial = std::clamp(highlights_dial, 0.0f, 1.0f);
    shadows_dial = std::clamp(shadows_dial, 0.0f, 1.0f);
    toe_pivot_dial = std::clamp(toe_pivot_dial, 0.0f, 1.0f);
    shoulder_pivot_dial = std::clamp(shoulder_pivot_dial, 0.0f, 1.0f);
    white_point_dial = std::clamp(white_point_dial, 0.0f, 1.0f);
    black_point_dial = std::clamp(black_point_dial, 0.0f, 1.0f);

    // Convert dials to working values (all neutral at 0.5)
    float contrast = 0.5f * std::exp(contrast_dial * 1.792f);
    float highlights = (highlights_dial - 0.5f) * 2.0f;
    float shadows = (shadows_dial - 0.5f) * 2.0f;
    float toe_pivot = 0.1f + toe_pivot_dial * 0.4f;
    float shoulder_pivot = 0.5f + shoulder_pivot_dial * 0.4f;
    bool bypass_reinhard = (white_point_dial > 0.45f && white_point_dial < 0.55f);
    float white_point = 2.0f + white_point_dial * 4.0f;
    float black_point = (black_point_dial - 0.5f) * 0.5f;
    const float mask_steepness = 12.0f;

    // Extract luminance from BGR
    std::vector<View> channels(3);
    cv::split(in, channels);

    View L;
    cv::addWeighted(channels[0], 0.0722f, channels[1], 0.7152f, 0.0f, L);
    cv::addWeighted(L, 1.0f, channels[2], 0.2126f, 0.0f, L);

    View L_orig;
    L.copyTo(L_orig);
    cv::max(L_orig, 0.0001f, L_orig);

    // Black point adjustment
    if (std::abs(black_point) > 0.001f)
    {
        if (black_point > 0)
        {
            cv::subtract(L, black_point, L);
            cv::max(L, 0.0f, L);
            cv::multiply(L, 1.0f / (1.0f - black_point), L);
        }
        else
        {
            float abs_bp = std::abs(black_point);
            cv::multiply(L, 1.0f - abs_bp, L);
            cv::add(L, abs_bp, L);
        }
    }

    // Extended Reinhard
    if (!bypass_reinhard)
    {
        float w2 = white_point * white_point;
        View L_sq, term2, num, denom;
        cv::multiply(L, L, L_sq);
        cv::divide(L_sq, w2, term2);
        cv::add(L, term2, num);
        cv::add(L, 1.0f, denom);
        cv::divide(num, denom, L);
    }

    // Shadow adjustment
    if (std::abs(shadows) > 0.01f)
    {
        View shifted, exp_val, shadow_mask;
        cv::subtract(L, toe_pivot, shifted);
        cv::multiply(shifted, mask_steepness, shifted);
        cv::exp(shifted, exp_val);
        cv::add(exp_val, 1.0f, shadow_mask);
        cv::divide(1.0f, shadow_mask, shadow_mask);

        float gamma = std::clamp(1.0f - shadows * 0.5f, 0.3f, 2.0f);
        View lifted, adjusted;
        cv::add(L, 0.001f, lifted);
        cv::pow(lifted, gamma, adjusted);

        View inv_mask, shadow_part, keep_part;
        cv::subtract(1.0f, shadow_mask, inv_mask);
        cv::multiply(adjusted, shadow_mask, shadow_part);
        cv::multiply(L, inv_mask, keep_part);
        cv::add(shadow_part, keep_part, L);
    }

    // Highlight adjustment
    if (std::abs(highlights) > 0.01f)
    {
        View shifted, exp_val, highlight_mask;
        cv::subtract(L, shoulder_pivot, shifted);
        cv::multiply(shifted, -mask_steepness, shifted);
        cv::exp(shifted, exp_val);
        cv::add(exp_val, 1.0f, highlight_mask);
        cv::divide(1.0f, highlight_mask, highlight_mask);

        float gamma = std::clamp(1.0f + highlights * 0.5f, 0.3f, 2.0f);
        View inv_L, adjusted;
        cv::subtract(1.0f, L, inv_L);
        cv::max(inv_L, 0.001f, inv_L);
        cv::pow(inv_L, gamma, adjusted);
        cv::subtract(1.0f, adjusted, adjusted);

        View inv_mask, highlight_part, keep_part;
        cv::subtract(1.0f, highlight_mask, inv_mask);
        cv::multiply(adjusted, highlight_mask, highlight_part);
        cv::multiply(L, inv_mask, keep_part);
        cv::add(highlight_part, keep_part, L);
    }

    // Contrast adjustment
    if (std::abs(contrast - 1.0f) > 0.01f)
    {
        cv::subtract(L, 0.5f, L);
        View sign_mask;
        cv::compare(L, 0.0f, sign_mask, cv::CMP_GE);
        sign_mask.convertTo(sign_mask, CV_32FC1, 1.0/255.0);

        View abs_L, powered;
        cv::absdiff(L, 0.0f, abs_L);
        cv::multiply(abs_L, 2.0f, abs_L);
        cv::add(abs_L, 0.001f, abs_L);
        cv::pow(abs_L, contrast, powered);
        cv::multiply(powered, 0.5f, powered);

        View pos_part, neg_part, inv_sign;
        cv::multiply(powered, sign_mask, pos_part);
        cv::subtract(1.0f, sign_mask, inv_sign);
        cv::multiply(powered, inv_sign, neg_part);
        cv::multiply(neg_part, -1.0f, neg_part);
        cv::add(pos_part, neg_part, L);
        cv::add(L, 0.5f, L);
    }

    cv::max(L, 0.0f, L);
    cv::min(L, 1.0f, L);

    // Scale RGB by luminance ratio
    View scale;
    cv::divide(L, L_orig, scale);
    cv::multiply(channels[0], scale, channels[0]);
    cv::multiply(channels[1], scale, channels[1]);
    cv::multiply(channels[2], scale, channels[2]);

    View result;
    cv::merge(channels, result);
    cv::max(result, 0.0f, result);
    cv::min(result, 1.0f, out);

    return true;
}

} // namespace mods
} // namespace vibe
