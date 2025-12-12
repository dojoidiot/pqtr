# PQTR

Professional photo processing from camera to social media.

## How It Works

PQTR processes RAW photos in two phases that mirror the professional workflow:

### Phase 1: Camera Profile (LUTE)

When photographers compose shots, they see the camera manufacturer's interpretation of the scene - the "out of camera" look on their LCD/viewfinder.

**LUTE** learns this camera-specific color science by analyzing RAW files alongside their embedded preview JPEGs:

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

Camera profiles are stored per camera model + creative style:
- `Sony_ILCE-7M4_Standard.json`
- `Canon_EOS-R5_Faithful.json`
- `Nikon_Z8_Vivid.json`

### Phase 2: Creative Style (VIBE)

After camera profile application, photographers apply their creative adjustments - the dials they twist in Lightroom/Darktable to express their personal style.

**VIBE** provides 51 adjustable dials organized into modules:

| Module | Dials | Purpose |
|--------|-------|---------|
| Geometric | 6 | Crop, zoom, rotation |
| ColorCorrection | 3 | Exposure, white balance |
| ToneMapping | 7 | Contrast, shadows, highlights |
| GlobalColor | 3 | Vibrance, saturation, density |
| SplitTone | 4 | Shadow/highlight color grading |
| SelectiveColour | 24 | Per-hue HSL adjustments |
| Detail | 4 | Sharpen, denoise |

**TUNE** optimizes these dials to match a reference image - learn a style once, apply everywhere.

## Complete Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                         LABS Pipeline                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   RAW file                                                      │
│      │                                                          │
│      ▼                                                          │
│   ┌──────┐                                                      │
│   │ RAWS │  Decode camera RAW to scene-linear RGB               │
│   └──┬───┘                                                      │
│      │                                                          │
│      ▼                                                          │
│   ┌──────┐  Camera Profile (learned from gear)                  │
│   │ LUTE │  - BaseCurve: tone response                          │
│   │      │  - PolyColor: polynomial color transform             │
│   │      │  - LutCurve: 17³ 3D LUT                              │
│   │      │  - HsvLut: HSV delta corrections                     │
│   └──┬───┘                                                      │
│      │                                                          │
│      ▼                                                          │
│   ┌──────┐  Creative Style (photographer's adjustments)         │
│   │ VIBE │  - 51 dials: exposure, contrast, color, detail       │
│   │      │  - Organized into 7 modules                          │
│   └──┬───┘                                                      │
│      │                                                          │
│      ▼                                                          │
│   output.png                                                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

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

## Key Concepts

### Vibes
Portable style presets (`.vibe.json`) containing all 51 dial values. Created by photographers, shared with clients.

### Camera Profiles
Learned color transforms (`.profile.json`) specific to camera model + creative style. Accumulated from multiple RAW+preview pairs.

### Separation of Concerns

| | LUTE | VIBE |
|---|------|------|
| **What** | Camera color science | Photographer creativity |
| **Learned from** | RAW + embedded preview | Reference image |
| **Varies by** | Camera model | Photographer style |
| **Contains** | LUTs, curves, polynomials | 51 adjustable dials |

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
