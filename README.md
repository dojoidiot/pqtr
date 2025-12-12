# PQTR

Your studio with you, working for you, helping you; where you are and to where you want to deliver.

## what PQTR does

PQTR is a digital image processing laboratory.  It processes RAW photo data at the highest available levels of data fidelity using proven canonical picture science techniques, optimized for hardware performance at the "edge" device where Creatives do their work - on their computer and on their phone.

You are the HERO when using PQTR.  PQTR works for you - you don't work for it.  You are always in control - PQTR uses AI to help you create, but AI NEVER touches creations. PQTR leaves all creativity to you, and works hard to leaves you more time to be creative. And PQTR works to make you look like a HERO and deliver fast.  Using PQTR allows you to go from creation to client as fast as technology allows.

PQTR also enhances your creativity - PQTR lets you instantly view images as you compose them in the VIBE you are looking for, and it lets you use VIBES on new images so can let other photographers apply your creativity as I you were there with them.

PQTR doesn't change your workflow - it aims to do nothing more than make you faster and your work more productive.  You compose, shoot, edit in LightRoom, and deliver to clients. PQTR ties into this exact workflow with the simple goal of making it faster, smarter, and scaleable to scale YOU.

## How PQTR Works

PQTR:PIPE manages your work flow using a simple pipe line that takes your RAW image from you gear and generates the finished creation to post it to your client or their preferred platform.  The pipe has a head, a body, and a tail.

```
┌─────────────────────────────────────────────────────────────────┐
│                         PQTR Pipeline                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   HEAD ─────────────────────────────────────────────────────    │
│      │  RAW file → RAWS decode → scene-linear RGB               │
│      │  Extract: bayer data, metadata, embedded preview         │
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

### HEAD - Camera Phase

The head takes RAW camera data, detects the camera type, extracts the camera bayer sensor data, camera/image meta data, extracts the embedded manufacturer image that was created using the camera settings, then normalises all of them into PQTR labs format and pushes it into the BODY.  RAW data is normalised into scene-linear RGB for maximum fidelity, the view image into lossless PNG (JPEG is lossy) and the PQTR info tree format for meta data types of text, float, and matrixes/arrays.

### BODY - Processing Modules

The body is where all the work happens.  It's a list of edit steps that can read any or all of the bayer data, meta data and camera view image data.  Each edit step does its work using one or more of the PQTR modules, and hands the data on to the next step.

#### LUTE - Lookup Table Extractor

Each manufacturer applies Camera Style modules (lookup tables - LUTs) using tone curves that allow non-linear enhancements.  These tone curves are the secret recipes that manufacturers use to provide the "Sony"/"Canon"/"Nikon" look that photographers choose as their preferred camera for composition and editing selection.

When an image enters the pipe, LUTE does 3 things:
1. If it has never seen this camera style before, extract LUT data from camera metadata and current image data to create an initial profile
2. If it has the style, check metrics to see if the new image can contribute additional information
3. Apply the learned profile in the pipeline

```
RAW file ──► RAWS (decode) ──► scene-linear RGB
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
| HsvLut | 1296 floats | HSV delta corrections |

Camera profiles are stored per camera model + creative style:
- `Sony_ILCE-7M4_Standard.lute`
- `Canon_EOS-R5_Faithful.lute`
- `Nikon_Z8_Vivid.lute`

#### DROP - Dynamic Range Optimization

DROP handles dynamic range optimization for high-contrast scenes. It follows the same three-step process as LUTE: detect, learn, apply.

#### VIBE - User Vibe

The User VIBE is where the creative's magic happens.  At its simplest level, VIBE is literally the PQTR lightroom/darktable feature where user selected style modules are implemented and made available as virtual "dials".  You can dial up/down contrast, colour ranges - you name it, it's all in there.

| Module | Dials | Purpose |
|--------|-------|---------|
| Geometric | 6 | Crop, zoom, rotation |
| ColorCorrection | 3 | Exposure, white balance |
| ToneMapping | 7 | Contrast, shadows, highlights |
| GlobalColor | 3 | Vibrance, saturation, density |
| SplitTone | 4 | Shadow/highlight color grading |
| SelectiveColour | 24 | Per-hue HSL adjustments |
| Detail | 4 | Sharpen, denoise |

The first part of the magic: if you add a production image into LABS that is styled by your favourite tool (e.g. Lightroom) then VIBE will find the optimal dial settings to match your image - capturing your VIBE!  This VIBE can then be applied to any new image.

The second part of the magic: you can have many VIBE steps - apply an existing VIBE in one step, then do quick edits in a subsequent step to finish the image including crop and tilt so it's ready to POST to clients.

### TAIL - Output Phase

Tail is the final step of the pipe.  It produces a final, display-referred image in lossless PNG format.  This master image can then be used in POST steps which can be configured to send your image to your client or a social media platform account, or multiple POST steps.  PIPE steps can also add image EXIF data to embed your process steps for later use, and cryptographically sign the image to prove it is your creative work.

## Applications

### LABS

LABS is the application you use to use PQTR.  It is a fully dynamic web app that determines what view you need based on the size of the screen you are working on.  LABS uses highly optimised WASM (web assembly language) and WEBGL GPU optimised code so that all tasks are done on the device instantly, and fast.

### BASE

BASE is your PQTR home base.  It is the web server, authentication system, and home of your PQTR data - your RAWS, camera settings, Lightroom/Darktable VIBE images, and your PQTR Pipe VIBED images that you have posted.  It also has a FIND function that allows you to find any image using natural language queries.

## Projects

| Project | Purpose |
|---------|---------|
| **RAWS** | RAW decoder - extracts scene-linear RGB from camera files |
| **LUTE** | Camera profiles - learns gear manufacturer's color science |
| **VIBE** | Creative styles - 51 adjustable dials for photographer expression |
| **TUNE** | Style optimizer - finds dial values to match a reference |
| **LABS** | Pipeline orchestrator - coordinates RAWS → LUTE → VIBE |
| **WGPU** | GPU compute - WebGPU/WGSL shaders for fast processing |
| **DESK** | Desktop app - GUI for creating and editing vibes |
| **BASE** | Web server - JWT auth + static site for pqtr.ai |

## Building

```bash
./wire.sh && make    # Build all projects
make clean           # Clean everything
```

## Testing

```bash
# GPU shader tests
cd VIBE && make test-dawn
cd RAWS && make test-dawn

# Module tests
cd VIBE && make test
cd LUTE && make test
```
