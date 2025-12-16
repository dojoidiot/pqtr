# PQTR:LABS

[back](../README.md)

WebAssembly application for PQTR. The anywhere, anytime interface for creating and editing vibes.

## How LABS Works

LABS is the browser-based frontend for PQTR. It compiles C++ to WebAssembly and runs in any modern browser. Implements the [PIPE](doc/pipe.md) processing model.

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Browser                                                     │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  labs.html                                             │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐   │  │
│  │  │  labs.wasm  │──│   labs.js   │──│   WebGL/GPU  │   │  │
│  │  │  (ImGui)    │  │ (Emscripten │  │   Canvas     │   │  │
│  │  │             │  │   glue)     │  │              │   │  │
│  │  └─────────────┘  └─────────────┘  └──────────────┘   │  │
│  └───────────────────────────────────────────────────────┘  │
│                            │                                 │
│                       HTTP/REST                              │
└────────────────────────────┼────────────────────────────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │      BASE       │
                    │  (API server)   │
                    └─────────────────┘
```

### Components

| Component | Role |
|-----------|------|
| **ImGui** | Immediate mode GUI - buttons, text, layouts |
| **GLFW** | Window/input abstraction (Emscripten maps to browser) |
| **Emscripten** | C++ → WebAssembly compiler + JS bindings |
| **BASE** | Backend server handling auth, storage, API |

### Panes

| Pane | Purpose |
|------|---------|
| **LABS** | Pipeline list, file browser, pipe.json tree view |
| **Note** | Event log showing all notes (debug view) |
| **Tune** | Pipeline stages: HEAD → LUTE → DRUM → DIFF |

### Note System

All communication happens through notes. Components post events, others listen and respond.

```cpp
postNote("event.name", "data");     // Post an event
checkNote("event.name");            // Check and consume an event
```

| Note | Data | When |
|------|------|------|
| `labs.open` | itag | User logs in |
| `labs.list` | count/error | List request completes |
| `raws.load` | filename | RAW file loaded from disk |
| `push.start` | filename | Push to BASE begins |
| `push.done` | base_name | Push succeeded |
| `pipe.select` | pipe_name | Pipeline selected |
| `tune.start` | pipe_name | Tune button clicked |
| `tune.done` | pipe_name | Tune pipeline complete |
| `drop.done` | pipe_name | Pipe deleted from server |

### Data Flow

1. **User opens** `labs.html` in browser
2. **Browser loads** `labs.js` (Emscripten runtime)
3. **JS fetches** `labs.wasm` and instantiates it
4. **ImGui renders** to WebGL canvas at 60fps
5. **User actions** post notes, handlers respond
6. **BASE calls** are triggered by note handlers

### Login Flow

```
User ──► Email ──► OTP ──► BASE/verify ──► JWT ──► localStorage
                                             │
                                             ▼
                                  labs.open note ──► list pipelines
```

## BASE Storage

```
BASE/var/LABS/<itag>/
├── gear/                           # LUTE tune data per gear/style
│   └── <gear name>/
│       └── <style name>.lute.json
└── pipe/                           # User's pipe projects
    └── <basename>/
        ├── <basename>.ARW          # Source RAW
        ├── <basename>.pipe.json    # Pipe configuration
        ├── <basename>.0.jpg        # On-camera JPEG
        ├── <basename>.png          # Final output
        ├── <basename>.head.png     # HEAD stage
        ├── <basename>.lute.png     # LUTE stage
        ├── <basename>.drum.png     # DRUM stage
        └── <basename>.diff.png     # Diff image
```

## Building

```bash
# From LABS directory
make           # Build WASM app to tmp/wasm/

# From repository root
make           # Build LABS + BASE, pack to tmp/pack/
bash test.sh   # Build and run local server
```

## Output

Built files go to `tmp/wasm/`:
- `labs.html` - Entry point
- `labs.js` - Emscripten glue code
- `labs.wasm` - WebAssembly binary

## Testing

```bash
# From repository root
bash test.sh   # Builds and starts server on http://127.0.0.1:4040
```

## Structure

```
LABS/
├── inc/                  # Public headers
│   ├── gear.hpp          # RAW decoding
│   ├── pipe.hpp          # Pipeline types
│   └── lute.hpp          # Camera profiles
├── lib/
│   ├── imgui/            # ImGui library
│   ├── glfw/             # GLFW (Emscripten maps to JS)
│   └── dawn/             # WebGPU implementation
├── src/
│   ├── main/
│   │   ├── gear/         # GEAR - RAW decoding
│   │   ├── lute/         # LUTE - Camera profiles
│   │   ├── pipe/         # PIPE - Pipeline coordination
│   │   └── labs/         # LABS - UI and app logic
│   │       ├── part/     # State, REST, tasks, notes
│   │       └── view/     # Login, OTP, desktop screens
│   ├── html/             # HTML shell template
│   └── test/             # Tests
├── doc/
│   └── pipe.md           # PIPE processing model
├── tmp/                  # Build output (gitignored)
└── Makefile
```

## See Also

- [PIPE](doc/pipe.md) - Processing pipeline model
- [BASE](../BASE/README.md) - Backend server
