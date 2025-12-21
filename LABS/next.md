# Pipeline: ACES + TINT (Validated)

## Results (2024-12-21)

| Image | HEAD | ACES | TINT | Reduction |
|-------|------|------|------|-----------|
| DSC00144 | 12.3% | 9.1% | **8.2%** | 33% |
| DSC00202 | 8.4% | 6.0% | **4.4%** | 48% |

**Pipeline works.** ACES handles HDR→SDR compression, TINT learns the style delta.

---

## Final Pipeline

```
HEAD → ACES → TINT → VIBE
 │       │      │      │
 │       │      │      └── User dials (exposure, contrast, sat)
 │       │      └── 3D LUT from LUTE (camera style, 20% coverage ok)
 │       └── ACES filmic + auto-exposure (minimize error vs ref)
 └── GPU RAW decode (BLC, WB, demosaic, CST, warp)
```

---

## ACES Stage

**Formula** (Narkowicz 2015, public domain):
```cpp
float aces(float x) {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0f, 1.0f);
}
```

**Auto-exposure**: Search for multiplier that minimizes error after ACES:
```cpp
float best_exp = 1.0f, best_err = 1e9f;
for (float exp = 1.0f; exp <= 6.0f; exp += 0.2f) {
    float err = compute_error_after_aces(head, ref, exp);
    if (err < best_err) { best_err = err; best_exp = exp; }
}
```

Typical values: 1.2x to 3.0x (+0.3 to +1.6 EV)

---

## TINT Stage

**Role**: Learn remaining color style after ACES.

- Train from ACES output (not HEAD)
- 17³ LUT is sufficient (ACES handles compression)
- 20% coverage threshold (was 70% when learning everything)

**With ACES foundation, TINT learns small adjustments** - Sony's creative style
color shifts, not HDR→SDR compression.

---

## Files

```
src/test/flow/flow.cpp        - Test pipeline (HEAD→ACES→TINT)
src/main/flow/part/tint.cpp   - 3D LUT application (20% threshold)
inc/tint.hpp                  - TINT API
inc/lute.hpp                  - Camera profile learning
```

---

## TODO

1. **GPU ACES shader** - Move ACES to GPU for performance
2. **Buffer limit** - Fix 6000x4000 images (need requiredLimits)
3. **Persistent profiles** - Save/load LUTE profiles per camera
4. **VIBE stage** - User exposure/contrast/saturation dials

---

## References

- [ACES Filmic (Narkowicz 2015)](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/)
- [Darktable Color Spaces](https://docs.darktable.org/usermanual/4.6/en/special-topics/color-management/color-spaces/)
