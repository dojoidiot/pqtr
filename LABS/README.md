# PQTR:LABS

[back](../README.md)

WebAssembly application for PQTR. The anywhere, anytime interface for creating and editing vibes.

## How LABS Works

LABS is the browser-based frontend for PQTR. It compiles C++ to WebAssembly and runs in any modern browser. All communication between components happens through a note-driven event system.

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
| **LABS** | Pipeline list, file selection, current RAW |
| **Note** | Event log showing all notes (debug view) |
| **Tune** | Pipeline stages: GEAR → LUTE → DRUM → DONE |

### Note System

All communication happens through notes. Components post events, others listen and respond. No shared state beyond the note queue.

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
| `push.error` | message | Push failed |
| `pipe.select` | pipe_name | Pipeline selected |
| `tune.start` | pipe_name | Tune button clicked |
| `tune.begin` | pipe_name | Tune pipeline starting |
| `tune.done` | pipe_name | Tune pipeline complete |
| `auth.expired` | - | JWT invalid, return to login |

### Tune Pipeline

When user clicks Tune, this pipeline executes:

```
gear.load    → Load RAW file
wgpu.open    → Init GPU
pipe.view    → GEAR stage image
lute.tune    → Apply camera profile
pipe.view    → LUTE stage image
drum.tune    → Apply dynamic range
pipe.view    → DRUM stage image (= DONE)
pipe.make    → Create tune.json
pipe.save    → Create final PNG
wgpu.shut    → Shutdown GPU
```

### Data Flow

1. **User opens** `labs.html` in browser
2. **Browser loads** `labs.js` (Emscripten runtime)
3. **JS fetches** `labs.wasm` and instantiates it
4. **ImGui renders** to WebGL canvas at 60fps
5. **User actions** post notes, handlers respond
6. **BASE calls** are triggered by note handlers
7. **State cleared** after each operation completes

### Login Flow

```
User ──► Email ──► OTP ──► BASE/verify ──► JWT ──► localStorage
                                             │
                                             ▼
                                  labs.open note ──► list pipelines
```

### BASE Storage

```
BASE/var/LABS/<itag>/pipe/<raw_name>/
├── <raw_name>.ARW           # Original RAW
├── <raw_name>.png           # Gear view (embedded preview)
├── <raw_name>.tune.json     # Tune sidecar
└── <raw_name>.tune.png      # Final output
```

## Building

```bash
# From repository root
make           # Wire + build + deploy to BASE/www

# Or from LABS directory
make -f Makefile.wasm
make -f Makefile.wasm deploy
```

## Output

Built files are deployed to `BASE/www/`:
- `labs.html` - Entry point
- `labs.js` - Emscripten glue code
- `labs.wasm` - WebAssembly binary

## Testing

```bash
cd BASE/www && python3 -m http.server 8080
# Open http://localhost:8080/main.html
```

## Structure

```
LABS/
├── lib/
│   ├── imgui/            # ImGui library
│   ├── glfw/             # GLFW (Emscripten maps to JS)
│   └── ImGuiFileDialog/  # File dialog
├── src/wasm/
│   ├── login.cpp         # Login/account UI
│   └── shell.html        # HTML wrapper
├── tmp/wasm/             # Build output (gitignored)
└── Makefile.wasm
```
