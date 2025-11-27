# Library Interface

[back](../README.md)

## Overview

LABS exposes a static library (`labs.a`) with public headers. The RAWS decoder is compiled into `labs.a`. OpenCV is dynamically linked at runtime, so consumers link against `labs.a` and set `LD_LIBRARY_PATH` for OpenCV.

---

## Library

### labs.a

The static library containing all LABS functionality.

| Aspect | Value |
|--------|-------|
| **Location** | `lib/labs.a` |
| **Contents** | RAWS decoder, pipe modules, tune optimizer |
| **OpenCV** | Runtime link via `LD_LIBRARY_PATH` |

**Build:**
```bash
make -f Makefile.labs
```

**Consumer linking:**
```makefile
LABS_DIR = ../LABS

# Headers
INCLUDES = -I$(LABS_DIR)/inc \
           -I$(LABS_DIR)/src/main/part/pipe

# Library (static)
LIBS = $(LABS_DIR)/lib/labs.a

# OpenCV (runtime)
# LD_LIBRARY_PATH=$(LABS_DIR)/lib/opencv/build/lib ./your_app
```

---

## Public Headers

| Header | Location | Namespace | Purpose |
|--------|----------|-----------|---------|
| `pipe.hpp` | `inc/` | `pipe` | HEAD/BODY/TAIL pipeline (PIMPL builder) |
| `tune.hpp` | `inc/` | `tune` | Loss metrics + optimization (PIMPL) |
| `data.hpp` | `inc/` | `tune` | Sidecar serialization (Data ↔ JSON) |
| `hold.hpp` | `inc/` | `pqtr` | Owning smart pointer |
| `sink.hpp` | `inc/` | `pqtr` | Chunked buffer for I/O |
| `tool.hpp` | `inc/` | `pqtr` | File → Sink utilities |
| `mods.h` | `src/main/part/pipe/mods/` | `mods` | Low-level processing kernels |

### Internal (not public)

| Header | Location | Purpose |
|--------|----------|---------|
| `view.hpp` | `src/main/part/pipe/` | Display conversion (linear → sRGB) |
| `link.hpp` | `src/main/part/pipe/` | Module implementations + LinkImpl |
| `diff.hpp` | `src/main/part/tune/` | Loss metric helpers |
| `geos.hpp` | `src/main/part/tune/` | SPSA optimizer internals |
| `edge.hpp` | `src/main/part/tune/` | Golden section internals |
| `sony.h` | `inc/RAWS/` | Sony ARW decoder |

---

## API Reference

### pqtr::Sink

Chunked buffer for data I/O.

```cpp
#include <tool.hpp>

// Read file into sink (caller owns via Hold)
pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read("image.ARW"));
```

### pipe (HEAD → BODY → TAIL)

PIMPL builder pattern for RAW processing.

```cpp
#include <pipe.hpp>
#include <tool.hpp>

// Load RAW file
pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read("image.ARW"));

// Create pipe and decode (decoder auto-detected)
pqtr::Hold<pipe::Pipe> pipe = pipe::make();
pqtr::Hold<pipe::Head> head = pipe->open(std::move(sink));

// HEAD: Access decoded data
pipe::Data& data = head->data();
pipe::View linear = data.view();  // CV_32FC3 scene-linear

// BODY: Create Links with 6 golden modules (45 dials)
pipe::Body& body = head->body(1024);  // Work at 1024px
pipe::Body::Link& link = body.add("style");

// Set dials (activates modules)
link.colorCorrection().exposure().set(0.6f);
link.toneMapping().contrast().set(0.55f);
link.globalColor().vibrance().set(0.6f);

// Get display-ready view (8-bit BGR, gamma encoded)
pipe::View display = body.view();

// TAIL: Export at full resolution
body.tail().save("output.png", 1080);
```

### tune (diff + optimization)

PIMPL for loss measurement and style optimization.

```cpp
#include <tune.hpp>
#include <pipe.hpp>

// Create tune task with target image
cv::UMat target = cv::imread("camera_jpeg.jpg");
pqtr::Hold<tune::Task> task = tune::make(target);

// Measure loss (no optimization)
tune::Data loss = task->diff(body.view());
// loss.spectral: [0,1] color/tone gap
// loss.frequency: [0,∞) sharpness gap

// Run optimization with progress callback
tune::Result result = task->run(body, link, tune::Config(),
    [](const tune::Progress& p) {
        if (p.stage == tune::Progress::Stage::GEOS)
            std::cout << "GEOS: " << p.dome.r << std::endl;
        return true;  // false to abort
    });
```

### mods (low-level kernels)

Direct access to processing modules. All operate on `cv::UMat` (CV_32FC3).

```cpp
#include <mods/mods.h>

cv::UMat input, output;

// Exposure (dial: 0.0-1.0, 0.5 = neutral)
mods::exposure(input, output, 0.6f);

// Tone mapping (5 dials)
mods::tone_map(input, output,
    0.5f,   // contrast
    0.5f,   // highlights
    0.5f,   // shadows
    0.5f,   // white_point
    0.5f);  // black_point
```

### Module Summary

| Function | Dials | Default |
|----------|-------|---------|
| `geometric()` | 6 | crop=0, zoom=0, tilt=0.5 |
| `exposure()` | 1 | 0.5 |
| `white_balance()` | 2 | temp=0.5, tint=0.5 |
| `tone_map()` | 5 | all 0.5 |
| `global_color()` | 3 | all 0.5 |
| `selective_color()` | 24 | all 0.5 |
| `detail()` | 4 | sharpen=0.6/0.4, denoise=0.3/0.5 |

**Total: 45 dials**

---

## Dependencies

### OpenCV

Runtime linked via `LD_LIBRARY_PATH`:
- `opencv_core`
- `opencv_imgproc`
- `opencv_imgcodecs`

Located at `lib/opencv/build/lib/`.

### RAWS

Sony decoder compiled into `labs.a` from `lib/RAWS.a`.

---

## Consumer Example (DESK)

```makefile
# DESK Makefile
LABS_DIR = ../LABS

INCLUDES = -I$(LABS_DIR)/inc \
           -I$(LABS_DIR)/src/main/part/pipe

LIBS = $(LABS_DIR)/lib/labs.a \
       $(LABS_DIR)/lib/opencv/build/lib/libopencv_core.so \
       $(LABS_DIR)/lib/opencv/build/lib/libopencv_imgproc.so \
       $(LABS_DIR)/lib/opencv/build/lib/libopencv_imgcodecs.so
```

```cpp
// DESK usage
#include <tool.hpp>
#include <pipe.hpp>

void processRaw(const std::string& rawPath) {
    // Load RAW
    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(rawPath));

    // HEAD → BODY → TAIL
    pqtr::Hold<pipe::Pipe> pipe = pipe::make();
    pqtr::Hold<pipe::Head> head = pipe->open(std::move(sink));
    pipe::Body& body = head->body(1024);

    // Configure link
    pipe::Body::Link& link = body.add("style");
    link.colorCorrection().exposure().set(0.6f);
    link.toneMapping().contrast().set(0.55f);

    // Export
    body.tail().save("output.png", 1080);
}
```

---

## Source Structure

```
src/main/part/
├── pipe/
│   ├── pipe.cpp      # HEAD/BODY/TAIL/Pipe
│   ├── view.cpp      # Display conversion (linear → sRGB)
│   ├── link.cpp      # Module implementations
│   └── mods/         # Processing kernels (45 dials)
└── tune/
    ├── tune.cpp      # Task + factory
    ├── diff.cpp      # Loss metrics
    ├── geos.cpp      # SPSA optimizer (stub)
    ├── edge.cpp      # Golden section (stub)
    └── data.cpp      # Serialization
```

---

## Build

```bash
# From LABS directory
make -f Makefile.labs
```

Output: `lib/labs.a`
