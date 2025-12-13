# PQTR

Your studio with you, working for you, helping you — where you are and to where you want to deliver.

## What PQTR Does

PQTR is a digital image processing laboratory. It processes RAW photo data at the highest available levels of data fidelity using proven canonical picture science techniques, optimized for hardware performance at the "edge" device where creatives do their work — on their computer and on their phone.

You are the HERO when using PQTR. PQTR works for you — you don't work for it. You are always in control. PQTR uses AI to help you create, but AI never touches your creations. PQTR leaves all creativity to you and works hard to leave you more time to be creative. PQTR makes you look like a hero and deliver fast. Using PQTR allows you to go from creation to client as fast as technology allows.

PQTR also enhances your creativity. It lets you instantly view images as you compose them in the vibe you're looking for, and it lets you use vibes on new images so other photographers can apply your creativity as if you were there with them.

PQTR doesn't change your workflow — it aims to do nothing more than make you faster and your work more productive. You compose, shoot, edit in Lightroom, and deliver to clients. PQTR ties into this exact workflow with the simple goal of making it faster, smarter, and scalable to scale you.

## How PQTR Works

The PQTR pipeline manages your workflow using a simple pipeline that takes your RAW image from your gear and generates the finished creation to post to your client or their preferred platform. The pipeline has a head, a body, and a tail.

```
┌─────────────────────────────────────────────────────────────────┐
│                         PQTR Pipeline                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   HEAD ─────────────────────────────────────────────────────    │
│      │  RAW file → GEAR decode → scene-linear RGB               │
│      │  Extract: Bayer data, metadata, embedded preview         │
│      ▼                                                          │
│   BODY ─────────────────────────────────────────────────────    │
│      │  LUTE: Apply/learn camera manufacturer's look            │
│      │  DROP: Dynamic range optimization                        │
│      │  VIBE: Apply/learn photographer's creative style         │
│      ▼                                                          │
│   TAIL ─────────────────────────────────────────────────────    │
│      │  Produce final PNG, add EXIF, sign image                 │
│      │  POST: Deliver to client/platform                        │
│      ▼                                                          │
│   output.png → client                                           │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### HEAD — Camera Phase

The head takes RAW camera data, detects the camera type, extracts the Bayer sensor data and metadata, extracts the embedded manufacturer image created using the camera settings, then normalizes everything into PQTR format and pushes it into the body. RAW data is normalized into scene-linear RGB for maximum fidelity, the view image into lossless PNG (JPEG is lossy), and metadata into the PQTR info tree format for text, floats, and matrices.

### BODY — Processing Modules

The body is where all the work happens. It's a list of edit steps that can read any or all of the Bayer data, metadata, and camera view image data. Each edit step does its work using one or more PQTR modules and hands the data to the next step.

#### LUTE — Lookup Table Extractor

Each manufacturer applies camera style modules (lookup tables) using tone curves that allow non-linear enhancements. These tone curves are the secret recipes that manufacturers use to provide the "Sony" / "Canon" / "Nikon" look that photographers choose as their preferred camera for composition and editing.

When an image enters the pipeline, LUTE does three things:

1. If it has never seen this camera style before, extract LUT data from camera metadata and current image data to create an initial profile
2. If it has the style, check metrics to see if the new image can contribute additional information
3. Apply the learned profile in the pipeline

```
RAW file ──► GEAR (decode) ──► scene-linear RGB
                                    │
embedded JPEG ─────────────────────►│ compare
                                    ▼
                              LUTE (learn)
                                    │
                                    ▼
                           camera profile
```

LUTE learns four transform types:

| Transform | Size | Purpose |
|-----------|------|---------|
| BaseCurve | 768 floats | Per-channel tone response |
| PolyColor | 30 floats | Polynomial color mapping |
| LutCurve | 51 floats | Per-channel curves |
| HsvLut | 1,296 floats | HSV delta corrections |

Camera profiles are stored per camera model and creative style:

- `Sony_ILCE-7M4_Standard.lute`
- `Canon_EOS-R5_Faithful.lute`
- `Nikon_Z8_Vivid.lute`

#### DROP — Dynamic Range Optimization

DROP handles dynamic range optimization for high-contrast scenes. It follows the same three-step process as LUTE: detect, learn, apply.

#### VIBE — User Vibe

The user vibe is where the creative's magic happens. At its simplest level, VIBE is the PQTR equivalent of Lightroom/Darktable — user-selected style modules implemented as virtual "dials." You can dial up or down contrast, color ranges, and more.

| Module | Dials | Purpose |
|--------|-------|---------|
| Geometric | 6 | Crop, zoom, rotation |
| ColorCorrection | 3 | Exposure, white balance |
| ToneMapping | 7 | Contrast, shadows, highlights |
| GlobalColor | 3 | Vibrance, saturation, density |
| SplitTone | 4 | Shadow/highlight color grading |
| SelectiveColor | 24 | Per-hue HSL adjustments |
| Detail | 4 | Sharpen, denoise |

The first part of the magic: if you add a production image into LABS that has been styled by your favorite tool (e.g., Lightroom), VIBE will find the optimal dial settings to match your image — capturing your vibe. This vibe can then be applied to any new image.

The second part of the magic: you can have many VIBE steps. Apply an existing vibe in one step, then do quick edits in a subsequent step to finish the image — including crop and tilt — so it's ready to post to clients.

### TAIL — Output Phase

The tail is the final step of the pipeline. It produces a final, display-referred image in lossless PNG format. This master image can then be used in POST steps configured to send your image to your client, a social media platform, or multiple destinations. Pipeline steps can also add EXIF data to embed your process steps for later use and cryptographically sign the image to prove it is your creative work.

## Applications

### LABS

LABS is the PQTR application — available anywhere, any time. It provides a GUI for creating and editing vibes, processing images through the pipeline, and managing your camera profiles. Built with WebAssembly for cross-platform deployment.

### BASE

BASE is your PQTR home base — the web server, authentication system, and home of your PQTR data. It stores your RAWs, camera settings, Lightroom/Darktable vibe images, and your PQTR-processed images. BASE includes a FIND function for searching any image using natural language queries.

BASE uses the industry-standard sidecar model:

```
BASE/
├── GEAR/                           # Camera data by type
│   └── Sony_ILCE-7M4/
├── PIPE/                           # Image data by RAW filename
│   └── DSC00144/
│       ├── DSC00144.ARW            # Original RAW file
│       ├── DSC00144.png            # Embedded preview
│       ├── DSC00144.lute.json      # LUTE sidecar
│       ├── DSC00144.vibe.json      # VIBE sidecar
│       └── DSC00144.pipe.json      # Pipeline sidecar
```

All metadata is indexed from JSON sidecars and camera EXIF data. You can download part or all of your BASE data at any time.

## Projects

| Project | Purpose |
|---------|---------|
| [GEAR](GEAR/README.md) | Camera gear — RAW decoding, metadata, scene-linear normalization |
| [LUTE](LUTE/README.md) | Camera profiles — learns gear manufacturer's color science |
| [DROP](DROP/README.md) | Dynamic range — optimization for high-contrast scenes |
| [VIBE](VIBE/README.md) | Creative styles — 51 adjustable dials for photographer expression |
| [PIPE](PIPE/README.md) | Pipeline library — coordinates HEAD [GEAR] → BODY [LUTE,DROP,VIBE] -> TAIL [POST] |
| [POST](POST/README.md) | Distribution — platform plugins for Instagram, Email, SMS, etc. |
| [WGPU](WGPU/README.md) | GPU compute — WebGPU/WGSL shaders for fast processing |
| [LABS](LABS/README.md) | WASM app — Anywhere, any time, creating and editing vibes |
| [BASE](BASE/README.md) | Web server — JWT auth + static site for pqtr.ai |

Reference documentation is in [docs/](docs/).

## Code Standards

### Directory Structure (FHS-inspired)

```
PROJECT/
├── inc/          # Public headers (API only)
├── src/
│   ├── main/     # Implementation
│   └── test/     # Tests (one file per module)
├── lib/          # Dependencies (git submodules)
├── bin/          # Executables
├── tmp/          # Build artifacts (gitignored)
├── var/          # Runtime data (gitignored)
├── etc/          # Configuration files
└── README.md     # Project documentation
```

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Types/Classes | PascalCase | `Node`, `Data`, `Link`, `Pipe` |
| Functions/Methods | camelCase | `dial()`, `text()`, `make()`, `find()` |
| Member variables | m_prefix | `m_impl`, `m_links`, `m_name` |
| Type aliases | PascalCase | `Name`, `List`, `Hold`, `Dict`, `Page`, `Info` |
| Namespaces | lowercase | `pipe::`, `gear::`, `vibe::` |

### Code Patterns

- **Type aliases** for STL types — keeps API stable if implementation changes
- **PIMPL** for public classes — hides implementation, enables ABI stability
- **Move-only** for resource types — delete copy, default move
- **Virtual interfaces** with factory functions — `Hold<Pipe> make()`
- **Single namespace** per module — `namespace pipe { ... }`
- **No exceptions** — return `nullptr` or `bool` for errors
- **No cout** in library code — tests only

### File Conventions

- One public header per module: `inc/pipe.hpp`
- Implementation matches header: `src/main/pipe.cpp`
- Test matches module: `src/test/pipe.cpp`
- Build to `tmp/`, install to `bin/`

## Development

**Prerequisites:** g++, make, git, autotools (for libsodium build)

**First time setup:**
```bash
git clone --recursive <repo-url>
cd pqtr
bash init.sh   # Downloads emsdk (~350MB), builds libsodium
```

**Test locally:**
```bash
bash test.sh        # Incremental build + run server
bash test.sh clean  # Full rebuild + run server
# Open http://127.0.0.1:4040
```

**Deploy to production:**
```bash
bash send.sh   # Build + deploy to pqtr.ai
```
