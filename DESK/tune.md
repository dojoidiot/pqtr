# Tune Desktop Application Specification

Version: 2.0
Status: Specification for reconstruction in DESK project
Original: `wip/dev/unit/tune`

## Overview

Tune is a desktop GUI application for RAW photo processing. It provides a visual interface for loading RAW files (with optional reference JPEG pairing), applying processing modules through a pipeline architecture, and tuning parameters to match camera-generated output or achieve creative looks.

## Application Purpose

The Tune desktop application serves as a visual workbench for:

1. **Project Management**: Creating and managing tune projects that pair RAW files with reference JPEGs
2. **Pipeline Visualization**: Displaying real-time pipeline output alongside reference images for comparison
3. **Module Configuration**: Interactive UI for enabling/disabling processing modules and organizing them into edit steps
4. **Settings Persistence**: Saving and loading sidecar JSON files that store all processing parameters

## Architecture

### Technology Stack

- **Language**: C++17
- **GUI Framework**: ImGui (Immediate Mode GUI)
- **Windowing**: GLFW 3.3 (OpenGL 3.3 Core Profile)
- **Graphics API**: OpenGL 3.3 with GLSL 330
- **Image Processing**: OpenCV (UMat GPU pipeline)
- **File Dialogs**: ImGuiFileDialog

### Dependencies

#### Core Libraries

1. **ImGui** (github.com/ocornut/imgui)
   - Core files:
     - `imgui.cpp` - Core ImGui implementation
     - `imgui_draw.cpp` - Rendering primitives
     - `imgui_tables.cpp` - Table widgets
     - `imgui_widgets.cpp` - Standard widgets
   - Backend files:
     - `backends/imgui_impl_glfw.cpp` - GLFW platform backend
     - `backends/imgui_impl_opengl3.cpp` - OpenGL3 renderer backend
   - Headers:
     - `imgui.h` - Public API
     - `imconfig.h` - Configuration (optional customization)

2. **GLFW** (github.com/glfw/glfw)
   - Version: 3.x
   - Purpose: Cross-platform window creation and input handling
   - Build: CMake-based, produces `libglfw3.a` static library
   - Required headers: `GLFW/glfw3.h`

3. **ImGuiFileDialog** (github.com/aiekick/ImGuiFileDialog)
   - Files:
     - `ImGuiFileDialog.cpp` - File dialog implementation
     - `ImGuiFileDialog.h` - Public API
     - `ImGuiFileDialogConfig.h` - Configuration options
   - Dependencies:
     - `dirent/` - Cross-platform directory iteration
     - `stb/` - Image loading for thumbnails (optional)

4. **OpenCV**
   - Modules used:
     - `core` - Mat/UMat, basic data structures
     - `imgproc` - Image processing operations
     - `imgcodecs` - Image file I/O (PNG, JPEG)
   - GPU Acceleration: Uses UMat for GPU-based processing
   - Build location: `lib/opencv/build/lib`

#### Pipeline Dependencies (WIP-specific, to be adapted)

The current implementation depends on a custom pipeline architecture:

- **pipe library** (`libpipe.a`): Core pipeline orchestration
  - `pipe.h` - Main pipeline class
  - `pipe/head.h` - RAW file loading and metadata extraction
  - `pipe/module.h` - Module registry and base classes
- **Processing modules**: Individual image processing units
  - Located in `wip/dev/part/mods/*/`
  - Examples: blc, wb_gain, demosaic, cst, gamma_oetf, quantize, io_write, normalize, image_analyser
- **Data structures**: Project and settings serialization
  - `tune_data.h` - Tune project metadata (tune.json format)
  - `sidecar.h` - Processing settings (RAW sidecar JSON format)
  - `sidecar.cpp` - JSON serialization/deserialization

## User Interface Layout

### Window Configuration

- **Default size**: 1280x720
- **Title**: "Tune - Labs RAW Processor"
- **Style**: Dark theme (`ImGui::StyleColorsDark()`)
- **Layout**: Fixed web-page style (no floating/docking windows)

### Main Menu Bar

Located at top of window, contains:

#### File Menu
- **New Project...** (Ctrl+N): Opens RAW file selection dialog
- **Open Project...** (Ctrl+O): Opens project folder selection dialog
- **Exit** (Ctrl+Q): Closes application

#### Status Display
- Shows currently loaded project name when a project is active
- Format: `| Project: {project_name}`

### Three-Panel Layout (Active when project loaded)

The UI uses a fixed three-panel layout that resembles a web application:

```
+-------------------+----------------------------------------+
| Menu Bar          |                                        |
+-------------------+----------------------------------------+
| LEFT PANEL        | CENTER PANEL                           |
| (350px wide)      | (Remaining width)                      |
|                   |                                        |
| +---------------+ | +------------------------------------+ |
| | PREP Modules  | | | Image Viewer                       | |
| | (40% height)  | | | - Side-by-side comparison if JPEG  | |
| |               | | | - Single view if no reference      | |
| +---------------+ | +------------------------------------+ |
| +---------------+ |                                        |
| | Edit Steps    | |                                        |
| | (60% height)  | |                                        |
| |               | |                                        |
| +---------------+ |                                        |
+-------------------+----------------------------------------+
```

#### Left Panel (350px fixed width)

**PREP Modules Panel** (Top 40%):
- Header: "PREP Modules" (cyan color)
- Always-enabled modules (grayed out, disabled checkboxes):
  - RAW Prepare
  - Demosaic
- Separator
- "Optional Modules:" label
- User-configurable checkboxes:
  - Temperature (White Balance)
  - Color Input (Color Matrix)
  - Flip/Rotate
- "Save Settings" button (full width)

**Edit Steps Panel** (Bottom 60%):
- Header: "Edit Steps" (cyan color)
- "+ Add Step" button (full width)
- Dynamic list of edit steps (collapsible tree nodes):
  - Each step shows: `[TreeNode] {step_name} [Drop]`
  - When expanded, shows module checkboxes:
    - Exposure
    - Highlights
    - Channel Mixer RGB
    - Color Balance RGB
    - RGB Primaries
    - Color Output
    - Sigmoid
    - Gamma
    - Sharpen
    - Saturation
    - Vignette
    - Tone Equalizer

#### Center Panel (Remaining width)

**Image Viewer**:

Two display modes:

1. **Side-by-side mode** (when reference JPEG loaded):
   - Left side: "Pipeline Output" (cyan label)
   - Right side: "Reference JPEG" (green label)
   - 10px gap between images
   - Both images scale proportionally to fit half-width

2. **Single image mode** (no reference):
   - "Pipeline Output" label only
   - Image scales to fit available space (aspect ratio preserved)

Image rendering:
- OpenGL textures created from pipeline output (RGB float32 format)
- Auto-scaling to fit panel while preserving aspect ratio
- No zoom/pan controls in current version

### Dialog Windows

#### 1. New Project Dialog (RAW File Selection)

- **Title**: "New Project - Select RAW File"
- **Size**: 800x600 (first use)
- **Type**: ImGuiFileDialog modal
- **Default path**: Configured via `tune.ini` → `var_pics` (defaults to `../var/pics`)
- **File filter**: `.ARW,.arw` (Sony RAW format)
- **Behavior**:
  - On selection: Triggers automatic JPEG pairing logic
  - Shows pairing status window after RAW selection

#### 2. RAW/JPEG Pairing Window

- **Title**: "RAW/JPEG Pairing"
- **Type**: Auto-resize window
- **Contents**:
  - RAW File: Shows filename in green
  - Separator
  - JPEG File: Shows filename (green if found, red "JPEG not found" if missing)
  - "Browse..." button: Opens JPEG selection dialog for manual pairing
  - Separator
  - "OK" button (120px wide): Enabled only when both files selected
    - On click: Opens project location dialog
  - "(Need both files)" warning (orange) if pairing incomplete
  - "Cancel" button (120px wide): Clears pairing state

JPEG pairing logic:
- Automatic: Searches same directory for matching basename with extensions: `.JPG`, `.jpg`, `.JPEG`, `.jpeg`
- Manual: User can browse to select any JPEG file

#### 3. Project Location Dialog

- **Title**: "Create Project"
- **Size**: 600x200 (no resize)
- **Position**: Centered on screen
- **Contents**:
  - "Choose project location and name:"
  - Separator
  - "Project Name:" text input (pre-filled with RAW basename)
  - "Location:" label + path display (gray text)
    - Default: Configured via `tune.ini` → `var_tune` (defaults to `../var/tune`)
  - Separator
  - "Full path:" preview (green text): `{location}/{project_name}`
  - Separator
  - "Create" button (120px, disabled if name empty):
    - Creates project folder: `{location}/{project_name}/`
    - Saves `tune.json` with project metadata
    - Loads RAW file into pipeline
    - Loads reference JPEG into texture
    - Saves as last project (`.last_project` file)
  - "Cancel" button (120px): Closes dialog without creating

#### 4. Add Edit Step Dialog

- **Title**: "Add Edit Step"
- **Size**: 400x150 (no resize)
- **Position**: Centered on screen
- **Contents**:
  - "Enter step name:" text input
  - Separator
  - "Create" button (120px, disabled if name empty):
    - Creates new edit step with all modules disabled by default
    - Adds to edit steps list
    - Updates pipeline
  - "Cancel" button (120px): Closes dialog

#### 5. Open Project Dialog

- **Title**: "Open Project"
- **Size**: 800x600 (first use)
- **Type**: ImGuiFileDialog modal (directory selection mode)
- **Default path**: `{var_tune}` directory
- **Behavior**:
  - User selects project folder
  - Loads `tune.json` to validate and restore state
  - Loads RAW file path from `tune.json`
  - Loads reference JPEG if specified
  - Saves as last project

## Application State

### AppState Structure

The application maintains a single global state structure:

```cpp
struct AppState {
    // Project state
    std::string raw_path;           // Selected RAW file (during pairing)
    std::string jpeg_path;          // Selected JPEG file (during pairing)
    std::string project_path;       // Current project folder path
    std::string project_name;       // Project display name
    bool has_pair;                  // RAW+JPEG pairing complete
    bool project_loaded;            // Project successfully loaded
    bool show_location_dialog;      // Show create project dialog

    // Pipeline state
    pipe::Pipe pipeline;            // Main processing pipeline
    pipe::head::Data raw_data;      // Loaded RAW data + metadata
    std::string current_raw_path;   // Path to loaded RAW (for sidecar saving)
    bool pipeline_ready;            // Pipeline initialized and ready

    // PREP module toggles
    bool prep_temperature;          // Temperature (white balance) enabled
    bool prep_colorin;              // Color input (color matrix) enabled
    bool prep_flip;                 // Flip/rotate enabled
    // Note: demosaic and rawprepare always enabled (not user-configurable)

    // Edit steps (dynamic list)
    struct EditStepState {
        std::string name;                           // Step name
        std::map<std::string, bool> module_enabled; // module_name -> enabled
    };
    std::vector<EditStepState> edit_steps;
    bool show_add_step_dialog;
    char new_step_name[64];

    // OpenGL textures for display
    GLuint texture_id;              // Pipeline output texture
    int texture_width;
    int texture_height;
    GLuint ref_texture_id;          // Reference JPEG texture
    int ref_texture_width;
    int ref_texture_height;

    // Constructor
    AppState(pipe::ModuleRegistry* registry);
};
```

### State Transitions

1. **Application Launch**:
   - Initializes GLFW, OpenGL, ImGui
   - Sets up module registry
   - Auto-loads last project if exists (from `.last_project` file)

2. **New Project Flow**:
   ```
   User clicks "New Project"
   → RAW file dialog opens
   → User selects RAW file
   → Automatic JPEG pairing attempted
   → Pairing window shown
   → User confirms/adjusts pairing
   → Project location dialog shown
   → User enters project name
   → Project folder created
   → tune.json saved
   → RAW loaded into pipeline
   → Reference JPEG loaded as texture
   → Main UI displayed
   ```

3. **Open Project Flow**:
   ```
   User clicks "Open Project"
   → Folder selection dialog opens
   → User selects project folder
   → tune.json loaded and validated
   → RAW path extracted from tune.json
   → RAW loaded into pipeline
   → Sidecar loaded (if exists)
   → Reference JPEG loaded (if specified)
   → Main UI displayed
   ```

4. **Pipeline Updates**:
   ```
   User toggles PREP checkbox
   → updatePipeline() called
   → Pipeline re-runs with new parameters
   → Output texture updated
   → UI re-renders

   User adds/modifies Edit step
   → updatePipeline() called
   → Pipeline clears old steps
   → New steps built from UI state
   → Pipeline runs edit chain
   → Output texture updated
   → UI re-renders
   ```

5. **Save Settings**:
   ```
   User clicks "Save Settings"
   → saveSidecar() called
   → Sidecar struct populated from AppState
   → JSON serialized
   → Saved to {raw_basename}.json
   ```

## Data Formats

### tune.json (Project Metadata)

Location: `{project_folder}/tune.json`

```json
{
  "project_name": "DSC00144",
  "raw_path": "/absolute/path/to/DSC00144.ARW",
  "jpeg_path": "/absolute/path/to/DSC00144.JPG",
  "created": "2024-11-20T15:30:45",
  "modified": "2024-11-20T16:22:10"
}
```

Fields:
- `project_name`: Display name (typically RAW basename)
- `raw_path`: Absolute path to RAW file
- `jpeg_path`: Absolute path to reference JPEG (can be empty)
- `created`: ISO 8601 timestamp of project creation
- `modified`: ISO 8601 timestamp of last modification

### Sidecar JSON (Processing Settings)

Location: `{raw_directory}/{raw_basename}.json`
Example: `/photos/DSC00144.json` (for `/photos/DSC00144.ARW`)

```json
{
  "version": "1.0",
  "created": "2024-11-20T15:30:45",
  "modified": "2024-11-20T16:22:10",
  "raw_filename": "DSC00144.ARW",
  "camera_make": "SONY",
  "camera_model": "ILCE-7M3",
  "width": 6000,
  "height": 4000,
  "iso": 400.0,
  "shutter_speed": 0.004,
  "aperture": 5.6,
  "focal_length": 85.0,
  "lens_model": "FE 85mm F1.8",
  "prep_params": {
    "demosaic": {
      "enabled": 1.0
    },
    "rawprepare": {
      "enabled": 1.0
    },
    "temperature": {
      "enabled": 1.0,
      "red": 1.05,
      "green": 1.0,
      "blue": 0.95
    },
    "colorin": {
      "enabled": 1.0
    },
    "flip": {
      "enabled": 0.0
    }
  },
  "edit_steps": [
    {
      "name": "brighten",
      "module_params": {
        "exposure": {
          "enabled": 1.0,
          "ev": 0.5
        },
        "highlights": {
          "enabled": 0.0
        }
      }
    }
  ]
}
```

Key sections:
- **Metadata**: Version, timestamps, RAW filename
- **Camera info**: Make, model, EXIF data
- **PREP params**: Module parameters for PREP pipeline stage
- **Edit steps**: Ordered list of edit steps with per-module parameters

### tune.ini (Configuration File)

Location: `{binary_directory}/tune.ini`

```ini
[Paths]
var_pics = ../var/pics
var_tune = ../var/tune
```

Purpose:
- Configures default directories for file dialogs
- Relative paths are resolved relative to binary location
- Read at runtime via `file_utils::readConfig()`

### .last_project (Auto-reopen)

Location: `{var_tune}/.last_project`

Contains single line with absolute path to last opened project:
```
/absolute/path/to/var/tune/DSC00144
```

Used to automatically reopen last project on application launch.

## Module Architecture

### Module Registry

Global registry that holds all available processing modules:

```cpp
pipe::ModuleRegistry s_module_registry;
```

Populated at startup via `setupModules()`:

```cpp
void setupModules(pipe::ModuleRegistry& registry) {
    // PREP modules
    registry.registerModule("blc", new mods::BLC());
    registry.registerModule("wb_gain", new mods::WBGain());
    registry.registerModule("demosaic", new mods::Demosaic());
    registry.registerModule("cst", new mods::CST());
    registry.registerModule("gamma_oetf", new mods::GammaOETF());
    registry.registerModule("rawloader", new mods::RawLoader());
    registry.registerModule("normalize", new mods::Normalize());
    registry.registerModule("image_analyser", new mods::ImageAnalyser());
    registry.registerModule("quantize", new mods::Quantize());
    registry.registerModule("io_write", new mods::IOWrite());

    // EDIT modules (registered when implemented)
    // registry.registerModule("exposure", new mods::Exposure());
    // registry.registerModule("highlights", new mods::Highlights());
    // ... etc
}
```

### Pipeline Flow

1. **HEAD** (RAW loading):
   ```cpp
   pipe::head::load(raw_path, raw_data);
   pipeline.setInput(raw_data.bayer, raw_data.metadata);
   ```

2. **PREP** (Basic processing):
   ```cpp
   std::map<std::string, mods::Params> prep_params;
   prep_params["demosaic"]["enabled"] = 1.0f;      // Always on
   prep_params["rawprepare"]["enabled"] = 1.0f;    // Always on
   prep_params["temperature"]["enabled"] = state.prep_temperature ? 1.0f : 0.0f;
   prep_params["colorin"]["enabled"] = state.prep_colorin ? 1.0f : 0.0f;
   prep_params["flip"]["enabled"] = state.prep_flip ? 1.0f : 0.0f;

   pipeline.runPrep(prep_params);
   ```

3. **BODY** (Edit steps):
   ```cpp
   pipeline.clearSteps();

   for (const auto& step_state : state.edit_steps) {
       pipe::Step* step = pipeline.addStep(step_state.name);

       for (const auto& [module_name, enabled] : step_state.module_enabled) {
           step->set(module_name, "enabled", enabled ? 1.0f : 0.0f);
       }
   }

   pipeline.runEdit();
   ```

4. **Output**:
   ```cpp
   cv::UMat output = pipeline.getOutput();  // or getPrepOutput() if no edit steps
   updateTexture(state, output);            // Upload to OpenGL texture
   ```

## Rendering Pipeline

### OpenGL Texture Management

Pipeline output is displayed via OpenGL textures:

```cpp
void updateTexture(AppState& state, const cv::UMat& image) {
    // Download from GPU
    cv::Mat cpu_image = image.getMat(cv::ACCESS_READ);

    // Create/bind texture
    if (state.texture_id == 0) {
        glGenTextures(1, &state.texture_id);
        glBindTexture(GL_TEXTURE_2D, state.texture_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Upload RGB float32 data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, cpu_image.cols, cpu_image.rows,
                 0, GL_RGB, GL_FLOAT, cpu_image.data);
}
```

Reference JPEG loading (similar process):
- Load JPEG via `cv::imread()`
- Convert BGR → RGB
- Convert uint8 → float32 [0,1]
- Upload to `ref_texture_id`

### ImGui Display

Images rendered via `ImGui::Image()`:

```cpp
ImGui::Image(
    reinterpret_cast<void*>(static_cast<intptr_t>(state.texture_id)),
    ImVec2(display_width, display_height)
);
```

Aspect ratio calculation:
```cpp
float aspect = (float)texture_width / texture_height;
if (region_width / region_height > aspect) {
    // Fit to height
    display_height = region_height;
    display_width = region_height * aspect;
} else {
    // Fit to width
    display_width = region_width;
    display_height = region_width / aspect;
}
```

## Build System

### Makefile Structure (wip/dev/unit/tune)

Key build parameters:

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Include paths
-I{pipe_inc}           # Pipeline headers
-I{imgui_dir}          # ImGui core
-I{imgui_backends}     # ImGui backends
-I{filedialog_dir}     # ImGuiFileDialog
-I{glfw_include}       # GLFW headers
-I{opencv_includes}    # OpenCV headers

# Libraries
-lpipe                     # Pipeline library
-lopencv_core
-lopencv_imgproc
-lopencv_imgcodecs
{glfw_static}              # libglfw3.a
-lGL -ldl -lpthread        # OpenGL + system
-lX11 -lXrandr -lXi -lXcursor  # X11 windowing

# RPATH for runtime library loading
-Wl,-rpath,'$$ORIGIN/../ref/opencv/build/lib'
```

### Compilation Units

Static compilation (all sources compiled directly):
- ImGui core: 4 files (imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp)
- ImGui backends: 2 files (imgui_impl_glfw.cpp, imgui_impl_opengl3.cpp)
- ImGuiFileDialog: 1 file (ImGuiFileDialog.cpp)
- Application: 1 file (main.cpp)
- Pipeline modules: N files (one per module .cpp)
- Data structures: 1 file (sidecar.cpp)

Total: ~20-30 .cpp files depending on module count

## File Utilities

### file_utils.h

Header-only utilities for path manipulation and configuration:

**Configuration**:
- `readConfig()`: Parses `tune.ini` INI file
- `getBinaryDir()`: Gets executable directory via `/proc/self/exe`
- `getVarPicsPath()`: Resolves `var/pics` directory
- `getVarTunePath()`: Resolves `var/tune` directory

**Path operations**:
- `getFilename(path)`: Extracts filename from path
- `getDirectory(path)`: Extracts directory from path
- `getBasename(path)`: Extracts basename without extension
- `fileExists(path)`: Checks file existence

**RAW/JPEG pairing**:
- `findMatchingJPEG(raw_path)`: Searches for matching JPEG with same basename

**Project management**:
- `createProjectFolder(raw_path)`: Creates project directory structure
- `getProjectPath(basename)`: Builds project path
- `projectExists(basename)`: Checks if project exists
- `saveLastProject(path)`: Saves last opened project
- `loadLastProject()`: Loads last opened project

All use `std::filesystem` for cross-platform path handling.

## Key Behaviors

### Auto-reopen Last Project

On application startup:
1. Reads `{var_tune}/.last_project`
2. If valid project path found, loads `tune.json`
3. Loads RAW file and reference JPEG
4. Loads sidecar settings if exist
5. Displays main UI immediately

### Pipeline Update Triggers

Pipeline is re-run when:
- PREP checkbox toggled
- Edit step added/removed
- Edit step module toggled
- RAW file loaded

Pipeline is NOT re-run when:
- Saving settings
- Navigating UI
- Opening/closing dialogs

### Settings Persistence

Two separate persistence mechanisms:

1. **Project metadata** (`tune.json`):
   - Saved on project creation
   - Contains RAW/JPEG paths
   - Modified timestamp updated on project modification

2. **Processing settings** (sidecar `.json`):
   - Saved manually via "Save Settings" button
   - Auto-loaded when RAW file opened
   - Contains all PREP + Edit step parameters

### Error Handling

Current implementation uses console output for errors:
- `std::cout` for success messages
- `std::cerr` for error messages
- No UI error dialogs (relies on console visibility)

Example:
```cpp
if (!pipe::head::load(raw_path, raw_data)) {
    std::cerr << "Failed to load RAW file" << std::endl;
    return false;
}
std::cout << "✓ Loaded RAW: " << raw_path << std::endl;
```

## ImGui Patterns Used

### Fixed Layout (Web-style)

```cpp
ImGui::SetNextWindowPos(ImVec2(x, y));
ImGui::SetNextWindowSize(ImVec2(width, height));
ImGui::Begin("Window", nullptr,
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoTitleBar);
```

### Child Regions

```cpp
ImGui::BeginChild("##ChildID", ImVec2(width, height), true);
// ... content ...
ImGui::EndChild();
```

### Modal Dialogs

```cpp
// Center on screen
ImVec2 center = ImGui::GetMainViewport()->GetCenter();
ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

ImGui::Begin("Dialog", &show_flag,
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
```

### Conditional UI

```cpp
if (!name_valid) {
    ImGui::BeginDisabled();
}

// ... disabled controls ...

if (!name_valid) {
    ImGui::EndDisabled();
}
```

### Tree Nodes

```cpp
bool open = ImGui::TreeNode("##NodeID", "Display Text");
ImGui::SameLine();
if (ImGui::SmallButton("Action")) { /* ... */ }

if (open) {
    // ... nested content ...
    ImGui::TreePop();
}
```

### Color Coding

```cpp
// Cyan headers
ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Section Header");

// Green success
ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Loaded");

// Red errors
ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Not found");

// Orange warnings
ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning");
```

## Platform Requirements

### Linux (X11)

- X11 development libraries: `libX11`, `libXrandr`, `libXi`, `libXcursor`
- OpenGL 3.3+ capable GPU
- GLFW 3.3+
- C++17 compiler (GCC/Clang)

### Build Dependencies

```bash
# Debian/Ubuntu
apt-get install libx11-dev libxrandr-dev libxi-dev libxcursor-dev
apt-get install libgl1-mesa-dev
apt-get install build-essential cmake

# Fedora
dnf install libX11-devel libXrandr-devel libXi-devel libXcursor-devel
dnf install mesa-libGL-devel
dnf install gcc-c++ cmake
```

## Adaptation Notes for DESK

When reconstructing this application in DESK:

1. **Remove pipeline dependencies**: Replace `libpipe.a` and module system with standalone stubs or alternative architecture
2. **Simplify module registry**: Either mock the registry or create a minimal version for UI demonstration
3. **Keep ImGui/GLFW/OpenCV**: These are the core UI dependencies worth preserving
4. **Preserve UI layout**: The three-panel web-style layout is the key UX feature
5. **Preserve data formats**: Keep `tune.json` and sidecar JSON formats for compatibility
6. **Add to DESK/lib**: Place ImGui, GLFW, ImGuiFileDialog as git submodules
7. **Follow LABS HFS convention**: Use `DESK/bin`, `DESK/inc`, `DESK/lib`, `DESK/src/main` structure

## Glossary

- **PREP**: Preparation pipeline stage (RAW decoding, demosaic, color matrix)
- **BODY**: Edit steps pipeline stage (creative adjustments)
- **Edit Step**: Named collection of enabled modules with parameters
- **Module**: Individual image processing unit (exposure, demosaic, etc.)
- **Sidecar**: JSON file storing processing settings alongside RAW file
- **Pairing**: Association of RAW file with reference JPEG for comparison
- **Pipeline**: Sequential processing chain from RAW input to PNG output
- **UMat**: OpenCV GPU-accelerated matrix (cv::UMat)

## References

- ImGui: https://github.com/ocornut/imgui
- GLFW: https://www.glfw.org/
- ImGuiFileDialog: https://github.com/aiekick/ImGuiFileDialog
- OpenCV: https://opencv.org/
- Original implementation: `wip/dev/unit/tune/`
