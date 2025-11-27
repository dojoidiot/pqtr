# Pipe Specification

[back](../README.md)

## Purpose

The `pipe` part implements the 6 golden modules for RAW image processing. It transforms camera RAW data into scene-linear RGB, applies processing modules, and outputs PNG files. The pipe is designed for headless operation by diff and tune tools.

## Operating Model

The pipe uses a **PIMPL builder pattern** with three stages:

**HEAD → BODY → TAIL**

### Flow

1. **HEAD**: `pipe->open()` decodes RAW → scene-linear RGB + metadata
2. **BODY**: `body.add()` creates Links with 6 golden modules (45 dials)
3. **TAIL**: `tail.save()` applies gamma and outputs PNG

## Architecture

### Types

```cpp
namespace pipe {
    using View = cv::UMat;                        // GPU-accelerated image
    using Info = std::map<std::string, std::string>;  // Metadata map
}
```

### Data

Contains decoded RAW data:
- **view()**: Scene-linear RGB (`CV_32FC3`, [0,1+] range)
- **info()**: Metadata (camera, EXIF, dimensions, etc.)

```cpp
class Data {
public:
    virtual Info info() = 0;
    virtual View view() = 0;
};
```

### HEAD Stage

```cpp
// Create pipe instance
pqtr::Hold<pipe::Pipe> pipe = pipe::make();

// Decode RAW → scene-linear RGB (decoder auto-detected from file signature)
pqtr::Hold<pipe::Head> head = pipe->open(sink);

// Access decoded data
pipe::Data& data = head->data();
```

### BODY Stage

Processing via Links containing 6 golden modules.

**Working Size**: Body can process at a reduced size for faster preview:

```cpp
// Continue from HEAD to BODY with optional working size
pipe::Body& body = head->body(1024);  // Process at 1024px for fast preview
pipe::Body& body = head->body();      // Process at full resolution (default)
```

**Activation Model**: Modules only run when dials are set. Setting any dial activates its parent module. If no dials are set, the module is skipped (passthrough).

```cpp
// Create a named Link with all 6 modules
pipe::Body::Link& link = body.add("tune");

// Setting a dial activates that module
link.colorCorrection().exposure().set(0.6f);        // activates ColorCorrection
link.toneMapping().contrast().set(0.55f);           // activates ToneMapping
link.globalColor().vibrance().set(0.6f);            // activates GlobalColor

// Modules with no dials set are skipped (Geometric, SelectiveColour, Detail)

// Get display-ready view (8-bit BGR, gamma encoded)
pipe::View display = body.view();       // At working size
pipe::View display = body.view(512);    // Further scaled to 512px
```

**Modules** (see [libs.md](./libs.md) for implementation details):
- `geometric()` - 6 dials (Crop, Zoom, Rotation)
- `colorCorrection()` - 3 dials (Exposure, WhiteBalance)
- `toneMapping()` - 5 dials (Contrast, CurveAdjustment, ClippingPoint)
- `globalColor()` - 3 dials (Vibrance, Saturation, ColourDensity)
- `selectiveColour()` - 24 dials (8 colors × 3 HSL dials)
- `detail()` - 4 dials (Sharpen, Denoise)

### TAIL Stage

Tail has access to Head's full-resolution data and can export at any size:

```cpp
// Continue from BODY to TAIL for export
pipe::Tail& tail = body.tail();

// Save to PNG - Tail runs pipeline on full-res data at requested output size
// max_dim: 0 = full resolution, >0 = scale to fit before processing
tail.save("/path/to/output.png");           // Full resolution
tail.save("/path/to/social.png", 1080);     // Process at 1080px, save
tail.save("/path/to/web.png", 2048);        // Process at 2048px, save
```

---

## The 6 Golden Modules

Each module contains sub-modules that work together. See [module documentation](./mods/) for complete specifications.

### 1. [Geometric](./mods/geometric.md)
**Purpose**: Geometric transformations (crop, scale, rotate)
**Sub-Modules**: Crop (4 dials), Zoom (1 dial), Rotation (1 dial)
**Total**: 6 dials
**Color Space**: SPATIAL

### 2. [Color Correction](./mods/color_correction.md)
**Purpose**: Camera-native RGB to device-independent color space
**Sub-Modules**: Exposure (1 dial), White Balance (2 dials)
**Total**: 3 dials
**Color Space**: LINEAR_RGB

### 3. [Tone Mapping](./mods/tone_mapping.md)
**Purpose**: HDR to SDR compression with perceptual contrast
**Sub-Modules**: Contrast (1 dial), Curve Adjustment (2 dials), Clipping Point (2 dials)
**Total**: 5 dials
**Color Space**: LINEAR_RGB

### 4. [Global Color](./mods/global_color.md)
**Purpose**: Overall color intensity and saturation
**Sub-Modules**: Vibrance (1 dial), Saturation (1 dial), Color Density (1 dial)
**Total**: 3 dials
**Color Space**: LCH (perceptually uniform)

### 5. [Selective Color](./mods/selective_color.md)
**Purpose**: Targeted adjustments to specific color ranges
**Sub-Modules**: HSL Adjustment × 8 colors (3 dials each)
**Total**: 24 dials
**Color Space**: HLS (via gamma-encoded conversion)

### 6. [Detail + Output](./mods/detail_output.md)
**Purpose**: Finalization with sharpening, noise reduction, output transform
**Sub-Modules**: Sharpen (2 dials), Denoise (2 dials), Output Transform (automatic)
**Total**: 4 dials
**Color Space**: LINEAR_RGB (sharpen), LCH (denoise), SRGB (output)

**Total Dials**: 6 + 3 + 5 + 3 + 24 + 4 = **45 dials**

---

## Processing Model

### Module Processing Order

Within each Link, modules are applied in this order:
1. Geometric (Crop → Zoom → Rotation)
2. Color Correction (Exposure → White Balance)
3. Tone Mapping (Contrast → Curve Adjustment → Clipping Point)
4. Global Color (Vibrance → Saturation → Color Density)
5. Selective Color (8 color bands)
6. Detail + Output (Sharpen → Denoise → Output Transform)

### Link Sequencing

Links execute in the order they were added to the Body. Common patterns:

**Pattern 1: Single tune link**
- One link with color/tone modules active
- Used for matching camera JPEG

**Pattern 2: Geometry + tune links**
- First link: Geometric module only (user-adjusted)
- Second link: Color/tone modules (auto-optimized by tune)
- Used for matching social media images with different framing

**Pattern 3: Creative workflow**
- Multiple links with different modules active
- Geometry typically in final link (composition)

### Channel Order Convention

All image data returned by `view()` methods uses **BGR channel order** (OpenCV convention):

- `head->view().view()` - Embedded preview, BGR, 8-bit
- `body.view()` - Display-ready output, BGR, 8-bit
- `body.tail().view()` - Final output, BGR, 8-bit

This ensures compatibility with `cv::imwrite()` and other OpenCV functions.

**Internal processing** uses RGB order for color math, but conversion to BGR happens automatically when calling any `view()` method.

### Color Space Transitions

The pipe automatically handles color space conversions:
- **SPATIAL** → Used for geometric operations (x,y coordinates)
- **SCENE_LINEAR_RGB** → Camera-native linear RGB
- **LINEAR_RGB** → Working space (D65 white point)
- **LCH** → Perceptual adjustments (CIELAB cylindrical)
- **SRGB** → Standard output (gamma-encoded)

---

## Working Size Architecture

The pipe supports a two-tier sizing model for fast preview and high-quality export:

### Body Working Size

`head->body(working_size)` controls the resolution used for preview processing:

- **working_size = 0** (default): Process at full decoded resolution
- **working_size > 0**: Scale decoded data down before processing (faster)

### Body View Size

`body.view(max_dim)` returns display-ready 8-bit BGR:

- **max_dim = 0** (default): Return at working size
- **max_dim > 0**: Further scale down for display (e.g., thumbnail)

### Tail Output Size

`tail.save(path, max_dim)` exports using Head's full-resolution data:

- **max_dim = 0** (default): Process and save at full resolution
- **max_dim > 0**: Scale full-res data down, process, save

### Workflow: Edit Small, Export Full

```cpp
// Create pipe and configure settings
auto head = pipe->open(sink);
auto& body = head->body(1024);   // Work at 1024px for fast preview
auto& link = body.add("tune");

// Adjust dials with instant feedback
link.colorCorrection().exposure().set(0.6f);
link.toneMapping().contrast().set(0.55f);

// Display preview (runs pipe at 1024px working size)
auto display = body.view();      // 1024px display
auto thumb = body.view(256);     // Further scaled to 256px thumbnail

// Export at multiple sizes (Tail uses Head's full-res data)
body.tail().save("output_full.png");             // Full resolution
body.tail().save("output_web.png", 2048);        // 2048px
body.tail().save("output_social.png", 1080);     // 1080px
```

The resize uses `cv::INTER_AREA` for proper downscaling in linear space before processing.

---

## Usage Patterns

### Library Usage (C++)

```cpp
#include <pipe.hpp>
#include <tool.hpp>

void processRaw(const std::string& rawPath, const std::string& outPath) {
    // Load RAW file into Sink
    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(rawPath));

    // Create pipe and open HEAD (decoder auto-detected)
    pqtr::Hold<pipe::Pipe> pipe = pipe::make();
    pqtr::Hold<pipe::Head> head = pipe->open(std::move(sink));

    // Continue to BODY with working size for fast preview
    pipe::Body& body = head->body(1024);  // Preview at 1024px

    // Create and configure a Link (setting dials activates modules)
    pipe::Body::Link& link = body.add("tune");
    link.colorCorrection().exposure().set(0.6f);          // +0.8 EV
    link.toneMapping().contrast().set(0.55f);
    link.toneMapping().curveAdjustment().highlights().set(0.45f);
    link.toneMapping().curveAdjustment().shadows().set(0.55f);
    link.globalColor().vibrance().set(0.6f);
    link.globalColor().saturation().set(0.55f);

    // Get display preview (runs pipe at working size)
    pipe::View preview = body.view();  // 8-bit BGR at 1024px

    // Export via TAIL (uses Head's full-res data, same link settings)
    body.tail().save(outPath + "_social.png", 1080);  // Social media
    body.tail().save(outPath + "_web.png", 2048);     // Website
    body.tail().save(outPath + "_full.png");          // Full resolution
}
```

### Command-Line Tool

The `pipe` executable provides headless processing:

```bash
# Process with default (neutral) dials
./pipe input.ARW

# Process with display-referred tone mapping
./pipe input.ARW --default-display

# Process with output directory
./pipe input.ARW --default-display /path/to/output
```

**Output**: Creates `<input>.png` and `<input>.ARW.pipe.json` sidecar.

**Note**: Configuration file format is documented in [data.md](./data.md).

---

## Integration

### With Tune

The `tune` tool uses pipe to optimize dial values:
1. Tune loads RAW via `pipe->open()`
2. Tune creates Links and iteratively adjusts dial values via getters/setters
3. Tune uses `diff` to measure loss against reference
4. Optimized dial values saved to `.pipe.json` sidecar (in body section)

### With Diff

The `diff` tool compares pipe output against reference images:
1. Pipe processes RAW with current dial settings
2. Diff computes spectral loss (color/tone) and frequency loss (sharpness)
3. Results inform tune optimization

### Data Persistence

Dial values are persisted in `.pipe.json` sidecar files. See [data.md](./data.md) for:
- JSON schema
- Dial value encoding
- Example configurations

---

## Design Principles

### PIMPL Builder Pattern

Clean object-oriented API with implementation hiding:
- `pipe::make()` - Factory creates Pipe instance
- `pipe->open()` - HEAD (decode) returns Head
- `head->body()` - BODY (process) returns Body with Links
- `body.tail()` - TAIL (output) returns Tail for save

User apps only see `inc/pipe.hpp` and link against `lib/labs.so`. All module implementations are hidden in the library.

### Activation Model

Modules use lazy activation:
- By default, no modules are active (passthrough)
- Setting any dial value activates that module
- Only active modules are processed in `body.tail()`
- Zero overhead for unused modules

### Decoder Abstraction

RAW decoders are internal implementation details:
- Decoder auto-detected from file signature in sink
- Consumer code simply calls `pipe->open(sink)`
- Future decoders added without API changes

### Module Immutability

The 6 golden modules and their dial counts are **immutable**:
- Module count (always 6)
- Dial counts (always 45 total)
- Color spaces (defined per module)
- Processing order (geometric → correction → tone → color → detail)

---

## Performance Characteristics

Target performance (from [README.md](../README.md) success criteria):

- **Pipe throughput**: 30+ fps @ 1080p
- **GPU acceleration**: All View processing uses cv::UMat
- **Memory efficiency**: Direct UMat operations, no intermediate copies

---

## Internal Structure

The pipe module is split into focused files:

| File | Purpose |
|------|---------|
| `pipe.cpp` | HEAD/BODY/TAIL/Pipe classes |
| `view.cpp` | Display conversion (linear → sRGB gamma) |
| `link.cpp` | LinkImpl + module implementations (430 lines) |
| `mods/*.cpp` | Processing kernels (45 dials) |

See [libs.md](./libs.md) for full source structure.
