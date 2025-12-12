# PQTR:PIPE

[back](../README.md)

The "settled science" of RAW image processing. PIPE is a stable utility library that powers all PQTR applications.

## Core Insight

**Photographers select images based on what they see—the camera's styled preview.**

They compose, expose, and choose keepers while looking at the LCD with picture style applied. The camera JPEG isn't "a reference"—it's the photographer's intent. Edits are adjustments TO that baseline, not FROM flat RAW.

This drives the architecture: **Style first, then tweaks.** See [tldr.md](./doc/tldr.md).

## Role in PQTR

PIPE sits between raw decoding and end-user applications:

```
[GEAR] ──► [PIPE] ──► DESK / FAST / PLAY
            │
            └── pipe::Pipe API
                HEAD → BODY → TAIL
```

- **Consumes**: `GEAR.a` (RAW decoder, via symlink)
- **Produces**: `pipe.a` (complete processing library)
- **Used by**: DESK, FAST, PLAY

PIPE knows nothing about camera-specific decoding—it calls `gear::decode()` and receives scene-linear RGB. When new cameras are added to GEAR, PIPE works unchanged.

### Separation of Concerns: GEAR vs TUNE

**GEAR extracts. TUNE transforms.**

| GEAR (upstream) | TUNE (in PIPE) |
|-----------------|----------------|
| Canonical extraction | Style optimization |
| Scene-referred linear RGB | Display-referred output |
| Camera-specific decoding | Camera-agnostic transforms |
| No style decisions | All style decisions |

**The camera JPEG is just one style.** TUNE finds transforms to match any reference—camera JPEG, film emulation, or custom look. GEAR provides neutral, canonical data that TUNE can shape into any style.

**Validation:** TUNE error rates prove GEAR correctness. If TUNE achieves low error, the extraction is working. Don't judge GEAR by visual appearance—scene-linear data looks flat before style transforms.

## Project Structure

```
PIPE/
├── bin/              # Compiled binaries (tune, labs)
├── inc/              # Public headers
│   ├── pipe.hpp      # Pipe API
│   ├── geos.hpp      # Optimizer API
│   ├── data.hpp      # Serialization API
│   ├── sink.hpp      # Data buffer
│   ├── hold.hpp      # Smart pointer
│   ├── tool.hpp      # File utilities
│   └── GEAR/         # [symlink] → GEAR/inc
├── lib/
│   ├── pipe.a        # Main library (includes GEAR)
│   ├── GEAR.a        # [symlink] → GEAR/lib/GEAR.a
│   └── opencv/       # OpenCV dependency
├── src/
│   ├── main/
│   │   ├── tune.cpp          # bin/tune source
│   │   ├── labs.cpp          # bin/labs source
│   │   └── part/
│   │       ├── pipe/         # Pipe part (HEAD/BODY/TAIL)
│   │       │   └── mods/     # Processing modules
│   │       └── geos/         # Optimizer part (SPSA + Edge)
│   └── test/
│       ├── mods/             # Module unit tests
│       └── geos/             # Optimizer tests
├── tmp/
│   ├── obj/<name>/           # Build objects (labs, mods, tune)
│   ├── bin/<name>/           # Test binaries
│   └── var/<name>/           # Test output
├── Makefile                  # Top-level (delegates)
├── Makefile.labs             # Builds lib/pipe.a
├── Makefile.tune             # Builds bin/tune, bin/labs + geos tests
└── Makefile.mods             # Module unit tests
```

## Building

```bash
make              # Build lib/pipe.a (default)
make tune         # Build bin/tune, bin/labs + tests
make test         # Run quick tests (mods + tune-fast)
make test-all     # Run full test suite
make test-labs    # Apply tune.json to test RAW
make test-batch   # Tune all pics, create comparison grid
make all          # Build everything
make clean        # Clean all artifacts
```

## Pipeline Philosophy

*   **Scope**: The pipeline's scope is from **camera to web**. It begins with a RAW file from a camera and ends with a PNG image suitable for web distribution (PNG is chosen over JPEG because it is lossless, avoiding compression artifacts during tuning and development).
*   **Canonical Processing**: The pipeline is designed to be "canonical," meaning it focuses strictly on the mathematical methods required to process the image.
*   **Edit Step Architecture**: The pipe processes images through a sequence of **edit steps**. Each edit step contains all 6 available modules, but only enables the modules relevant to its purpose.

## Components (Parts)

Parts are modular libraries in `src/main/part/`. They expose a public API using the PIMPL idiom in `inc/` headers.

*   [**`pipe`**](./doc/pipe.md): The core image processing pipeline.
    *   **HEAD**: Decodes the RAW file into scene-referred linear space.
    *   **BODY**: A sequence of configurable **edit steps** with 6 modules each.
    *   **TAIL**: Renders the final image to **PNG** (lossless format).
*   [**`geos`**](./doc/geos.md): The optimizer library. 45 style dials, 23D feature space, weighted L2 loss, three optimizer modes (SPSA, ACEO, HYBRID). See [tldr.md](./doc/tldr.md) for overview.

## Workflow

```bash
# 1. Optimize dials to match a reference
tune photo.ARW reference.jpg --save-area output/

# 2. Apply the vibe to produce final image
labs photo.ARW --tune output/tune.json --output photo.png

# 3. Debug: see pipeline stages
labs photo.ARW --tune output/tune.json --output photo.png --debug
```

Debug outputs (alongside photo.png):
- `photo_0_flat.png` - Scene-linear RAW data (before any processing)
- `photo_0_preview.png` - Camera's embedded JPEG preview
- `photo_1_body.png` - After all processing links applied

## Tools

*   **`tune`**: Optimizes 45 style dials to match a reference. Outputs `tune.json`.
*   **`labs`**: Applies tune settings to RAW, outputs PNG. Use `--debug` to save intermediate stages.

## RAW Decoding

RAW decoding is handled by [GEAR](../GEAR/README.md), a separate project that produces `GEAR.a`. PIPE links this library and calls `gear::decode()` to obtain scene-linear RGB.

This separation means:
- Camera support R&D happens in GEAR, not PIPE
- Adding Sony/Canon/Nikon support doesn't change PIPE code
- PIPE remains stable while GEAR evolves

## Out of Scope

See [out_of_scope.md](./doc/mods/out_of_scope.md) for detailed module ideas.

*   **Alternative Delivery Formats**: Primary output is PNG (lossless).
*   **User Interface (UI) Conventions**: Features for interactive UI applications.
*   **Advanced/Future Modules**: Noise reduction, lens correction profiles.

## Success Criteria

1.  **RAW to PNG Handoff Works Seamlessly**: Linear RGB validated, no data loss.
2.  **Pipe Produces Perceptually Accurate Output**: All 45 dials functional.
3.  **Diff Provides Accurate Loss Metrics**: 23D weighted L2 loss + frequency (Laplacian).
4.  **Tune Optimizes All Roles**: 45 dials via SPSA/ACEO/HYBRID; optional 3D LUT.
5.  **Performance Targets Met**: `tune`: ~65 seconds total.

**Current results (2024-12-01)**: Most images achieve <5% final loss with base curve + 3D LUT. GEAR estimates per-channel curves (768 floats) from RAW→preview comparison. Baseline guard ensures optimizer never degrades quality.

**Remaining gap analysis**: The 5% residual comes from per-channel curves shifting hue (50%), 3D LUT resolution limits (30%), DRO spatial variation (15%), and alignment/matrix precision (5%). Quick wins: neutral-pixel curve estimation and luminance-preserving tone mapping. See [todo.md](./doc/todo.md#strategic-analysis-path-to-camera-parity-2024-12-01) for full analysis.

**Direct LUT experiment (2024-12-01)**: Tested bypassing base curve + dials with a single 33³ LUT measured directly from flat→JPEG. Result: worse than current pipeline (7% vs 5% on DSC00144) because 96% of LUT cells are empty - scene-linear data clusters in low value range. The two-phase architecture (base curve → dials → small LUT) is more efficient.

## Documentation Standards

All PIPE documentation maintains:

- **Voice**: Declarative present tense
- **Tense**: Present for current, conditional future for out-of-scope
- **Terminology**: Consistent "edit step", "module", "dial", "camera to web"
- **Technical Accuracy**: Code examples and counts match implementation

Reference [docs.md](./doc/docs.md) for the review template and criteria.

## Documentation

- [tldr.md](./doc/tldr.md) - Quick overview (45 dials, 23D features, 3 optimizers)
- [todo.md](./doc/todo.md) - Current status and next steps
- [code.md](./doc/code.md) - Code management (building, testing, tmp/ conventions)
- [geos.md](./doc/geos.md) - GeoS model (23D feature space, weighted L2 loss)
- [aceo.md](./doc/aceo.md) - ACEO optimizer (eigenspace, covariance)
- [spsa.md](./doc/spsa.md) - SPSA optimizer (phased, gradient-free)
- [base_curve.md](./doc/base_curve.md) - Per-image base curve estimation

## Data Files

- `etc/jacob.json` - Jacobian matrix (45×23 dial→feature sensitivity)
- `etc/aceo_full.json` - Covariance prior (45×45 dial correlations)

## Research

- [analysis.md](./doc/analysis.md) - Empirical findings, batch results, LUT covariance problem
- [idea.md](./doc/idea.md) - Future enhancement ideas
