# Labs System Integration 

[back](../README.md)

This document describes how the various components within the `PQTR:LABS` system integrate and interact to provide RAW image processing with automatic style transfer capabilities.

## System Overview

The `LABS` system consists of several independent programs (executables and libraries) that work together:

```
┌─────────┐
│  raws   │  Existing: RAW decode → scene-referred linear RGB
└────┬────┘
     │ linear_rgb.exr (CV_32FC3, scene-referred)
     ↓
┌─────────┐
│  pipe   │  6-module creative grading pipeline
└────┬────┘  Input: linear RGB, 25 dials → Output: display RGB
     │ display_rgb.png
     │
     ├──────────────────────────────────┐
     │                                  │
     ↓                                  ↓
┌─────────┐                        ┌─────────┐
│  diff   │  Spectral loss         │  tune   │  Two-stage optimizer
└─────────┘  (color/tone) +        └─────────┘  3D LUT + SPSA (17 color dials)
             Frequency loss                     + Edge (2 detail dials)
             (sharpness)                        User: 6 geometric dials
                                                Outputs: tune.json
                                        ↓
                                   ┌─────────┐
                                   │  labs   │  Apply tune.json to RAW
                                   └─────────┘  Outputs: styled.png
```

## Headless Tools

### tune binary

Automatically optimizes dials to match a reference style:

```bash
./tune source.ARW reference.png --save-area ./output [options]
```

| Option | Description |
|--------|-------------|
| `--save-area <dir>` | Output directory for tune.json (required) |
| `--threshold <value>` | Stop when spectral loss below (default: 0.005) |
| `--size <pixels>` | Working size (default: 1080) |
| `--mode <mode>` | blockwise, full35d, linear (default: blockwise) |
| `--skip-lut` | Skip 3D LUT estimation |
| `--logs` | Verbose progress (dome.r, edge.ratio) |
| `--fine` | Save intermediate images + meta.json (camera metadata) |
| `--fine-area <dir>` | Directory for --fine outputs (default: --save-area) |

### labs binary

Applies tune.json settings to a RAW file:

```bash
./labs source.ARW --output styled.png [options]
```

| Option | Description |
|--------|-------------|
| `--output <image.png>` | Output file (required) |
| `--tune <tune.json>` | Apply settings from tune (dials + 3D LUT) |
| `--size <pixels>` | Max output dimension (default: full res) |

**Example workflow:**
```bash
# Step 1: Tune to match a reference
./tune photo.ARW reference_style.png --save-area ./styles

# Step 2: Apply to same or other photos
./labs photo.ARW --tune ./styles/tune.json --output styled.png
./labs other.ARW --tune ./styles/tune.json --output other_styled.png
```

## Data Flow and Integration Patterns

The interaction between `raws` (the RAW decoder), `pipe` (the processing pipeline), `diff` (the difference calculator), and `tune` (the dial optimizer) follows distinct patterns.

### Pattern 1: Full Pipeline (`raws` → `pipe` → output)

```cpp
// 1. Decode RAW using raws component
mods::RawLoader loader;
cv::UMat linear_rgb = loader.process("input.ARW");

// 2. Grade with pipe (core processing pipeline)
pqtr::Pipe pipeline;
pqtr::Dials dials;
dials.fill(0.5f);  // Defaults for all 25 dials
cv::UMat display_rgb = pipeline.process(linear_rgb, dials);

// 3. Save output
cv::imwrite("output.png", display_rgb);
```

### Pattern 2: Style Transfer (`tune`)

Transfer the "vibe" from any reference image to your photos.

```cpp
// 1. Load source RAW and any reference image
cv::UMat source_linear = raws.process("my_photo.ARW");
cv::UMat reference = cv::imread("inspiring_photo.png", cv::IMREAD_COLOR);

// 2. Run tune (two-stage: SPSA for color/tone, Edge for sharpness)
pqtr::Tune tuner(pipeline, diff_tool);
pqtr::TuneResult result = tuner.optimize(source_linear, reference);

// Result contains:
// - link: 17 color/tone dials + 2 detail dials + 17³ LUT
// - Geometric dials NOT included (user responsibility)

// 3. Save as tune.json
data::link::toJson(result.link, "sunset/tune.json");

// 4. Apply to new photos via labs binary
// ./labs another_photo.ARW --tune sunset/tune.json --output styled.png
```

**The roles:**
- **Style** (45 dials): Automated via SPSA/ACEO/HYBRID + 12D spectral loss (~5min)
- **Geometry** (6 dials): User-controlled per image

### Pattern 3: Real-Time Dial Tuning (`pipe` + `diff`)

This pattern describes an interactive user experience, typically within a GUI application (like `PQTR:DESK`), where users adjust dials and see real-time feedback and metrics.

```cpp
// Interactive dial adjustment with live diff
cv::UMat reference_linear = raws.process("input.ARW");
cv::UMat target = cv::imread("reference_styled.png");

pqtr::Pipe pipeline;
pqtr::Diff differ;
pqtr::Dials dials;
dials.fill(0.5f);

// User adjusts dial via UI (e.g., in PQTR:DESK's UI)
void onDialChange(int dial_idx, float value) {
    dials[dial_idx] = value;

    cv::UMat candidate = pipeline.process(reference_linear, dials);
    float loss = differ.compute(candidate, target).total_loss;
    cv::UMat visual_diff = differ.visualDiff(candidate, target, 5.0);

    // Update display (e.g., in PQTR:DESK's UI)
    // cv::imshow("Candidate", candidate);
    // cv::imshow("Visual Diff", visual_diff);
    // printf("Loss: %.4f\n", loss);
}
```

## Performance Targets

The `LABS` system targets specific performance metrics to ensure efficient operation:

| Program | Target Performance | Hardware |
|---------|-------------------|----------|
| `raws`   | Existing (validated) | Any |
| `pipe`   | 30-60 fps @ 1080p | RTX 3060+, RX 6700+ |
| `diff` (spectral) | <5ms per comparison | GPU |
| `diff` (frequency) | <2ms per comparison | GPU |
| `tune` (full) | ~65 seconds | GPU |

**Full Pipeline (`raws` + `pipe`)**:
- `raws`: ~500ms (decode + demosaic)
- `pipe`: ~16ms @ 1080p (real-time)
- **Total**: ~520ms per image (acceptable for batch processing)

**`tune` Two-Stage Optimization**:

*Full Optimization (45 style dials)*
- 45 dials optimized via SPSA/ACEO/HYBRID
- 12D feature vector (spectral + color cast)
- ~60ms per iteration (2 pipe evaluations + spectral diff)
- Prior covariance from `etc/aceo_full.json`
- **Total**: ~5 minutes

**Total tune time**: ~5 minutes for complete style transfer (0.05% spectral loss)

## Notes and Considerations

### Color Space Alignment

*   **`raws` outputs Camera RGB** (white-balanced but not device-independent).
*   **`pipe` Module 1 (Color Correction) must transform to working RGB**: Camera RGB → XYZ → sRGB/ProPhoto using camera calibration matrices. This is **critical** for perceptual accuracy.
*   **Recommendation**: Module 1 should load camera profiles (JSON) with calibration matrices for common cameras.

### GPU Optimization

*   Both `raws` and `pipe` (and `diff`, `tune`) leverage `cv::UMat` for GPU processing. This allows automatic GPU offload when available and gracefully falls back to CPU if not, minimizing CPU↔GPU transfers.
*   **Pattern**: Keep all processing on GPU until final output.

### Preset Management

*   Dial presets are stored as JSON files.
*   Common presets: `neutral.json` (all 0.5), `vibrant.json`, `matte.json`, `teal_orange.json`.
*   `tune` can generate new presets.

---
# Build System (Make) 

This document outlines the `make`-based build system used within the `PQTR:LABS` project.

## Philosophy

The build system is designed around these core principles:

1.  **Convention over Configuration**: Each component (`part` or `program`) has its own standard `Makefile`.
2.  **Relative Paths**: Makefiles use relative paths to locate dependencies, making the project portable.
3.  **Out-of-Source Builds**: All build artifacts (object files, executables) are placed in a `tmp/` directory, keeping the source directories clean.
4.  **Static Linking**: Where possible, executables are statically linked to their library components to create self-contained binaries.

--- 

## Makefile Analysis: A Case Study

The `Makefile` for the `raws` tool (`LABS/opt/raws/src/main/Makefile`) serves as a canonical example for the project's build conventions.

### Full Makefile

```makefile
# Makefile for C++ RAW processing pipeline
# Uses production modules with custom GPL-free Sony decoder

# Compiler
CXX = g++
CXXFLAGS = -std=c++11 -Wall -O2

# OpenCV paths (relative to src/main/)
OPENCV_ROOT ?= ../../../lib/opencv
OPENCV_INCLUDE = -I$(OPENCV_ROOT)/build \
                 -I$(OPENCV_ROOT)/modules/core/include \
                 -I$(OPENCV_ROOT)/modules/imgproc/include \
                 -I$(OPENCV_ROOT)/modules/imgcodecs/include
OPENCV_LIB = $(OPENCV_ROOT)/build/lib

# Build output directory
TMP_DIR = ../../tmp
BUILD_DIR = $(TMP_DIR)/make
OBJ_DIR = $(BUILD_DIR)/obj

# Include and library flags
INCLUDES = $(OPENCV_INCLUDE) -I.
LDFLAGS = -L$(OPENCV_LIB)
LIBS = -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -Wl,-rpath,$(OPENCV_LIB)

# Source files (production modules)
SOURCES = main.cpp \
          sony_arw2.cpp \
          blc.cpp \
          wb_gain.cpp \
          demosaic.cpp \
          gamma_oetf.cpp

OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BUILD_DIR)/pipeline

# Default target
all: $(EXECUTABLE)

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Link
$(EXECUTABLE): $(OBJECTS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compile
$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -rf $(TMP_DIR)

# Phony targets
.PHONY: all clean

# Dependencies (headers)
$(OBJ_DIR)/main.o: sony_arw2.h blc.h wb_gain.h demosaic.h gamma_oetf.h module.h
$(OBJ_DIR)/sony_arw2.o: sony_arw2.h module.h
$(OBJ_DIR)/blc.o: blc.h sony_arw2.h module.h
$(OBJ_DIR)/wb_gain.o: wb_gain.h sony_arw2.h module.h
$(OBJ_DIR)/demosaic.o: demosaic.h sony_arw2.h module.h
$(OBJ_DIR)/gamma_oetf.o: gamma_oetf.h module.h
```

### Key Sections Explained

1.  **Compiler and Flags**:
    ```makefile
    CXX = g++
    CXXFLAGS = -std=c++11 -Wall -O2
    ```
    - `CXX`: Defines the C++ compiler.
    - `CXXFLAGS`: Sets compiler flags for C++11 standard, all warnings, and level 2 optimization.

2.  **Dependency Paths (OpenCV)**:
    ```makefile
    OPENCV_ROOT ?= ../../../lib/opencv
    OPENCV_INCLUDE = -I$(OPENCV_ROOT)/...
    OPENCV_LIB = $(OPENCV_ROOT)/build/lib
    ```
    - `OPENCV_ROOT`: The root path to the OpenCV submodule. The `?=` means it can be overridden from the command line.
    - `OPENCV_INCLUDE`: Specifies the include paths for OpenCV headers.
    - `OPENCV_LIB`: Specifies the path to the compiled OpenCV libraries.

3.  **Build Output Configuration**:
    ```makefile
    TMP_DIR = ../../tmp
    BUILD_DIR = $(TMP_DIR)/make
    OBJ_DIR = $(BUILD_DIR)/obj
    ```
    - All build artifacts are directed to a temporary directory outside the `src` folder.

4.  **Linker Configuration**:
    ```makefile
    INCLUDES = $(OPENCV_INCLUDE) -I.
    LDFLAGS = -L$(OPENCV_LIB)
    LIBS = -lopencv_core -lopencv_imgproc ... -Wl,-rpath,$(OPENCV_LIB)
    ```
    - `INCLUDES`: Directories to search for header files.
    - `LDFLAGS`: Tells the linker where to find libraries.
    - `LIBS`: Specifies which libraries to link against. The `-Wl,-rpath` flag embeds the library path in the executable, so `LD_LIBRARY_PATH` does not need to be set at runtime.

5.  **Source and Object Files**:
    ```makefile
    SOURCES = main.cpp sony_arw2.cpp ...
    OBJECTS = $(SOURCES:%.cpp=$(OBJ_DIR)/%.o)
    EXECUTABLE = $(BUILD_DIR)/pipeline
    ```
    - Defines the list of source files and uses a substitution to generate the list of corresponding object files that will be created in `$(OBJ_DIR)`.

6.  **Makefile Targets**:
    - `all`: The default target, which builds the final `$(EXECUTABLE)`.
    - `$(EXECUTABLE): $(OBJECTS) ...`: The linking rule. It links all the `$(OBJECTS)` together to create the final executable.
    - `$(OBJ_DIR)/%.o: %.cpp ...`: The compilation rule. It compiles a single `.cpp` source file into a `.o` object file.
    - `clean`: A utility target to remove all build artifacts.
    - **Header Dependencies**: The explicit dependencies at the end ensure that a source file is recompiled if any of its included headers change.
