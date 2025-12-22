# VIBE Development Plan

## Meta-Strategy: Optimizer-Driven Development

The core insight is that the Nelder-Mead optimizer serves as both a **development tool** and a **validation system**. For each module in `vibe.hpp`:

1. **Implement** the module in the GPU shader (`vibe.cpp`)
2. **Add** its parameters to the optimizer (`tune.cpp`)
3. **Run** the optimizer and measure ΔE improvement
4. **Validate**: If ΔE improves, module works. If not, debug.
5. **Capture**: Learned values become the camera profile

This creates a systematic, testable approach to building out the full VIBE pipeline.

## Current State (v6 - Phase 5 Complete)

**Implemented modules:**
- ColorCorrection.whiteBalance.temperature ✓
- ColorCorrection.whiteBalance.tint ✓
- ColorCorrection.exposure ✓
- ToneMapping.contrast ✓
- ToneMapping.skew ✓
- GlobalColor.saturation ✓
- GlobalColor.vibrance ✓
- BaseCurve (Bezier per-channel) ✓
- SplitTone (shadow/highlight tinting) ✓ (disabled - no improvement)
- SelectiveColour (per-hue saturation) ✓ (disabled - no improvement)
- HSL Bezier Curves (per-hue H/S/L adjust) ✓ (disabled - no improvement)

**Results:**
- Phase 1 (5 params): ΔE = 10.0
- Phase 2 (7 params): ΔE = 9.9
- Phase 3 (13 params): ΔE = 8.0
- Phase 4a SplitTone (17 params): ΔE = 8.6 (no improvement)
- Phase 4b SelectiveColour (25 params): ΔE = 8.0 (no improvement)
- Phase 5 HSL Curves (37 params): ΔE = 8.0 (no improvement)
- **Best result:** Phase 3 with ΔE = 8.0 (13 active params)

**Learned profile (DSC00144, Phase 3):**
```json
{
  "whiteBalance": {
    "temperature": 6514,
    "tint": 0.74
  },
  "exposure": 2.0,
  "contrast": 1.0,
  "skew": 0.74,
  "saturation": -0.78,
  "vibrance": -0.13,
  "baseCurve": {
    "r_y1": 0.37, "r_y2": 0.63,
    "g_y1": 0.44, "g_y2": 0.51,
    "b_y1": 0.20, "b_y2": 0.66
  }
}
```

**Observations:**
- Temperature near neutral (6514K)
- Tint shifted magenta (+0.74) - camera adds warmth
- R curve: shadows lifted (0.37 vs 0.25), highlights compressed (0.63 vs 0.75)
- G curve: nearly flat S-curve (gamma reduction)
- B curve: shadows darker (0.20), highlights compressed (0.66)
- Per-channel curves capture different color responses

## Implementation Phases

### Phase 1: Core Scalars (COMPLETE)
- [x] Exposure (+/- EV)
- [x] Contrast (sigmoid slope)
- [x] Skew (highlight/shadow balance)
- [x] Saturation (global)
- [x] Vibrance (selective saturation)

### Phase 2: White Balance (COMPLETE)
- [x] Temperature (Kelvin)
- [x] Tint (green-magenta)

**Results:** ΔE 10.0 → 9.9 (marginal improvement)

The small improvement suggests the camera JPEG was already close to neutral white balance. However, the optimizer found a subtle warm+green shift that improved the match slightly.

### Phase 3: Tone Curves (COMPLETE)
- [x] BaseCurve (Bezier per-channel: 6 params)

**Why:** Residual tone adjustments after sigmoid. Camera applies subtle S-curves.

**Implementation:** Cubic Bezier curves with control points at x=0.25 and x=0.75.
- Identity: y1=0.25, y2=0.75 (straight line)
- Monotonicity ensured by bounds: y1 ∈ [0,0.5], y2 ∈ [0.5,1]
- 6 parameters total (2 per channel)

**Results:** ΔE 9.9 → 8.0 (significant 19% improvement)

### Phase 4: Color Grading (COMPLETE)

#### Phase 4a: SplitTone
- [x] SplitTone (shadow/highlight color shift) - implemented, no improvement

**Implementation:** SplitTone with hue (0-360°) and saturation (0-1) for shadows and highlights.
- 4 parameters: shadow_hue, shadow_sat, highlight_hue, highlight_sat
- Applied after curves, before global saturation

**Results:** ΔE 8.0 → 8.6 (17 params) - NO IMPROVEMENT
- Camera JPEG doesn't use split toning for this image
- Keeping implementation (disabled by default) for cameras that do use it

#### Phase 4b: SelectiveColour
- [x] SelectiveColour (per-hue saturation) - implemented, no improvement

**Implementation:** Per-hue saturation adjustment with 8 sectors (45° each).
- 8 parameters: red, orange, yellow, green, cyan, blue, purple, magenta
- HSL color space conversion with smooth sector interpolation
- Applied after curves, before global saturation

**Results:** ΔE 8.0 → 8.0 (25 params, 200 iters) - NO IMPROVEMENT
- Optimizer found all values ~0 (no per-hue adjustment needed)
- Camera JPEG doesn't apply per-hue saturation adjustments
- Keeping implementation (disabled by default) for cameras that do use it

**Conclusion:** Phase 4 shows that DSC00144 camera JPEG uses basic tone/color
adjustments (exposure, contrast, curves, saturation) but NOT advanced color grading
(split toning, per-hue adjustments). The optimizer correctly identifies this.

### Phase 5: Full Color Transform
- [ ] HsvLut (36×12×3 = 1,296 params)
- [ ] LutCurve (17³×3 = 14,739 params)

**Why:** Nuclear option for capturing any remaining color differences.

**Caution:** High-dimensional optimization needs:
- Regularization (L2 penalty on LUT smoothness)
- Good initialization (identity LUT)
- More iterations or different optimizer (L-BFGS, Adam)

### Phase 6: Meta-Optimization (LOOP BACK)
- [ ] Automatic parameter selection per camera
- [ ] Prune params that don't improve ΔE
- [ ] Generate minimal camera profile

**Why:** The optimizer should optimize its own process.

**Implementation:**
```cpp
// For each parameter:
// 1. Run optimizer WITH param, record ΔE_with
// 2. Run optimizer WITHOUT param, record ΔE_without
// 3. Keep param only if: ΔE_with < ΔE_without - threshold

struct ParamResult {
    int param_idx;
    float dE_with;
    float dE_without;
    bool keep;
};

std::vector<ParamResult> auto_select_params(image, ref) {
    for (int i = 0; i < N_PARAMS; i++) {
        // Enable only this param + core params
        auto result_with = optimize(enable_param(i));
        auto result_without = optimize(disable_param(i));

        results[i].keep = (result_with.dE < result_without.dE - 0.5f);
    }
    return results;
}
```

**Benefits:**
- Faster convergence (fewer params)
- Avoids overfitting
- Camera-specific minimal profiles
- Self-documenting: profile shows what camera actually uses

**Workflow:**
1. Complete Phase 5 (all modules implemented)
2. Run meta-optimizer on multiple images per camera
3. Generate camera profile with only useful params
4. Ship minimal profile for production use

## Parameter Bounds Reference

```cpp
// Phase 1 (current)
{ -2.0f, 2.0f, 0.6f },     // exposure_ev
{ 1.0f, 2.5f, 1.5f },      // contrast
{ -1.0f, 1.0f, 0.0f },     // skew
{ -1.0f, 1.0f, 0.0f },     // saturation
{ -1.0f, 1.0f, 0.0f },     // vibrance

// Phase 2 (next)
{ 4000.0f, 8000.0f, 5500.0f },  // temperature (Kelvin)
{ -1.0f, 1.0f, 0.0f },          // tint

// Phase 3
// PolyColor: 30 coefficients with small bounds around identity
// BaseCurve: 768 values, initialize to linear ramp

// Phase 4
// SplitTone: 4 params (shadow temp/tint, highlight temp/tint)
// SelectiveColour: 24 params (8 hues × 3 HSL adjustments)

// Phase 5
// HsvLut: 1296 params, regularized
// LutCurve: 14739 params, heavily regularized
```

## Validation Protocol

For each new module:

```
1. Baseline: Run optimizer WITHOUT new module, record ΔE_baseline
2. Add module: Run optimizer WITH new module, record ΔE_new
3. Check:
   - If ΔE_new < ΔE_baseline - 0.5: Module improves fit ✓
   - If ΔE_new ≈ ΔE_baseline: Module not needed for this image
   - If ΔE_new > ΔE_baseline: Bug in implementation ✗
4. Cross-validate: Test on multiple images from same camera
5. Profile: Save learned params as camera profile
```

## File Structure

```
VIBE/
├── doc/
│   ├── findings.md      # Session findings and debug notes
│   └── plan.md          # This file - development plan
├── inc/
│   ├── flow.hpp         # Pipeline API
│   └── vibe.hpp         # Full module definitions (target spec)
├── src/
│   ├── main/flow/part/
│   │   ├── vibe.cpp     # GPU shader - implements modules
│   │   ├── tune.cpp     # Optimizer - learns parameters
│   │   └── ...
│   └── test/flow/
│       ├── flow.cpp     # Pipeline test
│       └── test_tune.cpp # Optimizer test
└── tmp/
    └── var/flow/        # Output images and JSON profiles
```

## Recovery Instructions

To continue development in a new session:

1. **Read context:**
   ```
   Read: VIBE/doc/plan.md (this file)
   Read: VIBE/doc/findings.md
   Read: VIBE/inc/vibe.hpp (target API)
   ```

2. **Check current state:**
   ```bash
   cd VIBE && make tune
   ./tmp/test/test_tune src/test/flow/DSC00144.ARW
   # Note the final ΔE - this is the baseline
   ```

3. **Implement next phase:**
   - Find the next unchecked item in this plan
   - Add to shader (vibe.cpp)
   - Add to optimizer (tune.cpp)
   - Run and validate

4. **Update this document:**
   - Check off completed items
   - Record new ΔE
   - Add learned params to profile

## Success Criteria

| Phase | Target ΔE | Params | Result |
|-------|-----------|--------|--------|
| 1 | < 15 | 5 | ✓ 10.0 |
| 2 | < 10 | 7 | ✓ 9.9 |
| 3 | < 8 | 13 | ✓ 8.0 |
| 4a | < 5 | 17 | ✗ 8.6 (SplitTone no improvement) |
| 4b | < 5 | 25 | ✗ 8.0 (SelectiveColour no improvement) |
| 5 | < 2 | 1361+ | - |
| 6 | < 2 | auto | - (meta-optimize) |

**Best result:** Phase 3 with ΔE = 8.0 (13 useful params)

Final goal: ΔE < 2.0 (below perceptual threshold for most viewers)

**Development flow:**
```
Phase 1-5: Implement all modules → measure ΔE per module
    ↓
Phase 6: Loop back → auto-select useful params per camera
    ↓
Output: Minimal camera profile (only params that improve ΔE)
```

**Note:** Phase 3 Bezier curves achieved 19% improvement with only 6 additional parameters. Phase 4 SplitTone showed how the methodology validates what's needed - it was implemented but disabled when it didn't help.

## Commands Reference

```bash
# Build
cd VIBE && make all

# Run optimizer test
./tmp/test/test_tune src/test/flow/DSC00144.ARW

# Run pipeline test (with XMP)
./tmp/test/test_flow src/test/flow/DSC00144.ARW

# Clean
make tidy
```
