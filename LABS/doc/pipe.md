# PIPE

Processing pipeline for PQTR. Link-based architecture where projects contribute processing units.

## The Fidelity Rule

**No fidelity loss until POST.**

```
GEAR → LUTE → DRUM → VIBE → POST
 ↑       ↑       ↑       ↑      ↑
 │       │       │       │      └── 8-bit output here (lossy OK)
 └───────┴───────┴───────┴──────── Full precision (float32)
```

All processing maintains full floating-point precision. Quantization to 8-bit only happens at the final POST stage. This prevents accumulation of rounding errors through the pipeline.

| Stage | Precision | Fidelity |
|-------|-----------|----------|
| GEAR | float32 | Lossless (from RAW) |
| LUTE | float32 | Lossless |
| DRUM | float32 | Lossless |
| VIBE | float32 | Lossless |
| POST | uint8 | Lossy (final output) |

**Display vs Output**: `wgpu.view` may quantize for screen display, but this is temporary and not saved. Only `wgpu.post` writes the final 8-bit output.

## How PIPE Works

PIPE is a chain of Links. Data flows through each Link in sequence, transforming as it goes.

### Data Flow

```
                        ┌─────────────────────────────────────────────────────────┐
                        │                         PIPE                             │
                        │                                                          │
  RAW file ──►  Data ──►│──► Link ──► Link ──► Link ──► Link ──► Link ──►│──► Data ──► Output
               (in)     │    GEAR     WGPU     LUTE     VIBE     WGPU    │    (out)
                        │    decode   open     profile  style    shut    │
                        │                                                          │
                        │    Page:    Page:    Page:    Page:    Page:            │
                        │    raw buf  Bayer*   Ctx*     Ctx*     Ctx*    Output*  │
                        │                                                          │
                        │    Info accumulates metadata through each link ──────►  │
                        └─────────────────────────────────────────────────────────┘
```

### Core Concept

Each Link:
1. Receives `Data` (Page + Info)
2. Processes the Page (transforms buffer)
3. Enriches Info (adds metadata)
4. Returns new `Data`

```cpp
Data out = link.flow(in);  // Page transformed, Info accumulated
```

### Page vs Info

| Component | What it is | How it changes |
|-----------|------------|----------------|
| **Page** | Buffer pointer (`void*`) | Replaced at each Link (raw → Bayer → GPU → output) |
| **Info** | Metadata tree (Node) | Accumulated - each Link adds its metadata |

Info is a tree of key-value pairs (dials, text, arrays) that travels with the image through the pipeline.

## Model

```
Data = Page (buffer) + Info (metadata)
Link = processing unit with flow(Data) → Data
Pipe = chain of Links
```

**Page** flows through as buffer pointer (CPU or GPU).
**Info** accumulates metadata as tree structure.

## Core Links (RAW Processing)

Built-in links for the standard RAW processing pipeline. All run on GPU via WGSL compute shaders.

| Link | Info Params | Purpose |
|------|-------------|---------|
| `pipe::blc()` | `black_level`, `white_level` | Black level correction (normalize Bayer) |
| `pipe::wb()` | `wb_r`, `wb_g`, `wb_b`, `bayer_pattern` | White balance (per-channel gains) |
| `pipe::demosaic()` | `bayer_pattern` | Bayer → RGB (bilinear interpolation) |
| `pipe::cst()` | `color_matrix[9]` | Color space transform (3x3 matrix) |
| `pipe::crop()` | `crop_left`, `crop_top`, `crop_width`, `crop_height` | Active area extraction |

### Core Pipeline

```cpp
auto pipe = pipe::make();
pipe->link(gear::read());      // RAW → Bayer + Info
pipe->link(wgpu::open());      // CPU → GPU
pipe->link(pipe::blc());       // Black level correction
pipe->link(pipe::wb());        // White balance
pipe->link(pipe::demosaic());  // Bayer → RGB
pipe->link(pipe::cst());       // Color matrix
pipe->link(pipe::crop());      // Active area crop
pipe->link(wgpu::shut());      // GPU → CPU
```

GEAR populates Info with all parameters. Core links read from Info automatically.

## Project Link Contributions

| Project | Link | Input Page | Output Page |
|---------|------|------------|-------------|
| GEAR | `gear::read()` | raw file buffer | BayerBuffer* |
| WGPU | `wgpu::open()` | BayerBuffer* | Context* (GPU) |
| LUTE | `lute::tune()` | Context* | Context* (learns profile) |
| LUTE | `lute::view()` | Context* | Context* (applies profile) |
| DRUM | `drum::tune()` | Context* | Context* (learns DRO) |
| DRUM | `drum::view()` | Context* | Context* (applies DRO) |
| VIBE | `vibe::tune()` | Context* | Context* (learns style) |
| VIBE | `vibe::view()` | Context* | Context* (applies style) |
| WGPU | `wgpu::shut()` | Context* | OutputBuffer* |

## Pipe Configurations

### tune pipe - Learning

```cpp
auto tune = pipe::make();
tune->link(gear::read());    // raw → Bayer (CPU)
tune->link(wgpu::open());    // Bayer → GPU
tune->link(lute::tune());    // learn camera profile
tune->link(vibe::tune());    // learn style
tune->link(wgpu::shut());    // GPU → output (CPU)
```

Outputs learned parameters in Info.

### view pipe - Production

```cpp
auto view = pipe::make();
view->link(gear::read());    // raw → Bayer (CPU)
view->link(wgpu::open());    // Bayer → GPU
view->link(lute::view());    // apply camera profile
view->link(vibe::view());    // apply style
view->link(wgpu::shut());    // GPU → output (CPU)
```

Outputs processed pixels.

## API

### pipe.hpp

```cpp
namespace pipe {
    using Page = void*;       // Buffer pointer
    using Info = Node;        // Metadata tree
    using Name = std::string;

    class Data {
        Page page;
        Info info;
    };

    class Link {
        virtual Name name() const = 0;
        virtual Name type() const = 0;
        virtual Data flow(Data in) = 0;
    };

    class Pipe {
        virtual Pipe& link(Hold<Link> link) = 0;
        virtual Data flow(Data in) = 0;
        virtual Link* find(const Name& name) = 0;
    };

    Hold<Pipe> make();
}
```

### Info (Node) Methods

```cpp
info.dial("key", 1.5f);           // set float
float v = info.dial("key");       // get float

info.text("key", "value");        // set string
Name s = info.text("key");        // get string

info.data("key", floats, count);  // set array
const float* a = info.data("key");// get array

Node& child = info.make("child"); // create child node
Node* found = info.find("child"); // find child node

Name json = info.save();          // serialize
info.load(json);                  // deserialize
```

## Contributing a Link

```cpp
// myproject.hpp
namespace myproject {
    pipe::Hold<pipe::Link> link();
}

// myproject.cpp
class MyLink : public pipe::Link {
public:
    pipe::Name name() const override { return "myproject"; }
    pipe::Name type() const override { return "process"; }

    pipe::Data flow(pipe::Data in) override {
        // Get input
        auto* input = static_cast<InputType*>(in.page);

        // Process
        auto* output = process(input);

        // Return
        pipe::Data out;
        out.page = output;
        out.info = std::move(in.info);
        out.info.text("processed_by", "myproject");
        return out;
    }
};

pipe::Hold<pipe::Link> link() {
    return pipe::Hold<pipe::Link>(new MyLink());
}
```

## Structure

```
PIPE/
├── inc/pipe.hpp           # Public API
├── src/main/
│   ├── pipe.cpp           # Pipe implementation
│   └── link/              # Core processing links
│       ├── blc.cpp        # Black level correction
│       ├── wb.cpp         # White balance
│       ├── demosaic.cpp   # Bayer → RGB
│       ├── cst.cpp        # Color space transform
│       └── crop.cpp       # Active area crop
├── src/wgsl/              # GPU compute shaders
│   ├── blc.wgsl
│   ├── wb.wgsl
│   ├── demosaic.wgsl
│   ├── cst.wgsl
│   └── crop.wgsl
├── src/test/
│   └── pipe.cpp           # Tests
└── README.md
```

## Building

```bash
g++ -std=c++17 -I./inc src/main/pipe.cpp src/main/part/pipe/node.cpp -c
```

## pipe.json Format

The pipe configuration is stored as `<basename>.pipe.json` alongside the RAW file. This is the source of truth for the tune process.

### Structure

```json
{
  "info": {
    "file": "DSC01234.ARW",
    ...camera metadata after GEAR
  },
  "tune": {
    "step": ["gear", "lute", ...]
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `info.file` | string | Source RAW filename |
| `info.*` | various | Camera metadata (populated by GEAR) |
| `tune.step` | array | Completed tune steps |

### Lifecycle

**1. After Load RAW** - Initial creation:

```json
{
  "info": {
    "file": "DSC01234.ARW"
  },
  "tune": {
    "step": []
  }
}
```

**2. After GEAR step** - Camera metadata added:

```json
{
  "info": {
    "file": "DSC01234.ARW",
    "camera_model": "ILCE-7M3",
    "width": 6000,
    "height": 4000
  },
  "tune": {
    "step": ["gear"]
  }
}
```

**3. After LUTE step** - Camera profile learned:

```json
{
  "info": {
    "file": "DSC01234.ARW",
    "camera_model": "ILCE-7M3",
    "width": 6000,
    "height": 4000
  },
  "tune": {
    "step": ["gear", "lute"],
    "lute": {
      ...learned camera profile parameters
    }
  }
}
```

**4. After VIBE step** - Style learned:

```json
{
  "info": { ... },
  "tune": {
    "step": ["gear", "lute", "vibe"],
    "lute": { ... },
    "vibe": {
      ...51 dial values
    }
  }
}
```

### Sidecar Files

All sidecars follow `<basename>.<type>` naming:

| File | Content |
|------|---------|
| `<basename>.ARW` | Source RAW file |
| `<basename>.pipe.json` | Pipe configuration (source of truth) |
| `<basename>.png` | Camera preview (tune target) |

### Re-running

The pipe can be re-run at any time. The `tune.step` array tracks what's been done, but there's no status - steps are simply present or not. Running tune again will re-execute all steps.

## See Also

- [GEAR](gear.md) - `gear::read()` RAW decoder
- [WGPU](wgpu.md) - `wgpu::open()/shut()` GPU lifecycle
- [LUTE](lute.md) - `lute::tune()/view()` camera profiles
- [VIBE](vibe.md) - `vibe::tune()/view()` style processing
