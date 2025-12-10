# PQTR

Professional photo processing, from camera to social media.

**If we get F1 right, we get everyone right.**

## What It Does

PQTR takes RAW photos from professional cameras and automatically processes them to match your style. One workflow to go from camera file to Instagram-ready image.

**The Problem:** Professional photographers spend hours in Lightroom adjusting every photo. Social media demands fast turnaround.

**The Solution:** PQTR learns your editing style once, then applies it automatically to every photo.

| Metric | Value |
|--------|-------|
| Processing time | <1 second per photo |
| Automated adjustments | 45 dials |
| Output sizes | Any (1080px social, 2048px web, full resolution) |

## Product Suite

| Project | Description | Status |
|---------|-------------|--------|
| **DESK** | Create vibes | In Dev |
| **FAST** | Perfect social media delivery, now | Planned |
| **PLAY** | Apply pro vibes | Planned |

## Core Innovation: TUNE

TUNE automatically learns your style from a single reference photo:

| Component | What It Does | Dials | Time |
|-----------|--------------|-------|------|
| **GeoS** | Color & tone matching (warm/cool, saturated/muted, contrast) | 35 | ~60s |
| **Diff** | Sharpness matching (texture, edge definition) | 4 | ~2s |
| User | Geometry (crop, rotation) | 6 | manual |

GeoS uses geodesic distance on a mathematical hypersphere to measure style similarity regardless of image content. Show PQTR one photo you love, and it learns to make all your photos look like that.

## The Vibe Workflow

**Vibes** are portable style presets created using TUNE:

1. **Create** (DESK) - Pro creates a Vibe using TUNE
2. **Publish** - Vibe syncs to cloud instantly
3. **Apply** (FAST/PLAY) - Anyone applies the Vibe to their photos in one tap

A wedding photographer creates their signature look once on DESK. That Vibe is instantly available to their second shooter (FAST), the bride (PLAY), or any client who purchases the Vibe pack.

## Competitive Advantage

- **Speed** - 30x faster than manual Lightroom editing
- **Consistency** - Same look across thousands of photos
- **No lock-in** - GPL-free codebase, standard PNG output
- **Social-first** - Built for web delivery, not print
- **Style transfer** - Learn once, apply everywhere (GeoS innovation)

## Business Model

- **Pro tier** - DESK license for professional photographers and studios
- **Consumer tier** - PLAY freemium with premium style packs
- **Enterprise** - LABS embedded in to media company, agency, and newsroom workflows.

---

# Technical Reference

## Quick Start

```bash
make        # Build everything
make clean  # Clean all projects
```

## Architecture: MAINs

The repository is organized into top-level projects called **MAINs**. These are self-contained applications or services with their own executables.

### Design Philosophy

We use a **PIMPL-style separation** at the project level:

- Each MAIN is self-contained with its own `src/`, `inc/`, and `lib/` directories
- MAINs do not share code directly—they consume **artifacts** (headers, source, libraries) from other MAINs
- Dependencies are explicit and managed through symlinks created by `wire.sh`
- A master `Makefile` orchestrates builds in the correct dependency order

### All Projects

| Project | Description | Status |
|---------|-------------|--------|
| [**APEX**](./APEX/README.md) | Core infrastructure (Hold, Sink) | Active |
| [**RAWS**](./RAWS/README.md) | Camera RAW decoder (Sony, Canon, Nikon) | Active |
| [**LABS**](./LABS/README.md) | Core processing engine with 45 adjustment dials | Active |
| [**DESK**](./DESK/README.md) | Create vibes | In Dev |
| [**FAST**](./FAST/README.md) | Perfect social media delivery, now | Planned |
| [**PLAY**](./PLAY/README.md) | Apply pro vibes | Planned |
| [**SITE**](./SITE/README.md) | Marketing website at pqtr.ai | In Dev |

### Data Flow

```
Camera RAW files
       │
       ▼
    [RAWS] ─── decodes ───► Scene-linear RGB
       │
       ▼
    [LABS] ─── TUNE ──► Vibe (style preset)
       │
       ├──► DESK (create Vibes)
       ├──► FAST (apply Vibes in field)
       └──► PLAY (apply Vibes on phone)
```

### Dependency Tree

```
RAWS (RAW decoder library)
  │
  ├──[inc]──► LABS (includes raws.hpp API)
  ├──[lib]──► LABS (links raws.a into labs.a)
  │             │
  │             └──[lib]──► DESK (links labs.a)
  │
  └──[test]──► RAWS test binary (make test-raws)
```

### Layer Separation

| Layer | Project | Role |
|-------|---------|------|
| **Decoder** | RAWS | Camera-specific RAW decoding. R&D happens here as new cameras are supported. Isolated from LABS. |
| **Pipeline** | LABS | Core processing engine. Stable library exposing `pipe::Pipe` for HEAD→BODY→TAIL processing. |
| **Apps** | DESK/FAST/PLAY | User interfaces. DESK creates Vibes, FAST/PLAY consume them. |

### Architecture: RAWS vs TUNE

**Critical principle:** RAWS and TUNE have distinct responsibilities.

| | RAWS | TUNE |
|---|------|------|
| **Purpose** | Canonical extraction | Style optimization |
| **Output** | Scene-referred linear RGB | Display-referred, reference-matched |
| **Camera-specific** | Yes (decoding) | No (camera-agnostic transforms) |
| **Style decisions** | None | All |

**RAWS output will NOT look like a camera JPEG.** This is correct—scene-linear data is flat and desaturated before display transforms. The camera JPEG is just one style that TUNE can match.

**Validation:** TUNE error rates, not visual appearance of RAWS output. If TUNE achieves low error, RAWS is working correctly.

## Dependency Wiring

The `wire.sh` script manages cross-project dependencies by creating symbolic links.

### Model

```
WIRE <FROM> <type> <INTO>
```

- **FROM**: The source project that provides the artifact
- **type**: One of `inc` (headers), `src` (source), or `lib` (static library)
- **INTO**: The target project that consumes the artifact

### Current Wiring Rules

| Rule | Creates | Effect |
|------|---------|--------|
| `WIRE RAWS inc LABS` | `LABS/inc/RAWS` → `RAWS/inc` | LABS can `#include "RAWS/raws.hpp"` |
| `WIRE RAWS lib LABS` | `LABS/lib/RAWS.a` → `RAWS/lib/raws.a` | LABS links RAWS library |
| `WIRE LABS lib DESK` | `DESK/lib/LABS.a` → `LABS/lib/labs.a` | DESK links LABS library |

### Usage

```bash
./wire.sh           # Create all symlinks
./wire.sh --unwire  # Remove all symlinks
```

## Building

```bash
make           # Build everything (wire + raws + labs + desk)
make raws      # Build RAWS library
make labs      # Build LABS library (builds RAWS first)
make desk      # Build DESK app (builds LABS first)
make clean     # Clean all projects
make rewire    # Remove and recreate all symlinks
```

## Testing

```bash
make test       # Run all tests (RAWS + LABS)
make test-raws  # Run RAWS decoder test
make test-labs  # Run LABS full test suite
```
