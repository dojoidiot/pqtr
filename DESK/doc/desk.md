# DESK

[back](../README.md)

DESK is a graphical project management interface for LABS RAW image processing.

---

## Directory Structure

```
DESK/
├── bin/
│   └── desk                    # Executable
├── doc/
│   ├── desk.md                 # This document
│   ├── docs.md                 # Documentation standards
│   └── idea.md                 # Future enhancement ideas
├── inc/
│   └── desk.hpp                # Public API (unused - DESK is standalone)
├── lib/
│   ├── glfw/                   # GLFW library (windowing)
│   ├── imgui/                  # Dear ImGui (UI)
│   ├── ImGuiFileDialog/        # File dialog extension
│   └── LABS.a -> ../LABS/lib/labs.a
├── src/
│   └── main/
│       ├── desk.cpp            # Entry point + main loop
│       └── part/
│           ├── state.hpp/cpp   # Application state
│           ├── theme.hpp/cpp   # UI styling
│           ├── files.hpp/cpp   # File I/O + LABS integration
│           ├── projects.hpp/cpp # Pipe panel
│           ├── workarea.hpp/cpp # Image display
│           └── linkeditor.hpp/cpp # Dial editor
├── tmp/                        # Build artifacts
├── var/                        # Default project folder
└── Makefile.desk               # Build system
```

---

## Build

```bash
cd DESK
make -f Makefile.desk
```

**Dependencies:**
- LABS library (`../LABS/lib/labs.a`)
- OpenCV (from `../LABS/lib/opencv/build`)
- OpenGL, X11, Xrandr, Xi, Xcursor, Xinerama

**Output:** `bin/desk`

**Run:** `./bin/desk [project_folder]`

---

## Architecture

### Stack

| Layer | Technology |
|-------|------------|
| Window | GLFW 3.3 |
| Graphics | OpenGL 3.3 Core |
| UI | Dear ImGui |
| RAW Processing | LABS pipe |
| Image | OpenCV UMat (GPU) |

### Application State

All state is centralized in `desk::State` (`state.hpp:110`).

```cpp
struct State {
    // Folders
    std::filesystem::path project_folder;    // Where projects live
    std::filesystem::path raw_source_folder; // Where RAWs are imported from
    bool project_folder_set;

    // Data
    std::vector<Project> projects;
    Selection selection;
    PanelVisibility panels;

    // Textures
    Texture texture;           // Main preview
    Texture embedded_texture;  // Camera JPEG
    bool has_embedded;

    // Metadata
    Info raw_info;

    // Render
    int working_size;          // 512, 1024, 2048, 4096, 0 (full)

    // Status
    bool needs_refresh;
    bool needs_reprocess;
    bool needs_export;
    bool is_working;
    std::string status_message;
    std::string error_message;
};
```

### Project Structure

Each RAW file has three sidecar files:

```cpp
struct Project {
    std::string name;                // Stem of filename
    std::filesystem::path raw_path;  // DSC00144.ARW
    std::filesystem::path desk_path; // DSC00144.desk.json
    std::filesystem::path pipe_path; // DSC00144.pipe.json
    std::filesystem::path png_path;  // DSC00144.png (export)

    bool hidden;
    bool expanded;
    std::string decoder;             // "sony_arw2"
    std::vector<Link> links;
};
```

### Link Structure

A Link contains 6 modules with 45 total dials:

```cpp
struct Link {
    std::string name;
    bool editing_name;

    Module geometric;        // 6 dials (hidden from UI)
    Module color_correction; // 3 dials
    Module tone_mapping;     // 5 dials
    Module global_color;     // 3 dials
    Module selective_color;  // 24 dials
    Module detail;           // 4 dials
};

struct Module {
    std::string name;
    std::map<std::string, Dial> dials;  // Dial = float (0.0-1.0)
};
```

---

## Modules and Dials

### Dial Values

All dials are normalized floats in range `[0.0, 1.0]`. Default is `0.5` (neutral) except where noted.

### Geometric (6 dials)

| Dial | Key | Default | Range |
|------|-----|---------|-------|
| Crop Top | `crop_top` | 0.0 | 0.0-1.0 |
| Crop Right | `crop_right` | 0.0 | 0.0-1.0 |
| Crop Bottom | `crop_bottom` | 0.0 | 0.0-1.0 |
| Crop Left | `crop_left` | 0.0 | 0.0-1.0 |
| Scale | `scale` | 0.5 | 0.0-1.0 |
| Tilt Angle | `tilt_angle` | 0.5 | 0.0-1.0 |

**Note:** Geometric is hidden from the Link Editor UI.

### Color Correction (3 dials)

| Dial | Key | Default |
|------|-----|---------|
| Exposure | `exposure` | 0.5 |
| Temperature | `temperature` | 0.5 |
| Tint | `tint` | 0.5 |

### Tone Mapping (5 dials)

| Dial | Key | Default |
|------|-----|---------|
| Contrast | `contrast` | 0.5 |
| Highlights | `highlights` | 0.5 |
| Shadows | `shadows` | 0.5 |
| Black | `black` | 0.15 |
| White | `white` | 0.85 |

### Global Color (3 dials)

| Dial | Key | Default |
|------|-----|---------|
| Vibrance | `vibrance` | 0.5 |
| Saturation | `saturation` | 0.5 |
| Color Density | `color_density` | 0.5 |

### Selective Color (24 dials)

8 colors × 3 HSL adjustments. All default to `0.5`.

| Color | Hue Key | Saturation Key | Luminance Key |
|-------|---------|----------------|---------------|
| Red | `red_hue` | `red_saturation` | `red_luminance` |
| Orange | `orange_hue` | `orange_saturation` | `orange_luminance` |
| Yellow | `yellow_hue` | `yellow_saturation` | `yellow_luminance` |
| Green | `green_hue` | `green_saturation` | `green_luminance` |
| Cyan | `cyan_hue` | `cyan_saturation` | `cyan_luminance` |
| Blue | `blue_hue` | `blue_saturation` | `blue_luminance` |
| Purple | `purple_hue` | `purple_saturation` | `purple_luminance` |
| Magenta | `magenta_hue` | `magenta_saturation` | `magenta_luminance` |

### Detail (4 dials)

| Dial | Key | Default |
|------|-----|---------|
| Sharpen Amount | `sharpen_amount` | 0.0 |
| Sharpen Radius | `sharpen_radius` | 0.4 |
| Denoise Luminance | `denoise_luminance` | 0.0 |
| Denoise Chroma | `denoise_chroma` | 0.0 |

---

## File Formats

### desk.json

DESK project settings.

```json
{
  "version": "1.0",
  "hidden": false
}
```

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | Schema version |
| `hidden` | bool | If true, project not shown in UI |

### pipe.json

LABS pipe configuration. See `save_pipe_json()` in `files.cpp:244`.

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": [
    {
      "name": "Link 1",
      "modules": {
        "geometric": { ... },
        "color_correction": { ... },
        "tone_mapping": { ... },
        "global_color": { ... },
        "selective_color": { ... },
        "detail": { ... }
      }
    }
  ],
  "tail": {
    "output": "DSC00144.png"
  }
}
```

---

## User Interface

### Window Structure

```
┌──────────────────────────────────────────────────────────┐
│ RAW: [Dropdown▼] Import RAW | Pipe Info Editor Embedded | Preview: [1024▼] Export │
├──────────────────────────────────────────────────────────┤
│ ┌─────────────┐                          ┌─────────────┐ │
│ │ Pipe Panel  │                          │  Embedded   │ │
│ │ (top-left)  │                          │ (top-right) │ │
│ └─────────────┘                          └─────────────┘ │
│                                                          │
│                     [Processed Image]                    │
│                                                          │
│ ┌─────────────┐                          ┌─────────────┐ │
│ │  RAW Info   │                          │ Link Editor │ │
│ │(bottom-left)│                          │(bottom-right)│ │
│ └─────────────┘                          └─────────────┘ │
└──────────────────────────────────────────────────────────┘
```

### Menu Bar (`desk.cpp:35`)

| Element | Description |
|---------|-------------|
| RAW Dropdown | Select active project |
| Import RAW | Opens file dialog for ARW files |
| Pipe/Info/Editor/Embedded | Toggle panel visibility |
| Preview | Size selector (512/1024/2048/4096/Full) |
| Export | Render full resolution PNG |

### Panels

| Panel | Position | Size | Content |
|-------|----------|------|---------|
| Pipe | top-left | 300×400 | Links tree view |
| RAW Info | bottom-left | 280×250 | EXIF metadata table |
| Link Editor | bottom-right | 800×180 | Module/dial equalizer |
| Embedded | top-right | 400×300 | Camera JPEG preview |

Panel constants defined in `theme.hpp:16-28`.

### Link Editor (`linkeditor.cpp`)

Three-level breadcrumb menu:

```
Module: [Color Correction] > [Exposure] = 0.50
```

For Selective Color, adds fourth level:

```
Module: [Selective Colour] > [Red] > [Hue] = 0.50
```

**Slider States:**

| Color | State | Condition |
|-------|-------|-----------|
| Grey | Default | Value equals default |
| White | Set | Value modified from default |
| Blue | Active | Currently selected slider |

---

## LABS Integration

### Rendering Pipeline (`files.cpp:472`)

**Preview render:**

```cpp
pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(raw_path));
pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));

// Scale BEFORE processing for speed
pipe::Body& body = head->body(working_size);

// Add links with dial values
for (const auto& link : project.links) {
    pipe::Body::Link& pipe_link = body.add(link.name);
    apply_link_dials(link, pipe_link);
}

// Get display-ready BGR image
pipe::View display = body.view();

// Upload to OpenGL texture
cv::Mat cpu;
display.copyTo(cpu);
upload_texture(state, cpu);
```

**Export render:**

```cpp
// Same setup, but no working_size scaling
pipe::Body& body = head->body();  // Full resolution

// ... add links ...

// Save to PNG file
body.tail().save(png_path, 0);  // 0 = full resolution
```

### Dial Application (`files.cpp:377`)

`apply_link_dials()` maps DESK dial values to LABS pipe API:

```cpp
dst.colorCorrection().exposure().set(exposure_value);
dst.colorCorrection().whiteBalance().temperature(temp_value);
dst.toneMapping().contrast().set(contrast_value);
dst.globalColor().vibrance().set(vibrance_value);
dst.selectiveColour().red().hue(red_hue_value);
dst.detail().sharpen().amount(sharpen_value);
// ... etc
```

---

## Data Flow

### Project Selection

```
User selects project
    ↓
load_raw_info() → state.raw_info
load_embedded_preview() → state.embedded_texture
state.is_working = true, state.needs_reprocess = true
    ↓
[Next frame]
render_to_texture() → state.texture
state.is_working = false
```

### Dial Change

```
User adjusts slider
    ↓
set_dial_value() → Link dials updated
state.needs_reprocess = true
state.is_working = true
    ↓
render_to_texture() at working_size
    ↓
[On drag release]
save_pipe_json() → disk
```

### Export

```
User clicks Export
    ↓
state.needs_export = true
    ↓
export_project()
    ↓
Full resolution render through LABS
    ↓
body.tail().save() → PNG file
```

---

## Source Files

### desk.cpp (606 lines)

Main entry point and render loop.

| Function | Line | Purpose |
|----------|------|---------|
| `glfw_error_callback` | 27 | Error handler |
| `render_menu_bar` | 35 | Top toolbar |
| `render_image_background` | 171 | Work area container |
| `render_floating_panels` | 227 | Panel layout and resize |
| `process_file_dialogs` | 405 | ImGuiFileDialog handling |
| `handle_selection_change` | 437 | Project/link change logic |
| `main` | 499 | App initialization |

### state.hpp/cpp

State structures and module initialization.

| Type | Purpose |
|------|---------|
| `Dial` | `float` alias |
| `Info` | `map<string,string>` metadata |
| `Module` | Named dial collection |
| `Link` | 6 modules container |
| `Project` | RAW + sidecars |
| `PanelVisibility` | 4 panel flags |
| `Texture` | OpenGL texture handle |
| `Selection` | Current project/link/module/dial |
| `State` | Full application state |

### files.cpp (738 lines)

File I/O and LABS integration.

| Function | Line | Purpose |
|----------|------|---------|
| `scan_projects` | 79 | Find ARW files in folder |
| `load_desk_json` | 131 | Parse desk.json |
| `save_desk_json` | 139 | Write desk.json |
| `load_pipe_json` | 148 | Parse pipe.json |
| `save_pipe_json` | 244 | Write pipe.json |
| `create_project` | 341 | Copy RAW + create sidecars |
| `apply_link_dials` | 377 | Map dials to LABS API |
| `upload_texture` | 435 | cv::Mat → OpenGL |
| `render_to_texture` | 472 | Preview render |
| `export_project` | 519 | Full-res export |
| `load_texture` | 563 | PNG → OpenGL |
| `load_embedded_preview` | 613 | RAW JPEG → OpenGL |
| `load_raw_info` | 685 | EXIF metadata |

### projects.cpp (412 lines)

Pipe panel rendering.

| Function | Purpose |
|----------|---------|
| `render_workspace_panel` | Project list (legacy) |
| `render_pipe_panel` | Links tree with modules |
| `render_info_panel` | Metadata table |

### workarea.cpp (89 lines)

Image display with aspect ratio preservation and centering.

### linkeditor.cpp (459 lines)

Equalizer-style dial editor.

| Function | Purpose |
|----------|---------|
| `get_dial_default` | Default value per dial |
| `get_dial_value` | Read from Link |
| `set_dial_value` | Write to Link |
| `custom_vslider` | Vertical slider widget |
| `render_module_menus` | Main editor UI |

### theme.cpp (77 lines)

Modern Dark theme colors for ImGui.

---

## Constraints

- **Read-only LABS**: DESK consumes LABS as-is
- **No file deletion**: Projects are hidden, not removed
- **Single selection**: One project, one link at a time
- **Auto-save**: Sidecar files update on every change
- **Sony ARW only**: File dialog filters for `.ARW` files

---

## Supported Formats

**Input:** Sony ARW (`.ARW`)

**Output:** PNG (`.png`)
