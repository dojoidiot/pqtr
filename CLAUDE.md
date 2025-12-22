# Claude Code Session Context

This document summarizes the goals, progress, and current status of the development session.

## 1. Ultimate Goal

The primary objective is to develop a C++ image processing pipeline that can:
1.  Read the processing steps from a Darktable `.xmp` sidecar file.
2.  Decode a corresponding Sony `.ARW` RAW image file.
3.  Translate the Darktable settings into parameters for the custom `vibe.hpp` styling library.
4.  Apply these settings to the decoded RAW image to produce an artistically similar result.

## 2. Project Structure

All work is in `./VIBE` - a standalone project independent of LABS.

```
VIBE/
├── inc/
│   ├── flow.hpp      # Flow API (Tree, Task, Done)
│   └── vibe.hpp      # Vibe styling API (Linear + Display stages)
├── src/
│   ├── main/flow/
│   │   ├── flow.cpp
│   │   └── part/
│   │       ├── head.cpp    # HEAD: RAW decode (clean-room, no libraw)
│   │       ├── sony.cpp    # Sony ARW parsing
│   │       ├── vibe.cpp    # VIBE: style application
│   │       ├── tune.cpp    # Camera profile learning
│   │       └── ...         # Supporting modules
│   └── test/flow/
│       ├── flow.cpp        # Main test driver
│       ├── test_tune.cpp   # Tune tests
│       ├── DSC00144.ARW    # Test RAW
│       └── DSC00144.JPG    # Test reference
└── lib/                    # External dependencies (if needed)
```

## 3. The Three-Step Flow

1.  **Head (RAW Decode):** Decode `.ARW` to scene-linear RGB using clean-room implementation in `head.cpp`/`sony.cpp`
2.  **Copy (Translate):** Parse `.xmp` file and populate `flow::Tree` with Vibe settings (TO BE IMPLEMENTED)
3.  **Vibe (Apply):** Apply style using `vibe.cpp`

## 4. Current Status

**Project created.** VIBE folder initialized with flow.hpp, vibe.hpp, and all source/test files.

**Next step:** Implement the XMP parser (Copy step) to translate Darktable settings to Vibe parameters.

## 5. Key APIs

### flow.hpp
- `flow::Tree` - hierarchical metadata (Stem/Leaf nodes)
- `flow::Flow` - RAW image container with `head()` and `tune()` GPU tasks
- `flow::Done` - final RGB result

### vibe.hpp
- `vibe::Vibe::Linear` - scene-linear processing (exposure, WB, curves, LUTs)
- `vibe::Vibe::Display` - display-referred processing (tone curves in sRGB)

---
*Session continued by Claude Code.*
