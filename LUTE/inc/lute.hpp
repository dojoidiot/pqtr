// lute.hpp - PQTR Camera Profile Module
//
// LUTE learns and applies camera-specific color transforms.
// This is the "gear manufacturer's style" - what photographers see
// on their LCD when composing shots.
//
// Camera profiles are learned from RAW + embedded JPEG pairs:
//   1. RAWS decodes RAW to scene-linear
//   2. LUTE accumulates RGB→RGB mappings into LUTs
//   3. Profile converges across multiple images
//
// Transforms (learned from camera behavior):
//   - BaseCurve: Camera tone response curve
//   - PolyColor: Polynomial color transform
//   - LutCurve: 17³ 3D LUT (full color mapping)
//   - HsvLut: HSV delta corrections
//
// Usage:
//   auto lute = lute::create();
//   lute->tune(flat, preview);      // Learn from RAW+preview pair
//   auto out = lute->view(image);   // Apply camera profile

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>

namespace lute
{
    // Forward declarations
    class Lute;

    // ============================================================
    // Type Aliases
    // ============================================================

    using View = cv::UMat;
    using Name = std::string;
    using Grid = const float *;
    using Hold = std::unique_ptr<Lute>;
    class Lute
    {
    public:
        // ============================================================
        // BaseCurve - Camera tone response (768 floats)
        // ============================================================

        class BaseCurve
        {
        public:
            virtual ~BaseCurve() = default;
            static constexpr int CURVE_LEN = 256;
            static constexpr int CURVE_CHANNELS = 3;
            static constexpr int CURVE_SIZE = CURVE_LEN * CURVE_CHANNELS;

            virtual const float *curve() const = 0;
            virtual void setCurve(const float *values) = 0;
            virtual void reset() = 0;
            virtual bool isActive() const = 0;
        };

        // ============================================================
        // PolyColor - Polynomial color transform (30 floats)
        // ============================================================

        class PolyColor
        {
        public:
            virtual ~PolyColor() = default;
            static constexpr int COEFFS_PER_CHANNEL = 10;
            static constexpr int COEFFS_SIZE = COEFFS_PER_CHANNEL * 3;

            virtual const float *coeffs() const = 0;
            virtual void setCoeffs(const float *values) = 0;
            virtual void reset() = 0;
            virtual bool isActive() const = 0;

            // Estimate from image pair
            virtual bool estimate(View base, View target) = 0;
        };

        // ============================================================
        // LutCurve - 17³ 3D LUT (14739 floats)
        // ============================================================

        class LutCurve
        {
        public:
            virtual ~LutCurve() = default;
            static constexpr int GRID_SIZE = 17;
            static constexpr int LUT_SIZE = GRID_SIZE * GRID_SIZE * GRID_SIZE * 3;

            virtual const float *lut() const = 0;
            virtual void setLut(const float *values) = 0;
            virtual void reset() = 0;
            virtual bool isEstimated() const = 0;

            // Estimate from image pair
            virtual bool estimate(View base, View target) = 0;
        };

        // ============================================================
        // HsvLut - HSV delta table (1296 floats)
        // ============================================================

        class HsvLut
        {
        public:
            virtual ~HsvLut() = default;
            static constexpr int H_BINS = 36;
            static constexpr int S_BINS = 12;
            static constexpr int LUT_SIZE = H_BINS * S_BINS * 3;

            virtual const float *lut() const = 0;
            virtual void setLut(const float *values) = 0;
            virtual void reset() = 0;
            virtual bool isEstimated() const = 0;

            // Estimate from image pair
            virtual bool estimate(View base, View target) = 0;
        };

        // ============================================================
        // Profile - Camera-specific color profile
        // ============================================================
        //
        // Keyed by: camera_model + creative_style + dro
        // Contains all four transform types

        class Profile
        {
        public:
            virtual ~Profile() = default;

            // Profile identity
            virtual Name key() const = 0;
            virtual Name cameraModel() const = 0;
            virtual Name creativeStyle() const = 0;
            virtual Name dro() const = 0;

            // Transform access
            virtual BaseCurve &baseCurve() = 0;
            virtual PolyColor &polyColor() = 0;
            virtual LutCurve &lutCurve() = 0;
            virtual HsvLut &hsvLut() = 0;

            // Coverage tracking
            virtual float coverage() const = 0;
            virtual bool converged(float threshold = 0.001f) const = 0;
            virtual int sampleCount() const = 0;

            // Persistence
            virtual bool save(const Name &path) const = 0;
            virtual bool load(const Name &path) = 0;
            virtual void reset() = 0;
        };

        // ============================================================
        // Lute - Profile manager
        // ============================================================

        virtual ~Lute() = default;

        // Current profile
        virtual Profile *profile() = 0;
        virtual const Profile *profile() const = 0;

        // Set profile key (loads from disk if exists)
        virtual void setKey(const Name &cameraModel,
                            const Name &creativeStyle,
                            const Name &dro) = 0;

        // Apply profile to image
        virtual View view(View in) = 0;

        // Learn profile from RAW+preview pair
        virtual bool tune(View flat, View preview) = 0;

        // Profile directory (~/.pqtr/var/profiles/)
        virtual Name profileDir() const = 0;

        // Save current profile
        virtual bool save() = 0;
    };

    // Factory
    Hold create();
    Hold create(const Name &cameraModel,
                const Name &creativeStyle,
                const Name &dro);

} // namespace lute
