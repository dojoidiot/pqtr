# Pipe Specification

[back](../README.md)

## Purpose

The `pipe` part implements the 6 golden modules for RAW image processing. It transforms camera RAW data into scene-linear RGB, applies processing through a sequence of named links, and outputs PNG files. The pipe is designed for headless operation by diff and tune tools.

## Operating Model

The pipe uses a **builder pattern** with three stages:

**Pipe → Head → Body → Tail**

### Flow

1. **Open**: User provides RAW data via sink → receives Head
2. **Decode**: Head decodes RAW → produces Data (View + Info)
3. **Process**: Body creates/manages Links → each Link runs the 6 modules
4. **Finalize**: Tail encodes PNG → writes to sink

## Architecture

### Data

Combines image data with metadata:
- **View**: GPU-accelerated image matrix (`cv::UMat`)
- **Info**: Metadata map (EXIF, camera info, etc.)

Data flows through the pipe and changes at each stage.

### Task

Base interface for all processing units:
```cpp
class Task {
    virtual View run(View view) = 0;  // Process view
    virtual bool set() = 0;           // Returns true if any dial modified
};
```

### Head

Decodes RAW data from sink into scene-linear RGB.

**Methods:**
- `data()` - Access decoded image data and metadata
- `body()` - Continue to body processing

The Head automatically decodes when created.

### Body

Manages a sequence of named Links. Each Link contains the 6 golden modules.

**Methods:**
- `add(name)` - Create a new link with given name
- `get(name)` - Retrieve existing link by name
- `all()` - Get iterator over all links
- `data()` - Current state of image data and metadata
- `tail()` - Finalize processing

### Link

A named collection of the 6 golden modules. Extends Task interface.

**Methods:**
- `name()` - This link's identifier
- `geometric()` - Access geometric transformations
- `colorCorrection()` - Access color correction
- `toneMapping()` - Access tone mapping
- `globalColor()` - Access global color adjustments
- `selectiveColour()` - Access selective color (hue mixer)
- `detail()` - Access sharpening and denoising

**Module Activation:**
Modules become active when any dial is set (tracked via `Task.set()`).

### Tail

Finalizes processing and writes PNG output to sink.

**Methods:**
- `save()` - Write final PNG data to the sink

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
**Sub-Modules**: White Balance (2 dials), Exposure (1 dial)
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
**Color Space**: LCH (perceptually uniform)

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
2. Color Correction (White Balance → Exposure)
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

### Color Space Transitions

The pipe automatically handles color space conversions:
- **SPATIAL** → Used for geometric operations (x,y coordinates)
- **SCENE_LINEAR_RGB** → Camera-native linear RGB
- **LINEAR_RGB** → Working space (D65 white point)
- **LCH** → Perceptual adjustments (CIELAB cylindrical)
- **SRGB** → Standard output (gamma-encoded)

---

## Usage Patterns

### Library Usage (C++)

```cpp
#include <pipe.hpp>

// 1. Prepare sink with RAW data
pqtr::Sink sink;
// ... load RAW file into sink ...

// 2. Open pipe
pipe::Pipe pipe;
auto head = pipe.open(sink);

// 3. Decode RAW
auto headData = head.data();  // Scene-linear RGB + metadata

// 4. Create body and links
auto body = head.body();
auto link1 = body.add("tune_optimize");

// 5. Set dials on modules
auto colorCorrection = link1.colorCorrection();
colorCorrection.exposure().set(0.65);
colorCorrection.whiteBalance().temperature(0.52);

auto globalColor = link1.globalColor();
globalColor.saturation().set(0.68);

// 6. Finalize and save
auto tail = body.tail();
tail.save();  // PNG data now in sink

// 7. Extract PNG from sink
// ... read from sink and write to file ...
```

### Command-Line Tool

The `pipe` executable provides headless processing:

```bash
# Process with default neutral dials
./pipe input.ARW output.png

# Process with configuration file (see data.md for format)
./pipe input.ARW output.png --config processing.json
```

**Note**: Configuration file format is documented in [data.md](./data.md).

---

## Integration

### With Tune

The `tune` tool uses pipe to optimize dial values:
1. Tune creates Links and sets initial dial values
2. Tune iteratively adjusts dials to minimize diff
3. Optimized dial values can be saved to configuration

### With Diff

The `diff` tool compares pipe output against reference images:
1. Pipe processes RAW with current dial settings
2. Diff computes perceptual distance metrics
3. Results inform tune optimization

### Data Persistence

Link configurations (names, dial values) are persisted via the `labs` data format. See [data.md](./data.md) for:
- JSON schema
- Link persistence format
- Dial value encoding
- Example configurations

---

## Design Principles

### Task-Based Composition

All processing units implement the Task interface:
- Uniform `run(View) → View` execution model
- Activation tracking via `set()` method
- Composable and testable

### Builder Pattern

The pipeline flow enforces proper sequencing:
- Can't access Body without Head (must decode first)
- Can't access Tail without Body (must process first)
- Each stage is single-use (builder pattern)

### Sink-Based I/O

RAW input and PNG output both use the sink:
- User owns the sink (caller controls memory)
- Pipe reads from sink (RAW data)
- Pipe writes to sink (PNG data)
- Enables streaming and flexible I/O

### Module Immutability

The 6 golden modules and their dial counts are **immutable** (defined by module docs). Architecture changes must not alter:
- Module count (always 6)
- Dial counts (always 45 total)
- Color spaces (defined per module)
- Processing order (geometric → correction → tone → color → detail)

---

## Performance Characteristics

Target performance (from [README.md](../README.md) success criteria):

- **Pipe throughput**: 30+ fps @ 1080p
- **GPU acceleration**: All View processing uses cv::UMat
- **Link overhead**: Minimal (dials checked via `Task.set()`)
- **Memory efficiency**: Sink reuse via `tidy()` method
