// lute.hpp - PQTR Camera Profile LUT Module
//
// LUTE manages camera-specific color profiles learned from RAW/JPEG pairs.
// Implements pipe::Task interface for integration with LABS.
//
// Key responsibilities:
//   - Accumulate 3D LUTs from flat/preview image pairs
//   - Save/load profiles to ~/.pqtr/var/profiles/
//   - Apply learned profiles to flatten camera→display differences
//
// Usage:
//   auto lute = lute::create();         // Factory
//   lute->tune(data);                   // Accumulate from RAW (has flat + preview)
//   auto out = lute->view(data);        // Apply profile LUT

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/core.hpp>

namespace lute
{
    // ============================================================
    // Type Aliases (compatible with pipe::)
    // ============================================================

    using View = cv::UMat;
    using Name = std::string;

    // ============================================================
    // Profile - Camera-specific 3D LUT
    // ============================================================
    //
    // Captures RGB→RGB transform from scene-linear to display-referred.
    // Accumulates across multiple images for robust estimation.
    // Keyed by: camera_model + creative_style + dro

    class Profile
    {
    public:
        virtual ~Profile() = default;

        static constexpr int GRID_SIZE = 17;
        static constexpr int CELLS = GRID_SIZE * GRID_SIZE * GRID_SIZE;  // 4,913
        static constexpr int LUT_SIZE = CELLS * 3;  // 14,739

        // Profile identity
        virtual Name key() const = 0;          // e.g., "Sony_ILCE-7M4_Standard_DRO-Auto"
        virtual Name cameraModel() const = 0;
        virtual Name creativeStyle() const = 0;
        virtual Name dro() const = 0;

        // LUT access
        virtual const float* lut() const = 0;
        virtual void lut(float* out) const = 0;  // Copy to caller buffer

        // Coverage: fraction of cells with data (0.0 - 1.0)
        virtual float coverage() const = 0;
        virtual int emptyCells() const = 0;

        // Convergence tracking
        virtual bool converged(float threshold = 0.001f) const = 0;
        virtual float lastDelta() const = 0;
        virtual int sampleCount() const = 0;
        virtual bool frozen() const = 0;

        // Describe missing coverage
        virtual std::vector<Name> missing() const = 0;

        // Reset accumulators
        virtual void reset() = 0;

        // Persistence
        virtual bool save(const Name& path) const = 0;
        virtual bool load(const Name& path) = 0;
    };

    // ============================================================
    // Lute - Profile manager and Task implementation
    // ============================================================
    //
    // Manages profile lifecycle:
    //   - Auto-loads matching profile based on image EXIF
    //   - view(): Apply profile LUT to image
    //   - tune(): Accumulate profile from flat/preview pair

    class Lute
    {
    public:
        virtual ~Lute() = default;

        // Current profile (may be null if no match)
        virtual Profile* profile() = 0;
        virtual const Profile* profile() const = 0;

        // Set profile key (camera_model, creative_style, dro)
        // Loads from ~/.pqtr/var/profiles/ if exists
        virtual void setKey(const Name& cameraModel,
                           const Name& creativeStyle,
                           const Name& dro) = 0;

        // Apply profile LUT to image
        // If no profile loaded, returns input unchanged
        virtual View view(View in) = 0;

        // Accumulate profile from flat/preview pair
        // flat: Scene-linear from RAWS
        // preview: Camera JPEG (embedded or external)
        // Returns true if profile was updated
        virtual bool tune(View flat, View preview) = 0;

        // Analyze: count NEW cells that would be filled
        virtual int analyze(View flat, View preview) const = 0;

        // Profile directory (~/.pqtr/var/profiles/)
        virtual Name profileDir() const = 0;

        // Save current profile to disk
        virtual bool save() = 0;
    };

    // Factory functions
    std::unique_ptr<Lute> create();
    std::unique_ptr<Lute> create(const Name& cameraModel,
                                 const Name& creativeStyle,
                                 const Name& dro);

} // namespace lute
