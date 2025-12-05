// RAWS - RAW decoder library
// Decodes RAW files to scene-linear RGB
// Auto-detects format (Sony ARW, Canon CR2/CR3, Nikon NEF, etc.)

#pragma once

#include "pipe.hpp"
#include "sink.hpp"

namespace raws {

    // Decode options - control what processing is applied
    struct Options {
        bool undistort = true;   // Apply lens distortion correction (default: on)
        // Future options:
        // bool crop = true;     // Apply optical black crop
    };

    // Decoded RAW result
    struct Result {
        bool success = false;

        // Scene-linear RGB data
        pipe::View data;          // CV_32FC3, scene-linear sRGB
        pipe::Info dataInfo;      // Metadata (decoder, dimensions, camera, etc.)

        // Embedded camera preview
        pipe::View preview;       // CV_8UC3, display-referred sRGB
        pipe::Info previewInfo;   // Preview metadata (creative style, etc.)

        // Color matrix (3x3) - cross-channel color transform
        // Captures hue rotation and color grading that per-channel curves can't
        // Applied in linear space: [R',G',B']^T = M × [R,G,B]^T
        // Row-major order: [m00,m01,m02, m10,m11,m12, m20,m21,m22]
        // Identity matrix = [1,0,0, 0,1,0, 0,0,1]
        static constexpr int MATRIX_SIZE = 9;
        float colorMatrix[MATRIX_SIZE];
        bool hasColorMatrix = false;

        // Base curve (derived from data→preview comparison, AFTER matrix)
        // Per-channel RGB curves: maps gamma-space input [0-255] to output [0-1]
        // Layout: [B0..B255, G0..G255, R0..R255] (BGR order for OpenCV)
        static constexpr int CURVE_LEN = 256;
        static constexpr int CURVE_CHANNELS = 3;
        static constexpr int CURVE_SIZE = CURVE_LEN * CURVE_CHANNELS;  // 768
        float baseCurve[CURVE_SIZE];
        bool hasBaseCurve = false;

        // Polynomial color transform (Camera Math)
        // Quadratic polynomial per output channel: Out = c0 + c1*R + c2*G + c3*B + c4*R² + c5*G² + c6*B² + c7*RG + c8*RB + c9*GB
        // Layout: [R_coeffs(10), G_coeffs(10), B_coeffs(10)]
        // Estimated from scene-linear→preview comparison via least squares
        static constexpr int POLY_COEFFS_PER_CHANNEL = 10;
        static constexpr int POLY_COEFFS_SIZE = POLY_COEFFS_PER_CHANNEL * 3;  // 30
        float polyCoeffs[POLY_COEFFS_SIZE];
        bool hasPolyCoeffs = false;
    };

    // Decode a RAW file
    // Auto-detects format from file signature
    // Default options: full processing (undistort enabled)
    Result decode(pqtr::Sink& sink, const Options& opts = Options{});

} // namespace raws
