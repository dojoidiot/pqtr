# PQTR

Professional photo processing, from camera to social media.

## What It Does

PQTR takes RAW photos from professional cameras and automatically processes them to match your style. One workflow to go from camera file to Instagram-ready image.

**The Problem:** Professional photographers spend hours in Lightroom adjusting every photo. Social media demands fast turnaround.

**The Solution:** PQTR learns your editing style once, then applies it automatically to every photo.

| Metric | Value |
|--------|-------|
| Processing time | <1 second per photo |
| Automated adjustments | 45 dials |
| Output sizes | Any (1080px social, 2048px web, full resolution) |

## Core Innovation: TUNE

TUNE automatically learns your style from a single reference photo:

| Component | What It Does | Dials | Time |
|-----------|--------------|-------|------|
| **GeoS** | Color & tone matching (warm/cool, saturated/muted, contrast) | 35 | ~60s |
| **Diff** | Sharpness matching (texture, edge definition) | 4 | ~2s |
| User | Geometry (crop, rotation) | 6 | manual |

GeoS uses geodesic distance on a mathematical hypersphere to measure style similarity regardless of image content.

## The Vibe Workflow

**Vibes** are portable style presets created using TUNE:

1. **Create** (DESK) - Pro creates a Vibe using TUNE
2. **Publish** - Vibe syncs to cloud instantly
3. **Apply** - Anyone applies the Vibe to their photos

---

# Technical Reference

## Projects

| Project | Description |
|---------|-------------|
| [**BASE**](./BASE/README.md) | Web server + JWT auth + static site |
| [**DESK**](./DESK/README.md) | Desktop GUI for creating vibes |
| [**LABS**](./LABS/README.md) | Core processing pipeline |
| [**RAWS**](./RAWS/README.md) | Camera RAW decoder (Sony, Canon, Nikon) |
| [**TUNE**](./TUNE/README.md) | Style optimizer (GeoS algorithm) |
| [**VIBE**](./VIBE/README.md) | Image processing modules (17 mods, 45 dials) |
| [**WGPU**](./WGPU/README.md) | WebGPU compute library (Dawn backend) |

## Data Flow

```
Camera RAW files
       │
       ▼
    [RAWS] ─── decode ───► Scene-linear RGB
       │
       ▼
    [VIBE] ─── style ────► Display-ready RGB
       │
       ▼
    [TUNE] ─── optimize ─► Vibe preset (.pipe.json)
```

## Building

```bash
./wire.sh && make    # Build everything
make clean           # Clean all projects
```

## Testing

```bash
# VIBE module tests
cd VIBE && make test

# WGPU shader tests
cd VIBE && make test-dawn
cd RAWS && make test-dawn

# TUNE optimizer
cd TUNE && ./bin/tune --help
```
