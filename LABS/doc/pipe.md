# PIPE

Processing pipeline for PQTR. Link-based architecture where projects contribute processing units.

## The Fidelity Rule

**No fidelity loss until POST.**

```
HEAD → LUTE → DRUM → VIBE → POST
 ↑       ↑       ↑       ↑      ↑
 │       │       │       │      └── 8-bit output here (lossy OK)
 └───────┴───────┴───────┴──────── Full precision (float32)
```

All processing maintains full floating-point precision. Quantization to 8-bit only happens at the final POST stage.

| Stage | Precision | Purpose |
|-------|-----------|---------|
| HEAD | float32 | RAW decode to linear RGB |
| LUTE | float32 | Camera profile |
| DRUM | float32 | Dynamic range |
| VIBE | float32 | Style processing |
| POST | uint8 | Final output |

## How PIPE Works

PIPE is a chain of Links. Data flows through each Link in sequence.

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
| **Page** | Buffer pointer (`void*`) | Replaced at each Link |
| **Info** | Metadata tree (Node) | Accumulated - each Link adds its metadata |

## Execution Flows

The pipe has two execution flows:

### tune flow - Learning

```
RAW ──► HEAD ──► LUTE.tune ──► DRUM.tune ──► VIBE.tune ──► POST
                    │              │              │
                    ▼              ▼              ▼
              learns from    learns from    learns from
              camera JPEG    camera JPEG    camera JPEG
```

Phase 1: LUTE and DRUM process on the on-camera JPEG
Phase 2: Default VIBE optimisation to get the 0 vibe settings

### exec flow - Production

```
RAW ──► HEAD ──► LUTE.exec ──► DRUM.exec ──► VIBE.exec ──► POST
                    │              │              │
                    ▼              ▼              ▼
               applies         applies        applies
               profile         DRO            style
```

Uses tuned parameters from pipe.json body.

## pipe.json Format

The pipe configuration stored as `<basename>.pipe.json`. Source of truth for the tune process.

### Structure

```json
{
  "pipe": "1.0",
  "head": {
    "file": "DSC01234.ARW",
    "gear_model": "ILCE-7M3",
    "width": 6000,
    "height": 4000
  },
  "body": {
    "lute": {},
    "drum": {},
    "vibe": [
      {}
    ]
  },
  "tail": ["head", "lute", "drum", "vibe", "diff"],
  "vibe-list": [
    {
      "file": "DSC01234.ARW",
      "name": "Beach sunset",
      "find": "golden hour waves"
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `pipe` | string | Version identifier |
| `head` | object | Camera info from GEAR |
| `body.lute` | object | Camera profile parameters |
| `body.drum` | object | Dynamic range parameters |
| `body.vibe` | array | Vibe settings (index 0 = default) |
| `tail` | array | Stage outputs to produce (head, lute, drum, vibe, diff) |
| `vibe-list` | array | Source files with metadata |

**Note:** `<basename>.png` is always produced. `tail` controls which intermediate stage outputs are saved.

### Vibe List

Multiple images can be added for comparison:

```json
"vibe-list": [
  {
    "file": "DSC01234.ARW",
    "name": "Beach sunset",
    "find": "golden hour waves"
  },
  {
    "file": "DSC01235.ARW",
    "name": "Beach portrait",
    "find": "golden hour person"
  }
]
```

The default vibe (index 0) comes from tuning against the on-camera JPEG.

### Sidecar Files

```
<basename>.ARW           # Source RAW
<basename>.pipe.json     # Pipe configuration
<basename>.0.jpg         # On-camera JPEG (tune target)
<basename>.png           # Final output (always)
```

Stage outputs (if in tail array):
```
<basename>.head.png      # "head" - after HEAD stage
<basename>.lute.png      # "lute" - after LUTE stage
<basename>.drum.png      # "drum" - after DRUM stage
<basename>.vibe.0.png    # "vibe" - after VIBE stage (default vibe)
<basename>.diff.png      # "diff" - difference vs .0.jpg
```

## API

### pipe.hpp

```cpp
namespace pipe {
    using Page = void*;
    using Info = Node;
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

Node& child = info.make("child"); // create child node
Node* found = info.find("child"); // find child node

Name json = info.save();          // serialize
info.load(json);                  // deserialize
```

## Link Contributions

| Project | Link | Purpose |
|---------|------|---------|
| GEAR | `gear::read()` | RAW → Bayer + Info |
| HEAD | `head::flow()` | Bayer → Linear RGB (blc, wb, demosaic, cst, crop) |
| LUTE | `lute::tune()` | Learn camera profile |
| LUTE | `lute::exec()` | Apply camera profile |
| DRUM | `drum::tune()` | Learn dynamic range |
| DRUM | `drum::exec()` | Apply dynamic range |
| VIBE | `vibe::tune()` | Learn style |
| VIBE | `vibe::exec()` | Apply style |

## Rules

1. Everything is a Link - no processing code outside Links
2. Tune Links write results to Info
3. Exec Links read parameters from Info
4. pipe.json is the source of truth
5. Full precision until POST

## WASI Build (pipe.wasm)

Headless WASM module for server-side processing via wasmtime.

### Output

```
tmp/pack/lib/pipe.wasm
```

### Sources (processing only, no UI)

```
src/main/gear/*.cpp
src/main/lute/*.cpp
src/main/pipe/*.cpp
src/wasi/main.cpp       # WASI entry point (to create)
```

### WASI Entry Point

```cpp
// src/wasi/main.cpp
#include "gear.hpp"
#include "pipe.hpp"

int main(int argc, char** argv) {
    // Read RAW from stdin or file arg
    // Run GEAR decode
    // Run PIPE processing
    // Write result to stdout
    return 0;
}
```

### Makefile Target

```make
WASI_SDK = /opt/wasi-sdk
WASI_CC = $(WASI_SDK)/bin/clang++
WASI_FLAGS = -std=c++17 -O2 --target=wasm32-wasi

PIPE_SRCS = $(shell find src/main -name '*.cpp' ! -path '*/labs/*' ! -path '*/wgpu/*')
PIPE_SRCS += src/wasi/main.cpp

wasi: tmp/pack/lib/pipe.wasm

tmp/pack/lib/pipe.wasm: $(PIPE_SRCS)
	@mkdir -p tmp/pack/lib
	$(WASI_CC) $(WASI_FLAGS) -Iinc -Iinc/gear -Iinc/pipe $(PIPE_SRCS) -o $@
```

### Install wasi-sdk

```bash
cd /opt
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-21/wasi-sdk-21.0-linux.tar.gz
tar xf wasi-sdk-21.0-linux.tar.gz
ln -s wasi-sdk-21.0 wasi-sdk
```

### BASE Integration

```cpp
wasmtime::Engine engine;
wasmtime::Module module = Module::from_file(engine, "lib/pipe.wasm");

void handle_pipe_request(Request& req) {
    auto instance = Instance::create(engine, module);
    // Pass RAW data via WASI stdin or memory
    // Run _start
    // Read result from WASI stdout or memory
}
```

### Architecture

```
Browser                         Server (BASE)
   │                               │
   └─ labs.wasm (GUI)              ├─ wasmtime + pipe.wasm
       └─ fetch ──────────────────►│
           "process DSC001.ARW"    └─ GEAR → PIPE → result
                                       └─ store to pipe dir
```
