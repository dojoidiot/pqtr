# GOLD - Stage-Aware Optimizer

## Overview

`bin/gold` tests the STAGED optimizer mode. It separates optimization into phases that match human perception:

1. **VIEW** - Tone/luminance dials (how bright/contrasty)
2. **POPS** - Color dials by opponent axes (how colorful)
3. **Joint** - All dials together (final polish)

## Usage

```bash
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/gold \
  ../DESK/var/DSC00202.ARW preview \
  --save-area tmp/var/tune
```

## Output

```
tmp/var/tune/DSC00202/
  head.png    - camera preview (reference)
  tail.png    - pipeline output
  diff.png    - difference x5
  tune.json   - dial settings
```

## Architecture

### Phase 1: VIEW (6 dials)

```
exposure, contrast, highlights, shadows, black, white
```

Uses `viewLoss()` which weights luminance features high:
- std_L (contrast) × 6.0
- skew_L (tone asymmetry) × 6.0
- L_p10..L_p90 (percentiles) × 6.0 each
- Chroma features suppressed (× 0.2)

### Phase 2: POPS (31 dials in 6 groups)

Organized by **opponent color pairs** - how human vision processes color:

| Group | Dials | Count |
|-------|-------|-------|
| GLOBAL | vibrance, saturation, colourDensity | 3 |
| SPLIT | shadow_temp/tint, highlight_temp/tint | 4 |
| R-C | Red + Cyan HSL | 6 |
| G-M | Green + Magenta HSL | 6 |
| B-Y | Blue + Yellow HSL | 6 |
| O-P | Orange + Purple HSL | 6 |

Uses `popsLoss()` which weights chroma features high:
- mu_C, std_C × 5.0
- mu_a, mu_b × 5.0
- C_p50, C_shadow × 5.0
- Split tone colors × 4.0
- Luminance features as anchors (× 1.0)

### Phase 3: Joint (45 dials)

All dials polished together using `geodesicLoss()` (standard weighted L2).

## Results

| Image | After Poly | STAGED | Notes |
|-------|-----------|--------|-------|
| DSC00202 | 2.6% | **2.4%** | Easy image - STAGED wins |
| DSC00144 | 12.8% | **12.8%** | Hard image - VIEW oscillates |

## Key Insight

Opponent color pairs (R-C, G-M, B-Y) are optimized together because:
- Complementary colors often compensate each other
- Matches opponent-process theory of human color vision
- Reduces interference between color adjustments

## Source Files

| File | Purpose |
|------|---------|
| `src/main/gold.cpp` | Test binary |
| `src/main/part/geos/staged.cpp` | STAGED optimizer |
| `src/main/part/geos/diff.cpp` | viewLoss(), popsLoss() |
| `inc/geos.hpp` | Mode::STAGED enum |
