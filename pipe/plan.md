# Pipe

## Objective

Clean-room stepwise recreation of darktable processing using pipe.hpp.
Each Link = one darktable module, matched 1:1.

## Golden Rules

1. **DT is always right. We are always wrong until we match DT exactly.**
2. **LibRaw is right for RAW decoding. We are wrong until we match LibRaw exactly.**
3. **Monkey see, monkey do.** Read dt module, replicate exactly, verify, move on.
4. **Never go back without asking.** Each step is signed off, then frozen.

## Reference

- **darktable**: v5.3.0 (in `dark/`)
- **LibRaw**: (in `LibRaw/`)
- **Test file**: `src/test/DSC00144.ARW` (Sony A7III, 3968x2648)
- **XMP sidecar**: `src/test/DSC00144.ARW.xmp`
- **Reference output**: `tmp/var/pipe/step-1-ref.png`

## Notes

- **Sony ARW embedded JPEG**: Varies by camera generation. Older cameras store
  full JPEG separately from ARW. Newer cameras embed JPEG+thumb in ARW.
  Preview extraction is a separate concern outside the pipe.

## Tools

- `make test` - run Step 0 (LibRaw verification)
- `./tmp/build/dark <file.xmp>` - parse XMP modules
- `dark/lib/dark/bin/darktable-cli` - generate reference images

---

## Step 0: RAW Decoder Verification ✓

Verify Sony ARW2 decoder matches LibRaw unprocessed_raw exactly.

| Check | Status |
|-------|--------|
| Bayer data | PASS - exact match |
| Dimensions | PASS - 3968x2648 |
| Black/White | PASS - 512/16383 |
| WB RGGB | PASS - [2420, 1024, 1616, 1024] |

**SIGNED OFF**

---

## Step 1: Generate DT Reference ✓

Run darktable-cli with XMP sidecar to generate reference.

| Output | Location |
|--------|----------|
| XMP sidecar | `src/test/DSC00144.ARW.xmp` |
| Reference PNG | `tmp/var/pipe/step-1-ref.png` |

**Pipeline (6 modules):**
```
0: rawprepare   - black level subtraction
1: demosaic     - bayer → RGB
2: temperature  - white balance
3: colorin      - camera RGB → Lab
4: colorout     - Lab → sRGB
5: gamma        - sRGB transfer function
```

**SIGNED OFF**

---

## Step 2: rawprepare

### Goal
Match darktable's `rawprepare` module - black level subtraction.

### DT Source
`dark/src/iop/rawprepare.c`

### XMP Params
```
params: 000000000000000000000000000000000002000200020002003c000000000000
```

### Implementation
- Link name: `rawprepare`
- Input: raw bayer uint16 from Head
- Operation: subtract black level (512) from each pixel
- Output: bayer with black subtracted

### Verification
- [ ] Output matches dt intermediate (if possible to capture)
- [ ] Values in expected range after subtraction

### Status
- [ ] Implemented
- [ ] Verified
- [ ] Signed off

---

## Step 3: demosaic

### Goal
Match darktable's `demosaic` module - bayer interpolation.

### DT Source
`dark/src/iop/demosaic.c`

### XMP Params
```
params: 0000000000000000000000000500000001000000cdcc4c3e00000000713d8a3e00000000080000000000000000000000
```

### Implementation
- Link name: `demosaic`
- Input: bayer data (after rawprepare)
- Operation: RCD demosaic (method 5)
- Output: RGB float

### Verification
- [ ] Demosaic method matches dt (RCD)
- [ ] Output dimensions correct

### Status
- [ ] Implemented
- [ ] Verified
- [ ] Signed off

---

## Step 4: temperature

### Goal
Match darktable's `temperature` module - white balance.

### DT Source
`dark/src/iop/temperature.c`

### XMP Params
```
params: 004018400000803f0080c83f0000000004000000
[0] = 2.37891 (r)
[1] = 1.0 (g)
[2] = 1.56641 (b)
```

### Implementation
- Link name: `temperature`
- Input: RGB (after demosaic)
- Operation: multiply by WB coefficients
- Output: white-balanced RGB

### Verification
- [ ] WB multipliers match XMP params
- [ ] Neutral grey stays neutral

### Status
- [ ] Implemented
- [ ] Verified
- [ ] Signed off

---

## Step 5: colorin

### Goal
Match darktable's `colorin` module - camera RGB to Lab.

### DT Source
`dark/src/iop/colorin.c`

### XMP Params
```
params: gz48eJzjZhgFowABWAbaAaNgwAEAOQAAEA== (compressed)
```

### Implementation
- Link name: `colorin`
- Input: camera RGB (after temperature)
- Operation: apply camera→XYZ matrix, XYZ→Lab
- Output: Lab

### Verification
- [ ] Color matrix matches camera profile
- [ ] Lab values in expected range

### Status
- [ ] Implemented
- [ ] Verified
- [ ] Signed off

---

## Step 6: colorout

### Goal
Match darktable's `colorout` module - Lab to sRGB.

### DT Source
`dark/src/iop/colorout.c`

### XMP Params
```
params: gz35eJxjZBgFo4CBAQAEEAAC (compressed)
```

### Implementation
- Link name: `colorout`
- Input: Lab (after colorin)
- Operation: Lab→XYZ→sRGB
- Output: linear sRGB

### Verification
- [ ] Output profile is sRGB
- [ ] Colors match reference

### Status
- [ ] Implemented
- [ ] Verified
- [ ] Signed off

---

## Step 7: gamma

### Goal
Match darktable's `gamma` module - sRGB transfer function.

### DT Source
`dark/src/iop/gamma.c`

### XMP Params
```
params: 0000000000000000
```

### Implementation
- Link name: `gamma`
- Input: linear sRGB (after colorout)
- Operation: apply sRGB gamma curve
- Output: display sRGB (8-bit)

### Verification
- [ ] Gamma curve matches sRGB spec
- [ ] Final output matches `step-1-ref.png`

### Status
- [ ] Implemented
- [ ] Verified
- [ ] Signed off

---

## Final Verification

When all steps complete:
- [ ] Pixel-perfect match with `tmp/var/pipe/step-1-ref.png`
- [ ] All 6 modules signed off
