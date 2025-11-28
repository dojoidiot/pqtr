# PQTR:LABS

[back](../README.md)

The "settled science" of RAW image processing. LABS is a stable utility library that powers all PQTR applications.

## Role in PQTR

LABS sits between raw decoding and end-user applications:

```
[RAWS] ──► [LABS HEAD] ──► [LABS BODY] ──► DESK / FAST / PLAY
   │            │               │
   │            │               └── pipe::Pipe API (dials, modules)
   │            │
   │            └── Applies color science:
   │                • WB (from metadata)
   │                • ColorMatrix (from metadata)
   │
   └── Provides:
       • Camera-native RGB
       • ColorMeta (WB, matrix, distortion)
```

- **Consumes**: `raws.a` (RAW decoder, via symlink)
- **Produces**: `labs.a` (complete processing library)
- **Used by**: DESK, FAST, PLAY

## Separation of Concerns

| Component | Responsibility | Camera-Specific? |
|-----------|----------------|------------------|
| **RAWS** | Sensor data extraction (BLC, demosaic, crop) | Yes |
| **LABS HEAD** | Color science (WB, matrix) from metadata | No |
| **LABS BODY** | Image processing (dials, modules, tune) | No |

**Why this matters:**
- RAWS decoder outputs camera-native RGB + metadata
- LABS applies WB/matrix using metadata—same code for all cameras
- Tune optimizer learns *actual* camera transform, not "correction to decoder's guess"
- Adding new cameras requires only a RAWS decoder; LABS code unchanged

## Project Structure

```
LABS/
├── bin/          # Compiled binaries (pipe, tune, diff)
├── inc/          # Public headers (pipe.hpp, sink.hpp, hold.hpp, tool.hpp)
│   └── RAWS/     # [symlink] → RAWS/inc (raws.hpp API)
├── lib/          # Compiled libraries
│   ├── labs.a    # Main library (includes RAWS)
│   ├── RAWS.a    # [symlink] → RAWS/lib/raws.a
│   └── opencv/   # OpenCV dependency
├── src/
│   ├── main/     # Source code for binaries
│   └── main/part # Source code for library components
└── doc/          # Documentation
```

## Pipeline Philosophy

*   **Scope**: The pipeline's scope is from **camera to web**. It begins with a RAW file from a camera and ends with a PNG image suitable for web distribution (PNG is chosen over JPEG because it is lossless, avoiding compression artifacts during tuning and development; JPEG conversion can be done as a separate post-processing step if needed).
*   **Canonical Processing**: The pipeline is designed to be "canonical," meaning it focuses strictly on the mathematical methods required to process the image. It avoids user-driven UI conventions or features not essential to the core tuning and rendering process.
*   **Edit Step Architecture**: The pipe processes images through a sequence of **edit steps**. Each edit step contains all 6 available modules, but only enables the modules relevant to its purpose. This allows flexible workflows: tune-only (color/tone matching), user-manual (creative editing), or hybrid (geometry preparation followed by automatic tuning).

## Components (Parts)

Parts are modular libraries that provide specific functionalities. They expose a public API using the PIMPL idiom in `inc/` headers to hide implementation details. They are compiled into libraries in `lib/`.

*   [**`labs`**](./doc/labs.md): The central data model for all LABS operations. It manages settings for the entire pipeline, stored in a single JSON sidecar file accompanying each RAW file. Each part has its own dedicated section within this JSON file.
*   [**`pipe`**](./doc/pipe.md): The core image processing pipeline.
    *   **HEAD**: Decodes the RAW file into a scene-referred linear space. The specific decoder is selectable via the `.pipe.json` configuration.
    *   **BODY**: A sequence of configurable **edit steps**. Each step can enable/disable specific modules from the 6 available modules (geometric adjustments, color correction, tone mapping, global color, selective color, detail + output transform). Geometry is always its own separate step, positioned before tune steps or after creative editing steps depending on the workflow.
    *   **TAIL**: Renders the final image to a **PNG** file (lossless format to preserve quality during development and tuning). Gamma encoding is applied internally.
*   [**`diff`**](./doc/diff.md): A library to compute loss metrics between images. Provides **spectral loss** (color/tone, geodesic distance) and **frequency loss** (sharpness, Laplacian variance). Both are content-invariant.
*   [**`tune`**](./doc/tune.md): A library that uses `diff` to optimize pipeline settings. Handles three roles: **color/tone** (17 dials + 17³ LUT, SPSA + spectral), **sharpness** (2 dials, golden section + frequency), and **geometry** (6 dials, user-controlled). Outputs style sidecars.

## Headless Tools (Programs)

These are command-line executables located in `bin/` that use the parts to perform tasks.

*   [**`tune`**](./doc/tune.md): Optimizer that finds optimal Link settings to match camera preview. Outputs `edit.json`.
*   [**`labs`**](./doc/labs.md): Pipe runner that processes RAW with optional edit settings. Outputs PNG.
*   [**`diff`**](./doc/diff.md): Computes spectral loss (color/tone) and frequency loss (sharpness) between images.

### Two-Phase Workflow

```
PHASE 1: OPTIMIZE
  tune photo.ARW --output edit.json
  → Loads RAW, uses embedded preview as target
  → Runs SPSA optimizer to find dial values
  → Saves optimized Link settings to edit.json

PHASE 2: APPLY
  labs photo.ARW --output tail.png --edit edit.json
  → Loads RAW through HEAD (decode + color science)
  → Loads edit.json into BODY as Link
  → Runs BODY → TAIL → saves PNG
```

This separation allows:
- Batch optimization (tune once, apply to many)
- Manual editing (create edit.json by hand or GUI)
- Style transfer (apply one image's edit.json to another)

## RAW Decoding

RAW decoding is handled by [RAWS](../RAWS/README.md), a separate project that produces `raws.a`. LABS links this library and calls `raws::decode()` to obtain scene-linear RGB from any supported camera format.

This separation means:
- Camera support R&D happens in RAWS, not LABS
- Adding Sony/Canon/Nikon support doesn't change LABS code
- LABS remains stable while RAWS evolves

## Out of Scope

To maintain focus on a canonical, camera-to-web pipeline, the following areas are considered out of the project's current scope. This list helps preserve ideas for future expansion. See [out_of_scope.md](./doc/mods/out_of_scope.md) for detailed module ideas.

*   **Alternative Delivery Formats**: The primary output is PNG (lossless). Future support for additional formats (TIFF for print, JPEG with quality settings, other color spaces like Display P3 or Adobe RGB) is out of scope.
*   **User Interface (UI) Conventions**: Features designed for interactive UI applications, such as non-essential metadata displays, complex preset management systems, or UI-specific groupings are excluded.
*   **Advanced/Future Modules**: See [out_of_scope.md](./doc/mods/out_of_scope.md) for modules that may enhance tune/diff in the future:
    *   Advanced noise reduction algorithms
    *   Lens distortion correction profiles
    *   Additional color grading tools

## Overall Success Criteria

The `PQTR:LABS` system achieves its goals when:

1.  **RAW to "LABS GOLD" PNG Image Handoff Works Seamlessly**: Linear RGB format validated, no data loss or corruption, and metadata preserved.
2.  **Pipe Produces Perceptually Accurate Output**: All dials functional, default dials result in a neutral look, and output matches professional tools (Lightroom, darktable).
3.  **Diff Provides Accurate Loss Metrics**: Spectral loss (geodesic) validated for color/tone; frequency loss (Laplacian) validated for sharpness. Both content-invariant. **Achieved: 0.05% spectral loss.**
4.  **Tune Optimizes All Three Roles**: SPSA + 17³ LUT converges in ~5 minutes for 17 color/tone dials; edge optimizer converges in ~2 seconds for 2 sharpness dials; user handles 6 geometric dials.
5.  **Performance Targets Met**: `pipe`: 30+ fps @ 1080p, `tune`: ~65 seconds total (SPSA + edge), Full pipeline: under 1 second per image.

## Documentation Quality Standards

All LABS documentation maintains the following standards:

- **Voice**: Declarative present tense (describes what the system does, not what it will do or should do)
- **Tense**: Present for current capabilities, conditional future ("may", "could") for out-of-scope features
- **Terminology**: Consistent use of "edit step", "module", "dial", "camera to web"
- **Structure**: Clear hierarchy, proper cross-references, logical flow from concept to implementation
- **Technical Accuracy**: All code examples, dial counts, and architectural descriptions must match implementation

**Periodic Review**: Run a quality control review to verify voice consistency, tense usage, internal consistency, terminology alignment, and coherence across all documentation files. Reference [docs.md](./doc/docs.md) for the review template and criteria.