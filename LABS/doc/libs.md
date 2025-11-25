# Library Interface

[back](../README.md)

## Overview

LABS exposes a single shared library (`labs.so`) with public headers. The raws decoder is compiled into `labs.so`. OpenCV is dynamically linked with an embedded rpath, so consumers only need to link against `labs.so`.

---

## Library

### labs.so

The monolithic shared library containing all LABS functionality.

| Aspect | Value |
|--------|-------|
| **Location** | `lib/labs.so` |
| **Size** | ~96KB |
| **Contents** | sony decoder, pipe modules |
| **OpenCV** | Dynamic link with embedded rpath |

**Build:**
```bash
make -f Makefile.libs
```

**Consumer linking:**
```makefile
LABS_DIR = ../LABS

# Headers
INCLUDES = -I$(LABS_DIR)/inc \
           -I$(LABS_DIR)/src/main/part/pipe

# Library
LDFLAGS  = -L$(LABS_DIR)/lib -Wl,-rpath,$(LABS_DIR)/lib
LIBS     = -llabs
```

---

## Public Headers

| Header | Location | Namespace | Purpose |
|--------|----------|-----------|---------|
| `pipe.hpp` | `inc/` | `pipe` | HEAD/TAIL abstraction (decode, save) |
| `mods.h` | `src/main/part/pipe/mods/` | `pipe::mods` | BODY processing modules (45 dials) |
| `hold.hpp` | `inc/` | `pqtr` | Owning smart pointer |
| `sink.hpp` | `inc/` | `pqtr` | Chunked buffer for I/O |
| `tool.hpp` | `inc/` | `pqtr` | File → Sink utilities |

### Planned

| Header | Namespace | Purpose |
|--------|-----------|---------|
| `data.hpp` | `data` | JSON I/O for pipe.json and style sidecars |
| `geos.hpp` | `geos` | Spectral optimization (SPSA, 35 color/tone dials) |
| `edge.hpp` | `edge` | Frequency optimization (greedy, 4 detail dials) |
| `diff.hpp` | `diff` | Loss metrics (spectral, frequency) |

### Internal (not public)

| Header | Purpose |
|--------|---------|
| `sony.h` | Sony ARW decoder (internal to pipe) |

---

## API Reference

### pqtr::Sink

Chunked buffer for data I/O.

```cpp
// Read file into sink
pqtr::Sink* sink = pqtr::Tool::read("image.ARW");

// Use with pipe
pipe::Head head;
pipe::open(*sink, pipe::decoder::SONY_ARW2, head);

delete sink;  // Caller owns
```

### pipe (HEAD/TAIL)

Abstraction for RAW decoding and output transforms.

```cpp
#include <pipe.hpp>

// Load RAW file
pqtr::Sink* sink = pqtr::Tool::read("image.ARW");

// HEAD: Decode to scene-linear RGB
pipe::Head head;
pipe::open(*sink, pipe::decoder::SONY_ARW2, head);
// head.view = CV_32FC3 scene-linear sRGB
// head.info = metadata map

delete sink;

// ... apply BODY processing via pipe::mods::* ...

// TAIL: Save (gamma applied internally)
pipe::save(head.view, "output.png");
```

### pipe::mods

Processing modules. All operate on `cv::UMat` (CV_32FC3).

```cpp
#include <mods/mods.h>

cv::UMat input, output;

// Exposure (dial: 0.0-1.0, 0.5 = neutral)
pipe::mods::exposure(input, output, 0.6f);

// Tone mapping (5 dials)
pipe::mods::tone_map(input, output,
    0.5f,   // contrast
    0.5f,   // highlights
    0.5f,   // shadows
    0.5f,   // white_point
    0.5f);  // black_point

// Geometric (6 dials)
pipe::mods::geometric(input, output,
    0.0f,   // crop_top
    0.0f,   // crop_right
    0.0f,   // crop_bottom
    0.0f,   // crop_left
    0.0f,   // zoom
    0.5f);  // tilt_angle
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

Dynamically linked with embedded rpath:
- `opencv_core`
- `opencv_imgproc`
- `opencv_imgcodecs`

Located at `lib/opencv/build/lib/`.

### raws

Sony decoder compiled directly into `labs.so` from `opt/raws/`.

---

## Consumer Example (DESK)

```makefile
# DESK Makefile additions
LABS_DIR = ../LABS

INCLUDES += -I$(LABS_DIR)/inc \
            -I$(LABS_DIR)/src/main/part/pipe

LDFLAGS  += -L$(LABS_DIR)/lib -Wl,-rpath,$(LABS_DIR)/lib
LIBS     += -llabs
```

```cpp
// DESK usage
#include <tool.hpp>
#include <pipe.hpp>
#include <mods/mods.h>

void processRaw(const std::string& rawPath) {
    // Load RAW
    pqtr::Sink* sink = pqtr::Tool::read(rawPath);

    // HEAD: Decode to scene-linear RGB
    pipe::Head head;
    pipe::open(*sink, pipe::decoder::SONY_ARW2, head);
    delete sink;

    // BODY: Apply processing modules
    cv::UMat processed;
    pipe::mods::tone_map(head.view, processed);

    // TAIL: Save (gamma applied internally)
    pipe::save(processed, "output.png");
}
```

---

## Build

```bash
# From LABS directory
make -f Makefile.libs
```

Output: `lib/labs.so` (~96KB)
