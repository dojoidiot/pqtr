
## March 1: Baseline

### DT Pipeline (empty history)

DT with "[empty history stack]" applies these **mandatory** modules:

| Module | Source File | Description |
|--------|-------------|-------------|
| rawprepare | iop/rawprepare.c | Black level subtraction |
| demosaic | iop/demosaic.c | Bayer → RGB (RCD default) |
| colorin | iop/colorin.c | Camera RGB → Lab, **applies D65/as_shot WB** |
| colorout | iop/colorout.c | Lab → sRGB |
| gamma | iop/gamma.c | sRGB transfer function |

### Key Finding: colorin applies WB

From `../dark/lib/desk/src/iop/colorin.c` line 656-658:
```c
float coeffs = { corrected ? chr->D65coeffs[0] / chr->as_shot[0] : 1.0f, ...
```

colorin normalizes to D65 illuminant by dividing by as_shot WB values.
This is NOT the same as the temperature module - temperature is for user adjustment.

### Current Status

**Mean diff: 8.4** (was 16 before enabling WB+ColorMatrix)

Remaining difference is nonlinear:
- Darks (DT 0-20): our output is 1.5x brighter
- Mids (DT 20-50): roughly equal
- Highlights (DT 100-200): our output is 0.78x darker

This pattern indicates a **linearization curve difference**.

### Investigation Notes

1. **Sony decoding is correct** - we had PPM-perfect match with LibRaw
   - Our linearization curve matches LibRaw
   - Issue is NOT in Sony decoding

2. **Gamma is correct** - dt uses same sRGB formula as us:
   - `in <= 0.0031308 ? 12.92*in : 1.055*pow(in, 1/2.4) - 0.055`

3. **Issue is likely in colorin/colorout**:
   - dt's colorin does: Camera RGB → Lab (with D65 normalization)
   - dt's colorout does: Lab → sRGB
   - We do: Camera RGB → sRGB directly (no Lab intermediate)

### Next Steps

1. **Check byte-perfect before gamma** - dump linear values after each step
2. **Match colorin exactly** - understand the Lab conversion path
3. LibRaw is available at `./LibRaw` for reference

### Code Flags (process.cpp)

```cpp
#define USE_CPU_RCD_DEMOSAIC 1  // Match dt's RCD
#define SKIP_WB 0               // WB enabled (colorin applies D65 norm)
#define SKIP_COLOR_MATRIX 0     // Camera matrix enabled
```

---

## March 2: + temperature (WB user adjustment)

*TBD after March 1 signed off*

## March 3: + exposure

*TBD after March 2 signed off*

## March 4: + sigmoid (tone mapping)

*TBD after March 3 signed off*

---

## DT Source Locations

Key files in `../dark/lib/desk/src/`:

| Module | File |
|--------|------|
| rawprepare | iop/rawprepare.c |
| demosaic | iop/demosaic.c |
| temperature | iop/temperature.c |
| colorin | iop/colorin.c |
| colorout | iop/colorout.c |
| gamma | iop/gamma.c |
| sigmoid | iop/sigmoid.c |
| Sony decoding | external/LibRaw/src/decoders/decoders_dcraw.cpp |
| Sony metadata | external/LibRaw/src/metadata/sony.cpp |
