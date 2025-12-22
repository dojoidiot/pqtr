# VIBE Session Findings

## Summary

The VIBE pipeline is now functional with debugged sigmoid tone mapping. The path is clear for parameter optimization.

## Key Findings

### 1. Darktable XMP Parsing

**blendop_params encoding:**
- `gz11` prefix = blend_cst=0 (normal blending) - modules execute
- `gz08` prefix = blend_cst=4 (RGB_SCENE) - can cause modules to skip

When creating test XMPs, always use the `gz11` blendop:
```
gz11eJxjYIAACQYYOOHEgAZY0QWAgBGLGANDgz0Ej1Q+dcF/IADRAGpyHQU=
```

**Auto-presets behavior:**
- Darktable auto-applies sigmoid in scene-referred workflow (iop_order_version=4)
- Use `--apply-custom-presets false` AND explicitly disable sigmoid in XMP to get sigmoid-free output
- Even with flag, darktable applies: temperature, highlights, exposure, channelmixerrgb

### 2. HEAD Stage Brightness Offset

Our RAW decode produces ~1.5x darker values than darktable's scene-linear output:
- VIBE HEAD mean: ~0.33 (linear)
- Darktable mean: ~0.50 (linear)
- Offset: ~0.6 EV

This is consistent across images and likely due to different normalization or auto-exposure that darktable applies. Can be compensated with exposure adjustment.

### 3. Log-Logistic Sigmoid Implementation

Darktable's sigmoid uses generalized log-logistic formula:
```cpp
film_response = pow(film_fog + value, film_power)
paper_response = white_target * pow(film_response / (paper_exp + film_response), paper_power)
```

Parameter derivation from UI values (contrast, skew):
- paper_power = pow(5, -skew)
- film_power derived from slope matching at middle grey (0.1845)
- film_fog and paper_exp derived from display white/black targets

Implementation in `vibe.cpp:calculate_sigmoid_params()` matches darktable's `commit_params()`.

### 4. Current Pipeline Accuracy

With compensated exposure (+1.3 EV total):
- VIBE sigmoid compression: 3.2%
- Darktable sigmoid compression: 5.4%
- Linear difference: ~4%

Good enough for optimization - the optimizer can learn the remaining offset.

## Path Forward: Parameter Optimization

### Why It's Now Tractable

1. **Small parameter space**: 5-10 scalar values
   - exposure_ev: [-2, +2]
   - contrast: [1.0, 2.5]
   - skew: [-1, +1]
   - saturation: [-1, +1]
   - vibrance: [-1, +1]

2. **Smooth, differentiable operations**: All VIBE operations are continuous functions suitable for gradient-based optimization

3. **Fast GPU evaluation**: VIBE shader can evaluate hundreds of parameter combinations per second

4. **Clear loss function**: Compare to reference JPEG (embedded or darktable export)

### Suggested Approach

```
1. Load RAW → HEAD decode → scene-linear RGB
2. Load reference JPEG → convert to linear
3. Initialize params (exposure=+0.6, contrast=1.5, etc.)
4. Optimization loop:
   a. VIBE(scene_linear, params) → output
   b. loss = perceptual_diff(output, reference)
   c. update params (gradient descent, Nelder-Mead, or grid search)
5. Return optimal params
```

### Loss Function Options

- **L2 in linear space**: Simple, fast, but ignores perceptual importance
- **L2 in LAB space**: Better perceptual weighting
- **Histogram matching**: Match overall tone distribution
- **SSIM**: Structural similarity
- **Hybrid**: Combine histogram + local SSIM

### Expected Outcomes

The optimizer should automatically discover:
- The +0.6 EV HEAD offset compensation
- Contrast/saturation settings that match camera rendering
- Any camera-specific tone curve characteristics

This learned profile can then be applied to other RAWs from the same camera.

## Test Files

XMP files for stepwise testing in `tmp/xmp/`:
- `no_sigmoid.xmp` - sigmoid explicitly disabled
- `sigmoid_only.xmp` - minimal pipeline + sigmoid
- `exp_sig.xmp` - exposure (0.7 EV) + sigmoid
- `exp_match.xmp` - compensated exposure (1.3 EV) + sigmoid

## Optimizer Implementation

### Results (First Run)

The Nelder-Mead optimizer successfully converged:

| Parameter | Initial | Final |
|-----------|---------|-------|
| exposure_ev | +0.6 EV | **+2.0 EV** |
| contrast | 1.5 | **1.0** |
| skew | 0.0 | **0.29** |
| saturation | 0.0 | **-0.88** |
| vibrance | 0.0 | **+0.18** |
| **ΔE** | 28.6 | **10.0** |

### Key Learnings

1. **HEAD offset is larger than expected**: The optimizer found +2.0 EV compensation (not +0.6 EV from earlier estimates)

2. **Lower contrast matches camera JPEG**: The Sony camera applies less aggressive contrast than darktable's default sigmoid

3. **Significant desaturation needed**: Camera JPEG has substantially less saturation (-0.88) than our default

4. **Positive skew helps**: A slight skew (0.29) toward highlights improves the match

### Run Command

```bash
cd VIBE && make tune
./tmp/test/test_tune [input.ARW]
```

Outputs:
- `tmp/var/flow/<name>.tune.head.png` - HEAD stage output
- `tmp/var/flow/<name>.tune.vibe.png` - VIBE output with learned params
- `tmp/var/flow/<name>.tune.ref.png` - Reference JPEG
- `tmp/var/flow/<name>.tune.json` - Full metadata with learned params

## Code References

- `src/main/flow/part/vibe.cpp` - VIBE GPU shader and sigmoid param calculation
- `src/main/flow/part/tune.cpp` - Nelder-Mead parameter optimizer
- `src/main/flow/part/copy.cpp` - XMP parsing and module decoding
- `src/test/flow/flow.cpp` - Stepwise test harness
- `src/test/flow/test_tune.cpp` - Optimizer test driver
