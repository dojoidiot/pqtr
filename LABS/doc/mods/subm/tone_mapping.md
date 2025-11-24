# Tone Mapping

[back](../tone_mapping.md)

**Purpose**: Compresses high dynamic range scene data to a display-capable range while preserving perceptual contrast.

## Sub-Module: Contrast

Adjusts the global contrast of the filmic curve.

### Dials

| Dial          | Range     | Default | Maps To                   | Transfer Function                          |
|---------------|-----------|---------|---------------------------|--------------------------------------------|
| `contrast`      | 0.0 - 1.0 | 0.5     | 0.5 - 2.0                 | Exponential: `c = 0.5 * exp(value * 1.386)` |

**Total**: 1 dial

## Sub-Module: Curve Adjustment

This is a parameterized sub-module that adjusts specific regions of the tone curve. The same function is instantiated 2 times with different region constants.

### Parameters

**Region Constant** (compile-time parameter):
- `HIGHLIGHTS` (curve shoulder)
- `SHADOWS` (curve toe)

### Dials (per region instance)

| Dial          | Range     | Default | Maps To                   | Transfer Function                          |
|---------------|-----------|---------|---------------------------|--------------------------------------------|
| `adjustment`    | 0.0 - 1.0 | 0.5     | -100 to +100              | Linear centered: `a = (value - 0.5) * 200`   |

**Total**: 1 dial per region × 2 regions = **2 dials**

## Sub-Module: Clipping Point

This is a parameterized sub-module that defines the black and white clipping points. The same function is instantiated 2 times with different endpoint constants.

### Parameters

**Endpoint Constant** (compile-time parameter):
- `WHITE` (upper bound, default 0.85 → 8.0 scene luminance)
- `BLACK` (lower bound, default 0.15 → 0.015 scene luminance)

### Dials (per endpoint instance)

| Dial          | Range     | Default | Maps To                          | Transfer Function                          |
|---------------|-----------|---------|----------------------------------|--------------------------------------------|
| `point`         | 0.0 - 1.0 | varies  | Varies by constant               | WHITE: `wp = exp(value * 2.773)`<br>BLACK: `bp = value * 0.1` |

**Total**: 1 dial per endpoint × 2 endpoints = **2 dials**

## Total Dials

**5 dials** across all tone mapping sub-modules (1 + 2 + 2)

**Notes**:
- The filmic curve is computed from these 5 parameters.
- White point default 0.85 → 8.0 scene luminance (typical for well-exposed images).
- Black point default 0.15 → 0.015 (to preserve shadow detail).
- Highlights/shadows adjust the curve shoulder/toe independently.
