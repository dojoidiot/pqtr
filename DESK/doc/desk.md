# DESK Application Specification

[back](../README.md)

## Purpose

DESK is a project management interface for LABS. It provides a graphical environment for managing RAW image processing projects using the LABS pipe system.

---

## Architecture

DESK operates on a single root folder selected by the user. Structure is defined by sidecar files, not subdirectories.

### Components

| Component | Description |
|-----------|-------------|
| **Root Folder** | User-selected directory containing RAW files and sidecars |
| **Sidecar Files** | JSON files that define project and pipe state |
| **Pipe Integration** | Uses LABS pipe (HEAD → BODY → TAIL) for processing |
| **UI** | ImGui-based interface (skeleton in LABS) |

### Constraints

- **Never modify LABS**: DESK consumes LABS as-is. See [idea.md](./idea.md) for future enhancement ideas.
- **No file destruction**: DESK never deletes user files. Projects are hidden, not removed.
- **Single selection**: No multi-select for projects or Links.
- **Auto-save**: Sidecar files update automatically on change.

---

## File Structure

DESK uses sidecar files alongside RAW images. No subdirectory structure is imposed.

### Sidecar Files

For each RAW file `<name>.ARW`:

| File | Purpose |
|------|---------|
| `<name>.desk.json` | DESK project settings |
| `<name>.pipe.json` | Pipe configuration (see [LABS/doc/data.md](../../LABS/doc/data.md)) |
| `<name>.png` | Output image from pipe TAIL |

### Example

```
/user/photos/                    # Root folder (user selected)
  DSC01234.ARW                   # RAW image
  DSC01234.desk.json             # DESK project file
  DSC01234.pipe.json             # Pipe configuration
  DSC01234.png                   # Output PNG
  DSC05678.ARW                   # Another RAW
  DSC05678.desk.json
  DSC05678.pipe.json
  DSC05678.png
```

---

## Project Format

### desk.json

The `.desk.json` file stores DESK-specific project settings.

```json
{
  "version": "1.0",
  "hidden": false
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `version` | string | "1.0" | Schema version |
| `hidden` | bool | false | Project visibility in DESK |

When `hidden` is `true`, the project does not appear in the Projects panel. The files remain on disk.

### pipe.json

The `.pipe.json` file follows the LABS pipe JSON schema defined in [LABS/doc/data.md](../../LABS/doc/data.md).

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": []
}
```

---

## User Interface

### Layout

| Area | Description |
|------|-------------|
| **Menu Bar** | File menu for root folder selection |
| **Projects Panel** | Left panel with project tree |
| **Work Area** | Central area displaying output PNG |
| **Link Editor** | Right panel for module/dial editing (when Link selected) |

### File Menu

The File menu allows the user to select a root folder. All RAW files and sidecars in the root folder are discovered and displayed.

### Projects Panel

The Projects panel displays all non-hidden RAW projects.

| Element | Description |
|---------|-------------|
| **Title** | "Projects" header |
| **+ Button** | Add new RAW file (Sony ARW only) |
| **Project List** | Tree view of RAW projects |

### Project Tree

Each RAW file appears as a tree root node.

```
▸ DSC01234          [+] [-]
  └─ Link: tune_optimize  [✎]
  └─ Link: final_adjust   [✎]
▸ DSC05678          [+] [-]
```

| Element | Description |
|---------|-------------|
| **RAW Name** | File name without extension (tree root) |
| **+ Button** | Add new Link to pipe BODY |
| **- Button** | Hide project (sets `hidden: true` in desk.json) |
| **Link Entries** | Named Links in the pipe BODY |
| **✎ Icon** | Edit Link name inline |

### Work Area

The Work Area displays the output PNG of the currently selected project. The image updates when:
- A new project is selected
- Dial values change
- Links are added or removed

### Link Editor

When a Link is selected, the Link Editor displays:
- 6 modules (Geometric, Color Correction, Tone Mapping, Global Color, Selective Color, Detail)
- Dials for each module (45 total per Link)

---

## Operations

### Link Operations

| Action | Result |
|--------|--------|
| Press **+** on project | Adds Link named "New Link" to pipe BODY |
| Press **-** on Link | Removes Link from pipe BODY |
| Press **✎** on Link | Enables inline name editing |
| Select Link | Opens Link Editor, displays modules/dials |

### Project Operations

| Action | Result |
|--------|--------|
| Press **+** in Projects panel | Opens file dialog (Sony ARW filter) |
| Press **-** on project | Hides project (sets `hidden: true`) |
| Select project | Displays output PNG in Work Area |

---

## Workflow

### Adding a Project

1. User presses **+** in Projects panel
2. File dialog opens (Sony ARW filter)
3. RAW file is copied to root folder
4. Sidecar files are created:
   - `<name>.desk.json` with `hidden: false`
   - `<name>.pipe.json` with empty links array
5. Pipe runs HEAD only → produces `<name>.png`
6. Project appears in tree

### Hiding a Project

1. User presses **-** next to project name
2. `desk.json` is updated: `hidden: true`
3. Project disappears from tree
4. Files remain on disk

### Adding a Link

1. User expands project tree
2. User presses **+** next to project name
3. New Link "New Link" is added to pipe BODY
4. Pipe runs HEAD → BODY → TAIL
5. Output PNG updates in Work Area

### Renaming a Link

1. User presses **✎** next to Link name
2. Name becomes editable inline
3. User types new name, presses Enter
4. `pipe.json` is updated automatically

### Editing a Link

1. User selects Link in tree
2. Link Editor opens showing 6 modules
3. User adjusts dials
4. Pipe re-runs on dial change
5. Output PNG updates in Work Area
6. `pipe.json` is updated automatically

---

## Pipe Integration

DESK uses the LABS pipe system defined in `LABS/inc/pipe.hpp`.

### Pipeline Stages

| Stage | DESK Trigger |
|-------|--------------|
| **HEAD** | RAW decode on project add |
| **BODY** | Link processing on dial change |
| **TAIL** | PNG output on any change |

### Link Structure

A Link contains 6 modules (see [LABS/doc/pipe.md](../../LABS/doc/pipe.md)):

- Geometric (6 dials)
- Color Correction (3 dials)
- Tone Mapping (5 dials)
- Global Color (3 dials)
- Selective Color (24 dials)
- Detail (4 dials)

**Total: 45 dials per Link**

### Error Handling

If RAW decode fails, DESK displays an error indication in the UI. The project remains in the tree but shows an error state.

---

## Supported Formats

### Input (RAW)

| Format | Extension | Status |
|--------|-----------|--------|
| Sony ARW | `.ARW` | Supported |
| Others | — | Future |

The file import dialog filters for Sony ARW files only.

### Output

| Format | Extension | Description |
|--------|-----------|-------------|
| PNG | `.png` | Lossless output from pipe TAIL |

---

## Future Considerations

- Undo support for dial changes and Link operations (not yet implemented)
- Additional RAW format support

See [idea.md](./idea.md) for enhancement ideas that require LABS changes.

---

## Documentation

DESK documentation follows [LABS/doc/docs.md](../../LABS/doc/docs.md) standards.

Additional DESK-specific rules are in [DESK/doc/docs.md](./docs.md).
