# PQTR:LABS

[back](../README.md)

WebAssembly application for PQTR. Login and account management interface.

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
