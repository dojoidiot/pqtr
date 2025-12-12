# Out of Scope Features

[back](../pipe.md)

This document preserves ideas for features and modules that are currently out of scope for the canonical camera-to-web pipeline. These may be considered in future expansions based on evolving requirements, particularly improvements to the `diff` and `tune` systems.

## Output Formats and Color Spaces

### Additional Output Formats
- **JPEG with quality settings**: Currently, PNG (lossless) is the only output format. JPEG export with configurable quality levels could be added for final web delivery.
- **TIFF (16-bit)**: For print workflows or archival purposes, 16-bit TIFF output would preserve more tonal information than 8-bit PNG.
- **HEIF/HEIC**: Modern container formats with better compression than JPEG while maintaining quality.

### Alternative Color Spaces
- **Display P3**: Wide-gamut color space for modern displays (Apple, high-end monitors). Would require:
  - Additional color space conversion routines
  - P3 gamut mapping to avoid out-of-gamut colors
  - Display profile support
- **Adobe RGB**: Legacy wide-gamut space primarily for print workflows. Largely superseded by Display P3 for digital work.
- **Rec. 2020**: Ultra-wide gamut for HDR and future display technologies.
- **ProPhoto RGB**: Extremely wide working space, mainly for archival and advanced color grading.

## Advanced Image Processing Modules

### Noise Reduction Enhancements
- **Two-stage noise reduction**: A hybrid approach with early linear-space denoising (after Color Correction, before Tone Mapping) plus final output cleanup. Early denoise would:
  - Operate on uniform noise profiles in linear RGB before non-linear transforms amplify artifacts
  - Prevent noise amplification during tone mapping contrast adjustments
  - Require a new 7th module in the BODY, increasing total dial count
  - Keep the existing Detail + Output denoise for final cleanup
- **AI-powered denoising**: Machine learning models (e.g., based on neural networks) for superior noise reduction while preserving detail.
- **Multi-frame noise reduction**: Combine multiple exposures to reduce noise (requires temporal alignment).
- **Chroma vs. luminance separation**: More sophisticated algorithms beyond the current basic implementation.

### Lens and Optical Corrections
- **Lens distortion correction**: Barrel/pincushion distortion correction using lens profiles (similar to Lightroom/DxO).
- **Chromatic aberration correction**: Lateral and longitudinal CA reduction.
- **Vignetting correction**: Automatic or manual compensation for lens vignetting.
- **Purple fringing removal**: Detection and correction of purple fringing artifacts.

### Masking and Local Adjustments
- **AI-powered subject masking**: Automatic detection and masking of subjects (people, objects, animals).
- **Sky masking**: Automatic sky detection for targeted sky adjustments.
- **Gradient masks**: Linear/radial gradients for local adjustments (similar to gradient filters).
- **Brush-based local adjustments**: Manual brush painting for selective edits (not currently possible in headless/tune workflow).

### Advanced Color Grading
- **LUT (Look-Up Table) support**: Apply creative LUTs for film emulation or brand-specific looks.
- **Color grading wheels**: Lift/gamma/gain controls for shadow/midtone/highlight color adjustments.
- **Split toning**: Separate color toning for highlights and shadows.
- **Film grain simulation**: Artistic film grain overlay for analog aesthetic.

### HDR and Exposure Blending
- **HDR merging**: Combine multiple exposures into a single high dynamic range image.
- **Exposure fusion**: Blend exposures without tone mapping (for natural-looking results).
- **Highlight recovery**: Advanced techniques to recover blown highlights using color channel interpolation.

## Diff and Tune System Enhancements

### Advanced Diff Metrics
- **Region-specific weighting**: Weight different image regions differently (e.g., face detection with higher weights for skin tones)
- **CIECAM02 color appearance model**: More sophisticated than CIELAB, accounts for viewing conditions and chromatic adaptation
- **Adaptive metric weighting**: Automatically adjust SSIM/color/luminance weights based on image content
- **Temporal diff for video**: Extend diff to handle sequences of frames for video processing pipelines

## UI and Workflow Features

### Preset Management
- **User presets library**: Save and recall complete edit step configurations.
- **Auto-apply presets**: Based on camera model, scene type, or EXIF metadata.
- **Preset sharing**: Import/export presets in standardized formats.

### Batch Processing
- **Batch tuning**: Apply tune settings from one image to a set of similar images.
- **Auto-sync across series**: Automatically apply edits to all images in a sequence (e.g., a photo burst).

### Metadata and EXIF
- **EXIF preservation**: Copy all camera metadata to output PNG (currently automatic, but could be enhanced).
- **XMP sidecar support**: Alternative to JSON sidecar files for compatibility with other tools.
- **GPS and location data**: Display and edit location information.

## Performance Optimizations

### GPU Acceleration
- **CUDA/OpenCL kernels**: GPU-accelerated versions of compute-intensive modules (demosaic, tone mapping, color space conversions).
- **Vulkan compute**: Modern cross-platform GPU compute for maximum performance.

### Advanced Caching
- **Module output caching**: Cache intermediate results between edit steps to avoid reprocessing.
- **Thumbnail generation**: Fast preview generation for large image sets.

## Why These Are Out of Scope

The current focus is on a **canonical, mathematically-driven pipeline** for camera-to-web workflows with automatic tuning. Features are excluded if they:

1. **Add UI complexity** without improving the core math (presets, brush tools)
2. **Target different workflows** (print, HDR merging, video)
3. **Duplicate functionality** available in post-processing tools (JPEG conversion, LUTs)
4. **Cannot be optimized by `tune`** (local adjustments, masks)
5. **Require extensive testing** to validate benefit to `diff` accuracy (AI models, advanced algorithms)

These features may be reconsidered if:
- The `diff` system is enhanced to measure aspects these features affect (e.g., noise, distortion)
- The `tune` system can automatically optimize these parameters
- User feedback indicates strong need for specific camera-to-web workflows
- Performance requirements necessitate GPU acceleration

---

**Note**: This list is not exhaustive. New ideas may be added as development and testing progress.
