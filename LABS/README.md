# PQTR:LABS

[back](../README.md)

WebAssembly application for PQTR. Login and account management interface.

## How LABS Works

LABS is the browser-based frontend for PQTR. It compiles C++ to WebAssembly and runs in any modern browser.

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

### Data Flow

1. **User opens** `labs.html` in browser
2. **Browser loads** `labs.js` (Emscripten runtime)
3. **JS fetches** `labs.wasm` and instantiates it
4. **ImGui renders** to WebGL canvas at 60fps
5. **User actions** trigger REST calls to BASE server
6. **BASE responds** with auth tokens, profile data, etc.

### Login Flow

```
User ──► Email/Password ──► BASE/auth ──► JWT Token ──► Local Storage
                                              │
                                              ▼
                              Future: Load profiles, sync settings
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
