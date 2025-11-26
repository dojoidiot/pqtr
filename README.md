# PQTR

PQTR is a scalable media technology platform that streamlines enterprise creative workflows, from capture through collaboration to delivery.

## Quick Start

```bash
make        # Build everything
make clean  # Clean all projects
```

## High-Level Architecture: MAINs

The repository is organized into top-level projects called **MAINs**. These are self-contained applications or services with their own executables.

### Design Philosophy

We use a **PIMPL-style separation** at the project level for code clarity:

- Each MAIN is self-contained with its own `src/`, `inc/`, and `lib/` directories
- MAINs do not share code directly—they consume **artifacts** (headers, source, libraries) from other MAINs
- Dependencies are explicit and managed through symlinks created by `wire.sh`
- A master `Makefile` orchestrates builds in the correct dependency order

This gives us the benefits of a monorepo (single checkout, atomic changes) while maintaining clear boundaries between projects.

### Current MAINs

| Project | Description | Produces |
|---------|-------------|----------|
| [**RAWS**](./RAWS/README.md) | GPL-free RAW decoder (Sony, future: Canon, Nikon) | `raws.a` static library |
| [**LABS**](./LABS/README.md) | Digital film development laboratory | `labs.a` static library |
| [**DESK**](./DESK/README.md) | Desktop pipeline editor for power users | `desk` executable |
| [**FAST**](./FAST/README.md) | Field capture app (custom Android) | — |
| [**PLAY**](./PLAY/README.md) | Consumer mobile app (iOS/Android) | — |
| [**SITE**](./SITE/README.md) | Public website for pqtr.ai | — |

### Dependency Tree

```
RAWS (RAW decoder library)
  │
  ├──[inc]──► LABS (includes raws.hpp API)
  ├──[lib]──► LABS (links raws.a into labs.a)
  │             │
  │             └──[lib]──► DESK (links labs.a)
  │
  └──[standalone]──► RAWS test binary (make raws-test)
```

**Flow:**
1. **RAWS** builds `raws.a` exposing `raws::decode()` API
2. **LABS** links `raws.a` into `labs.a`, calls `raws::decode()` - knows nothing about Sony internals
3. **DESK** links `labs.a` to build the GUI

## Dependency Wiring

The `wire.sh` script manages cross-project dependencies by creating symbolic links. This allows projects to share headers, source files, and libraries without duplicating code.

### Model

```
WIRE <FROM> <type> <INTO>
```

- **FROM**: The source project that provides the artifact
- **type**: One of `inc` (headers), `src` (source), or `lib` (static library)
- **INTO**: The target project that consumes the artifact

The symlink is created inside INTO, pointing to FROM's artifact.

### Assumptions

- Projects use **UPPERCASE** directory names (LABS, DESK, RAWS)
- Each project follows a standard structure:
  ```
  PROJECT/
  ├── inc/      # public headers
  ├── src/      # source files
  └── lib/      # built libraries
  ```
- Static libraries are named `<project>.a` in **lowercase** (e.g., `LABS/lib/labs.a`)
- Script must run from repository root

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

Symlinks are automatically added to `.gitignore`.

## Building

The master `Makefile` handles wiring and build order automatically:

```bash
make          # Wire + build LABS + build DESK
make labs     # Build LABS only (wires first)
make desk     # Build DESK (builds LABS first)
make raws     # Build RAWS standalone test binary
make clean    # Clean all projects
make rewire   # Remove and recreate all symlinks
```

### Manual Build (if needed)

```bash
./wire.sh                        # Create symlinks
cd RAWS && make -f Makefile.raws # Build RAWS (produces raws.a)
cd LABS && make -f Makefile.labs # Build LABS (produces labs.a, includes raws.a)
cd DESK && make -f Makefile.desk # Build DESK (links labs.a)
```

## Product Vision

PQTR is a layered ecosystem for RAW image processing:

### Core Libraries

| Layer | Project | Role |
|-------|---------|------|
| **Decoder** | RAWS | Camera-specific RAW decoding. R&D happens here as new cameras are supported. Isolated from LABS—changes don't affect downstream. |
| **Pipeline** | LABS | The "settled science" of image processing. Stable utility library exposing `pipe::Pipe` for HEAD→BODY→TAIL processing. |

### Applications

| App | Platform | Audience | Description |
|-----|----------|----------|-------------|
| **DESK** | Desktop | Power users | Full-featured pipeline editor. Create and tune processing styles. |
| **FAST** | Android (custom) | Field shooters | Minimal app on custom Android build. FTP transfer over WiFi for rapid ingest. |
| **PLAY** | iOS/Android | Consumers | Apply DESK-created styles to phone DNGs. Bridges pro workflows to casual users. |

### Data Flow

```
Camera RAW files
       │
       ▼
    [RAWS] ─── decodes ───► Scene-linear RGB
       │
       ▼
    [LABS] ─── processes ──► Styled output
       │
       ├──► DESK (create styles)
       ├──► FAST (field capture)
       └──► PLAY (apply styles)
```

DESK users create styles. PLAY users consume them. FAST users capture for later processing.