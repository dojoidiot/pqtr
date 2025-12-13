# DRUM

Dynamic Range Uplift Module - learns and applies DRO from camera metadata.

## What It Does

DRUM optimizes dynamic range for high-contrast scenes. When cameras detect challenging lighting (backlit subjects, bright sky/dark foreground), they apply DRO processing. DRUM learns these patterns to apply similar optimization.

## How It Works

Follows the same three-step pattern as LUTE:

1. **Detect** - Identify scenes where camera applied DRO
2. **Learn** - Extract DRO parameters from RAW+preview pairs
3. **Apply** - Use learned parameters on new images

## Pipe Link Contributions

DRUM contributes two links:

```cpp
pipe::Hold<pipe::Link> drum::tune();  // Learn DRO
pipe::Hold<pipe::Link> drum::view();  // Apply DRO
```

| Link | Input Page | Output Page | Purpose |
|------|------------|-------------|---------|
| `drum::tune()` | Context* | Context* | Learn DRO from RAW+preview |
| `drum::view()` | Context* | Context* | Apply learned DRO |

### Pipe Configuration

```cpp
pipe->link(gear::link());    // raw → Bayer
pipe->link(wgpu::open());    // Bayer → GPU
pipe->link(lute::view());    // apply camera profile
pipe->link(drum::view());    // ← apply DRO
pipe->link(vibe::view());    // apply style
pipe->link(wgpu::shut());    // GPU → output
```

See [PIPE](../PIPE/README.md) for the full pipeline model.

## Status

Planned. DRO detection and learning algorithms under research.
