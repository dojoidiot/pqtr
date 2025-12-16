# DESK Architecture

Desktop app with tile-based UI. Tiles communicate via notes, not direct state access.

## Structure

```
src/main/labs/
├── desk.hpp          # Types and shared state
├── desk.cpp          # Layout, orchestrates tiles
├── main.cpp          # Entry point
├── part/
│   ├── base.cpp      # BASE server calls (emscripten fetch)
│   ├── note.cpp      # Note system (postNote/checkNote)
│   └── task.cpp      # Task queue
├── pane/
│   └── auth.cpp      # Login/OTP screens
└── tile/
    ├── base.cpp      # BASE file viewer (s_base) - 40%
    ├── pipe.cpp      # Pipe JSON tree (s_pipe) - 35%
    ├── note.cpp      # Event history (s_note) - 25%
    └── head.cpp      # Tune stages (s_head)
```

## Layout

```
┌─────────────┬──────────────────────────┐
│    BASE     │                          │
│  (files)    │                          │
├─────────────│                          │
│    PIPE     │          HEAD            │
│  (tree)     │        (tune)            │
├─────────────│                          │
│    NOTE     │                          │
│  (events)   │                          │
└─────────────┴──────────────────────────┘
```

## Notes (Event System)

Tiles post and check notes instead of sharing state directly.

```cpp
postNote("pipe.select", pipe_name);   // Sender
checkNote("pipe.select", data, size); // Receiver
```

Current notes:
- `pipe.select` - Pipe selected in labs tile
- `push.done` - RAW upload complete
- `drop.done` - Pipe deleted
- `tune.start` - Start tune pipeline
- `vibe.add` - Add pipe to vibe training
- `labs.list` - List loading status
- `raws.load` - RAW file picked

## Transition: Global to Local State

Each tile has local static state (`s_labs`, `s_note`, `s_head`). Currently syncs from `g_state` (temporary).

### Done
- [x] Tile files created with local state structs
- [x] Note-based communication working
- [x] desk.hpp replaces state.hpp

### Pending
- [ ] Remove sync blocks from tiles (marked "temporary during migration")
- [ ] Move fields from AppState to tile-local state
- [ ] Tiles become fully independent

### AppState Fields to Migrate

**To s_base:**
- pipes[], pipe_count, list_loaded, list_loading
- files[], file_count, files_loaded, files_loading

**To s_pipe:**
- current_pipe
- json_file, json_tree, json_loading, json_loaded

**To s_head:**
- head_texture, lute_texture, drum_texture, diff_texture
- head_width, head_height, preview_width, preview_height
- preview_texture, head_rgb, head_rgb8
- tune_running, tune_step

**To s_note:**
- (already migrated - uses g_note_history directly)

**Keep in AppState (cross-cutting):**
- screen, window
- jwt, email, user_id, itag, role (auth)
- show_*_pane (layout)
- error_message, status_message
- Task queue (shared by multiple tiles)

## Adding a New Tile

1. Create `tile/foo.cpp` with local state struct
2. Add `render_foo_tile()` function
3. Declare in `desk.hpp`
4. Call from `desk.cpp`
5. Use notes to communicate with other tiles

## Dual Deployment Targets (Future)

main.cpp can switch between two WASM targets:

```cpp
#ifdef __EMSCRIPTEN__
    // Browser: ImGui loop
    initWindow();
    emscripten_set_main_loop(frame, 0, 1);
#elif defined(__wasi__)
    // Server: headless pipeline
    process_stdin_to_stdout();
#endif
```

**Browser (emscripten):** Full GUI - desk, tiles, auth, fetch API, textures

**Server (wasmtime):** Headless - just GEAR/PIPE/LUTE processing, no UI

Shared code: `gear/`, `pipe/`, `lute/` (image processing)
Browser-only: `labs/` (UI)

Server options:
- CLI-style: RAW from stdin, result to stdout
- WASI socket API for request/response
