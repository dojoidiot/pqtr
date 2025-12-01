# PQTR:LABS

[back](../README.md)

The "settled science" of RAW image processing. LABS is a stable utility library that powers all PQTR applications.

## Core Insight

**Photographers select images based on what they see—the camera's styled preview.**

They compose, expose, and choose keepers while looking at the LCD with picture style applied. The camera JPEG isn't "a reference"—it's the photographer's intent. Edits are adjustments TO that baseline, not FROM flat RAW.

This drives the architecture: **Style first, then tweaks.** See [tldr.md](./doc/tldr.md).

## Role in PQTR

LABS sits between raw decoding and end-user applications:

```
[RAWS] ──► [LABS] ──► DESK / FAST / PLAY
            │
            └── pipe::Pipe API
                HEAD → BODY → TAIL
```

- **Consumes**: `RAWS.a` (RAW decoder, via symlink)
- **Produces**: `labs.a` (complete processing library)
- **Used by**: DESK, FAST, PLAY

LABS knows nothing about camera-specific decoding—it calls `raws::decode()` and receives scene-linear RGB. When new cameras are added to RAWS, LABS works unchanged.

### Separation of Concerns: RAWS vs TUNE

**RAWS extracts. TUNE transforms.**

| RAWS (upstream) | TUNE (in LABS) |
|-----------------|----------------|
| Canonical extraction | Style optimization |
| Scene-referred linear RGB | Display-referred output |
| Camera-specific decoding | Camera-agnostic transforms |
| No style decisions | All style decisions |

**The camera JPEG is just one style.** TUNE finds transforms to match any reference—camera JPEG, film emulation, or custom look. RAWS provides neutral, canonical data that TUNE can shape into any style.

**Validation:** TUNE error rates prove RAWS correctness. If TUNE achieves low error, the extraction is working. Don't judge RAWS by visual appearance—scene-linear data looks flat before style transforms.

## Project Structure

```
LABS/
├── bin/              # Compiled binaries (tune, labs)
├── inc/              # Public headers
│   ├── pipe.hpp      # Pipe API
│   ├── geos.hpp      # Optimizer API
│   ├── data.hpp      # Serialization API
│   ├── sink.hpp      # Data buffer
│   ├── hold.hpp      # Smart pointer
│   ├── tool.hpp      # File utilities
│   └── RAWS/         # [symlink] → RAWS/inc
├── lib/
│   ├── labs.a        # Main library (includes RAWS)
│   ├── RAWS.a        # [symlink] → RAWS/lib/RAWS.a
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
├── Makefile.labs             # Builds lib/labs.a
├── Makefile.tune             # Builds bin/tune, bin/labs + geos tests
└── Makefile.mods             # Module unit tests
```

## Building

```bash
make              # Build lib/labs.a (default)
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

## Headless Tools (Programs)

Command-line executables in `bin/`:

*   **`tune`**: Automatically optimizes 45 style dials to match a reference style. Supports SPSA, ACEO, and HYBRID optimizers. Optional 17³ 3D LUT for nonlinear color transforms.
*   **`labs`**: Processes RAW to PNG with optional tune.json settings (dials + 3D LUT).

## RAW Decoding

RAW decoding is handled by [RAWS](../RAWS/README.md), a separate project that produces `RAWS.a`. LABS links this library and calls `raws::decode()` to obtain scene-linear RGB.

This separation means:
- Camera support R&D happens in RAWS, not LABS
- Adding Sony/Canon/Nikon support doesn't change LABS code
- LABS remains stable while RAWS evolves

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

**Current results (2024-12-01)**: Most images achieve <5% final loss with base curve + 3D LUT. RAWS estimates per-channel curves (768 floats) from RAW→preview comparison. Baseline guard ensures optimizer never degrades quality.

**Remaining gap analysis**: The 5% residual comes from per-channel curves shifting hue (50%), 3D LUT resolution limits (30%), DRO spatial variation (15%), and alignment/matrix precision (5%). Quick wins: neutral-pixel curve estimation and luminance-preserving tone mapping. See [todo.md](./doc/todo.md#strategic-analysis-path-to-camera-parity-2024-12-01) for full analysis.

**Direct LUT experiment (2024-12-01)**: Tested bypassing base curve + dials with a single 33³ LUT measured directly from flat→JPEG. Result: worse than current pipeline (7% vs 5% on DSC00144) because 96% of LUT cells are empty - scene-linear data clusters in low value range. The two-phase architecture (base curve → dials → small LUT) is more efficient.

## Documentation Standards

All LABS documentation maintains:

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
