# PQTR:LABS

[back](../README.md)

The "settled science" of RAW image processing. LABS is a stable utility library that powers all PQTR applications.

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
*   [**`geos`**](./doc/geos.md): The optimizer library. Handles **color/tone** (17 dials + 17³ LUT, SPSA + spectral) and **sharpness** (2 dials, golden section + frequency).

## Headless Tools (Programs)

Command-line executables in `bin/`:

*   **`tune`**: Automatically optimizes dials to match a reference style. Two-stage: SPSA for color/tone (17 dials + 17³ LUT), golden section for sharpness (2 dials).
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
2.  **Pipe Produces Perceptually Accurate Output**: All dials functional.
3.  **Diff Provides Accurate Loss Metrics**: Spectral (geodesic) + frequency (Laplacian). **Achieved: 0.05% spectral loss.**
4.  **Tune Optimizes All Roles**: SPSA + LUT for color/tone; golden section for sharpness.
5.  **Performance Targets Met**: `tune`: ~65 seconds total.

## Documentation Standards

All LABS documentation maintains:

- **Voice**: Declarative present tense
- **Tense**: Present for current, conditional future for out-of-scope
- **Terminology**: Consistent "edit step", "module", "dial", "camera to web"
- **Technical Accuracy**: Code examples and counts match implementation

Reference [docs.md](./doc/docs.md) for the review template and criteria.

## Research

- [analysis.md](./doc/analysis.md) - Empirical findings, batch results, LUT covariance problem
- [idea.md](./doc/idea.md) - Future enhancement ideas
