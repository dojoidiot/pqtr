# VIBE - Neural Dial Prediction

## Overview

`bin/vibe` trains neural networks to predict dial settings from image features. Learns how humans (camera or user) manipulate images.

## Two-Stage Architecture

### Stage 1: Camera Vibe (complete)
- Input: StyleFeatures (23)
- Output: 45 dial values [0.0 - 1.0]
- Training: 537 ARW+JPG pairs from /home/z/base/pics
- Goal: Replicate camera JPG appearance

### Stage 2: User Vibe (future)
- Input: Camera dials + user-edited result
- Output: 45 dial deltas (adjustments)
- Training: User's edited images
- Goal: Learn personal editing style

## Vibe Classes

From EXIF metadata (537 pairs with both ARW+JPG):

| Vibe Class | Count |
|------------|-------|
| portrait_off | 146 |
| vivid_off | 118 |
| standard_off | 101 |
| portrait_on | 90 |
| standard_on | 82 |

## Network Architecture

```
MLP: 23 → 128 → 64 → 45

Input Layer:  23 StyleFeatures
Hidden 1:     128 units, ReLU
Hidden 2:     64 units, ReLU
Output:       45 dials, Sigmoid (clamp to 0-1)
```

Parameters: ~12,000 weights
Training: CPU, pure C++, no external ML libs

## Camera Base + Deltas

Raw MLP predictions cluster near 0.5 (conservative). The optimizer learned to match camera JPGs by reducing our pipeline's saturation.

**Solution:** Apply predictions as deltas from a camera base:

```cpp
// Camera base = mean dials across 537 training samples
float CAMERA_BASE[45] = {0.574, 0.535, 0.465, ...};

// Apply as deltas
for (int i = 0; i < 45; i++) {
    float delta = mlp_prediction[i] - 0.5f;
    final[i] = clamp(CAMERA_BASE[i] + delta, 0, 1);
}
```

Key insight: vibrance/saturation base is ~0.46, meaning our pipeline's neutral (0.5) is more saturated than camera JPGs.

## Usage

```bash
# Extract training data (runs optimizer on each pair)
bin/vibe extract /home/z/base/pics --out tmp/var/vibe/train.json

# Train MLP
bin/vibe train tmp/var/vibe/train.json --out etc/camera.vibe

# Render with different modes
bin/vibe render image.ARW --model etc/camera.vibe --out tmp/         # raw
bin/vibe render image.ARW --model etc/camera.vibe --out tmp/ --base  # camera base + deltas
bin/vibe render image.ARW --model etc/camera.vibe --out tmp/ --lush 0.15  # saturation boost
```

## Files

| File | Purpose |
|------|---------|
| src/main/vibe.cpp | Training/inference binary |
| src/main/part/vibe/mlp.hpp | MLP forward/backward |
| etc/camera.vibe | Trained Stage 1 model |
| tmp/var/vibe/train_full.json | 537 training samples |

## Training Results

- 537 samples extracted with 16-thread parallel optimization
- Training loss: 0.19%, Validation loss: 0.23%
- Model saved to etc/camera.vibe

## Render Output

For each image, creates:
- `_preview.png` - camera preview (reference)
- `_vibe.png` - pipeline with predicted dials
- `_camera.png` - camera JPG resized
- `_diff.png` - difference x5

## Future Work

### Per-Vibe-Class Base Dials
Instead of global average, compute base per class:
- portrait_off base, vivid_off base, etc.
- MLP could predict class, then use class-specific base

### Confidence-Guided Optimization

Extend MLP to output uncertainty per dial:

```
Option 1: Dual-head MLP
  features → hidden → [dials_head] → 45 values
                   → [confidence_head] → 45 uncertainties

Option 2: Monte Carlo Dropout
  Run inference 10x with dropout, measure variance per dial
```

Benefits:
- Focus optimizer iterations on uncertain dials
- Skip optimization entirely if all dials high-confidence

### Warm-Start Optimizer

Use MLP prediction as starting point:
- Current: optimizer starts from neutral (0.5), needs ~300 iterations
- With warm-start: starts from MLP prediction, converges in ~20 iterations

## Status

- [x] MLP class with backprop
- [x] Extract command with optimizer
- [x] Train command
- [x] Predict command
- [x] Render command with --base and --lush modes
- [x] OpenMP parallel extraction (16 threads)
- [x] Full 537-pair training
- [x] Camera base + deltas mode
- [ ] Per-vibe-class base dials
- [ ] Confidence output head
- [ ] Warm-start optimizer integration
