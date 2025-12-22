// lute.cpp - Camera Profile Learning
//
// LUTE is a container for learned color and tone transforms.
// The actual learning logic is now implemented in the respective
// pipeline plugins (TONE, TUNE).

#include "lute.hpp"

#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace lute {

// Helper to fill empty bins in a curve by linear interpolation
static void fill_empty_bins(float* curve, const std::vector<int>& counts, int size)
{
    for (int i = 0; i < size; ++i) {
        if (counts[i] == 0) {
            curve[i] = -1.0f; // Mark as empty
        }
    }

    for (int i = 0; i < size; ++i) {
        if (curve[i] < 0) {
            int prev = i - 1;
            while (prev >= 0 && curve[prev] < 0) {
                prev--;
            }

            int next = i + 1;
            while (next < size && curve[next] < 0) {
                next++;
            }

            float default_val = static_cast<float>(i) / (size - 1);
            
            if (prev < 0 && next >= size) {
                curve[i] = default_val;
            } else if (prev < 0) {
                curve[i] = curve[next];
            } else if (next >= size) {
                curve[i] = curve[prev];
            } else {
                float t = static_cast<float>(i - prev) / (next - prev);
                curve[i] = curve[prev] * (1.0f - t) + curve[next] * t;
            }
        }
    }
}


// ============================================================================
// CameraLut implementation
// ============================================================================

CameraLut::CameraLut() {
    reset();
}

void CameraLut::reset() {
    tone_sum.assign(TONE_CURVE_SIZE, 0.0);
    tone_count.assign(TONE_CURVE_SIZE, 0);
    hue_sum.assign(HUE_CURVE_SIZE, 0.0);
    hue_count.assign(HUE_CURVE_SIZE, 0);
    sat_sum.assign(SAT_CURVE_SIZE, 0.0);
    sat_count.assign(SAT_CURVE_SIZE, 0);
    val_sum.assign(VAL_CURVE_SIZE, 0.0);
    val_count.assign(VAL_CURVE_SIZE, 0);

    sample_count = 0;
    frozen = false;
    estimated = false;
    camera_make.clear();
    camera_model.clear();
    creative_style.clear();
}

std::string CameraLut::key() const {
    std::string k = camera_make + "_" + camera_model;
    if (!creative_style.empty())
        k += "_" + creative_style;
    return k;
}

void CameraLut::tone_curve(float* out) const {
    for (int i = 0; i < TONE_CURVE_SIZE; i++) {
        if (tone_count[i] > 0) {
            out[i] = static_cast<float>(tone_sum[i] / tone_count[i]);
        } else {
            out[i] = static_cast<float>(i) / (TONE_CURVE_SIZE - 1);
        }
    }
    fill_empty_bins(out, tone_count, TONE_CURVE_SIZE);
}

void CameraLut::hue_curve(float* out) const {
    for (int i = 0; i < HUE_CURVE_SIZE; i++) {
        if (hue_count[i] > 0) {
            out[i] = static_cast<float>(hue_sum[i] / hue_count[i]);
        } else {
            out[i] = static_cast<float>(i) / (HUE_CURVE_SIZE - 1);
        }
    }
    // Note: Hue is circular, so interpolation needs to handle wrap-around.
    // Simple linear fill for now.
    fill_empty_bins(out, hue_count, HUE_CURVE_SIZE);
}

void CameraLut::sat_curve(float* out) const {
    for (int i = 0; i < SAT_CURVE_SIZE; i++) {
        if (sat_count[i] > 0) {
            out[i] = static_cast<float>(sat_sum[i] / sat_count[i]);
        } else {
            out[i] = static_cast<float>(i) / (SAT_CURVE_SIZE - 1);
        }
    }
    fill_empty_bins(out, sat_count, SAT_CURVE_SIZE);
}

void CameraLut::val_curve(float* out) const {
    for (int i = 0; i < VAL_CURVE_SIZE; i++) {
        if (val_count[i] > 0) {
            out[i] = static_cast<float>(val_sum[i] / val_count[i]);
        } else {
            out[i] = static_cast<float>(i) / (VAL_CURVE_SIZE - 1);
        }
    }
    fill_empty_bins(out, val_count, VAL_CURVE_SIZE);
}


// ============================================================================
// Stubs for global functions (logic moved to plugins)
// ============================================================================

bool tune(const float* flat, const uint8_t* target, int width, int height, CameraLut& lut, bool direct) {
    // This logic is now handled by the TONE and TUNE plugins directly.
    return true;
}

void view(const float* in, float* out, int width, int height, const CameraLut& lut) {
    // This logic is now handled by the TONE and TUNE plugins directly.
    // For simplicity, just copy input to output.
    std::copy(in, in + (size_t)width * height * 3, out);
}

// ============================================================================
// Stubs for persistence (TODO: Re-implement for HSV model)
// ============================================================================

bool save(const CameraLut& lut, const std::string& path) {
    std::cerr << "[lute::save] STUB: Persistence for new HSV model is not implemented.\n";
    return false;
}

bool load(CameraLut& lut, const std::string& path) {
    std::cerr << "[lute::load] STUB: Persistence for new HSV model is not implemented.\n";
    return false;
}

} // namespace lute
