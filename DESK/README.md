# `PQTR:DESK`

Project management interface for `LABS`. Provides a graphical environment for managing RAW image processing projects using the LABS pipe system.

## Features

- **Project Management**: Organize RAW files with sidecar-based structure (no subdirectories)
- **Pipe Editor**: Visual editor for HEAD → BODY → TAIL pipeline
- **Link Editor**: 6 modules with 45 dials per Link
- **Live Preview**: Output PNG updates on dial changes

## Architecture

| Component | Description |
|-----------|-------------|
| Sidecar Files | `<name>.desk.json` (project), `<name>.pipe.json` (pipe config) |
| UI Framework | ImGui-based interface |
| Input | Sony ARW (more formats planned) |
| Output | PNG |

## Build

```bash
cd DESK
make -f Makefile.desk
```

Run with default project folder (`var/`):
```bash
./bin/desk
```

Run with custom project folder:
```bash
./bin/desk /path/to/photos
```

## Documentation

- [Specification](doc/desk.md) - Full application spec
- [Ideas](doc/idea.md) - Future enhancement ideas
