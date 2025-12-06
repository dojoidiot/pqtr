# PQTR:DESK

[back](../README.md)

Desktop application for power users. Create and tune image processing styles using the full LABS pipeline.

## Role in PQTR

DESK is for professionals and enthusiasts who want full control over RAW processing. Styles created in DESK can be shared with PLAY users.

```
[LABS] ──► [DESK] ──► Styles (.pipe.json)
             │              │
             │              └──► PLAY users apply these
             └── Power user workflow
```

- **Consumes**: `labs.a` (via symlink)
- **Produces**: `desk` executable, `.pipe.json` style files
- **Audience**: Photographers, colorists, power users

## Features

- **Project Management**: Organize RAW files with sidecar-based structure
- **Pipe Editor**: Visual tree with HEAD/BODY structure and inline preview
- **Link Editor**: 6 modules with 45 dials per Link
- **Live Preview**: In-memory rendering at selectable preview sizes (512–4096px or full)
- **Export**: Full-resolution PNG output on demand
- **Style Export**: Save styles as `.pipe.json` for use in PLAY

## Pipe Panel

The Pipe panel shows the processing tree with an inline image preview:

```
┌─────────────────────────────────────┐
│ Tree (left)      │ Preview (right)  │
├──────────────────┼──────────────────┤
│ HEAD             │                  │
│ ├── base         │   [selected      │
│ └── view         │    image]        │
│ BODY             │                  │
│ └── Link 1       │                  │
│     ├── CC       │                  │
│     └── Tone     │                  │
└──────────────────┴──────────────────┘
```

| Node | Shows |
|------|-------|
| `HEAD > base` | Scene-linear RGB from RAWS (gamma-corrected for display) |
| `HEAD > view` | Embedded camera JPEG preview |
| `BODY` | Processed output after all Links |

## Output Files

When processing a RAW file, DESK produces these outputs:

```
<name>.base.png      # HEAD base: scene-linear from RAWS (gamma for display)
<name>.view.png      # HEAD view: embedded camera JPEG preview
<name>.0.pipe.png    # BODY step 0: first link output
<name>.1.pipe.png    # BODY step 1: second link output (if exists)
...
<name>.tail.png      # Final output (same as last step)
<name>.diff.png      # Difference: view vs tail (4x amplified)
<name>.pipe.json     # Pipe configuration (dials, LUT, etc.)
```

## Architecture

| Component | Description |
|-----------|-------------|
| Sidecar Files | `<name>.desk.json` (project), `<name>.pipe.json` (pipe config) |
| UI Framework | ImGui + GLFW + OpenGL |
| Input | Any format supported by RAWS (Sony ARW, etc.) |
| Output | PNG (lossless) |

## Project Structure

```
DESK/
├── bin/
│   └── desk              # Built executable
├── lib/
│   ├── LABS.a            # [symlink] → LABS/lib/labs.a
│   ├── imgui/            # ImGui library
│   ├── glfw/             # GLFW library
│   └── ImGuiFileDialog/  # File dialog
├── src/
│   └── main/
│       ├── desk.cpp      # Main entry point
│       └── part/         # UI components
├── var/                  # Default project folder (test images)
└── Makefile.desk
```

## Building

```bash
# From DESK directory
make -f Makefile.desk

# Or from repository root
make desk
```

## Running

```bash
# Default project folder (var/)
./bin/desk

# Custom project folder
./bin/desk /path/to/photos
```

## Workflow

1. Open a folder containing RAW files
2. Select an image to edit
3. Adjust pipeline settings (HEAD decoder, BODY modules, TAIL output)
4. Save style as `.pipe.json`
5. Share style with PLAY users or apply to batch processing

## Documentation

- [Specification](doc/desk.md) - Full application spec
- [Ideas](doc/idea.md) - Future enhancement ideas
