# VIEW: Camera Look Matching

[back](../README.md)

Match the camera's in-body rendering using the embedded preview as a tune reference.

---

## Architecture

```
pipe::Pipe::open(sink)
    │
    ├──► head.data()  →  scene-linear RGB + camera metadata
    │
    └──► head.view()  →  embedded preview + style metadata
                              │
                              ▼
                         tune reference
```

The decoder extracts both the scene-referred image and the camera's display-referred preview in a single `prepare()` call. No separate extraction step needed.

---

## Strategy

The embedded JPEG is just another reference image. Use tune.

```
head.data() ──► pipe (body) ──► output
                                   │
head.view() ─────────────────► diff ◄──┘
                                   │
                               tune (SPSA)
```

No special "camera look extraction" logic. Tune finds dials that minimize spectral loss against `head.view()`.

---

## View Metadata

Available via `head.view().info()`:

| Key | Example | Description |
|-----|---------|-------------|
| width | 1616 | Preview width |
| height | 1080 | Preview height |
| format | srgb_8bit | Color space |
| creative_style | Standard | Camera look preset |
| dro | Auto | Dynamic Range Optimizer |
| contrast | 0 | -3 to +3 |
| saturation | 0 | -3 to +3 |
| sharpness | 0 | -3 to +3 |

Style metadata describes what camera settings produced the preview. Useful for caching calibrated dials by style.

---

## Optional: Cache Calibrations

If the same Creative Style + DRO combo is used repeatedly, cache the tuned dials:

```
tmp/view/calibration/
├── a7iii_standard_dro_auto.json
├── a7iii_vivid_dro_auto.json
└── ...
```

Runtime: read `creative_style` + `dro` from view info → lookup cached dials → skip tune.

This is optimization, not core functionality. Tune works without it.

---

## Analysis Notes

**Sony Tone Curve** (tag 0x7010): Fixed per Creative Style, not per-image. Values `8000 10400 12900 14100` are identical across all Standard images.

**DRO Auto**: Scene-dependent processing. Camera doesn't record what it did - only that Auto was selected. Calibration captures the "average" behavior.

**Resolution**: Preview is 1616x1080. Spectral loss is content-invariant, works across resolutions.

---

## Current Scope

- **Camera**: ILCE-7M3 (A7III)
- **Style**: Standard + DRO Auto
- **Adjustments**: 0, 0, 0 (Normal)

See [todo.md](./todo.md) for expansion plans.
