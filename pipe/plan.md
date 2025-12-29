# Pipe

Clean-room darktable recreation. Each Link = one DT module, matched 1:1.

---

## How To: Validate a Module

For each module in pipeline order:

### 1. Params Check
```bash
./tmp/build/dark <test.xmp> --dump | grep -A20 "modulename"
```
Compare with `run_<module>` in pipe-cli.cpp. All params must be:
- USED: extracted and passed to module
- UNSUPPORTED: documented with reason in module header

### 2. Colorspace Check
Verify module operates in correct colorspace:
| Stage | Colorspace | Modules |
|-------|------------|---------|
| SENSOR | Bayer float [0,1] | rawprepare, temperature, highlights, demosaic |
| LINEAR | RGB float | exposure, channelmixer, channelmixerrgb, colorbalancergb, colorin |
| LAB | L[0,100] a,b[-128,128] | bilat |
| DISPLAY | RGB [0,1] | colorout, gamma, sigmoid, filmicrgb |

### 3. Blend Check
```bash
./tmp/build/dark <test.xmp> --dump | grep -E "blend|opacity"
```
Must be `normal@100%` or skip module until blendops implemented.

### 4. Code Review
Open DT source and our implementation side-by-side:
- DT: `dark/lib/desk/src/iop/<module>.c`
- Ours: `src/main/flow/<module>.cpp`

Verify algorithm matches at pseudo-code level. Same operations, same order.

### 5. Run Test
```bash
./scripts/unit-test.sh <module>
```
PASS = dE < 2.0. FAIL = fix before continuing.

---

## 0. Prerequisites

| Check | Status |
|-------|--------|
| Head matches LibRaw | ✓ |
| colorout code verified | - |
| gamma code verified | - |
| Baseline test (0082) dE < 2.0 | - |

Complete these before module validation. colorout/gamma gate all tests.

---

## 1. Audit (Code Review)

| Module | Params | Colorspace | Code | Notes |
|--------|--------|------------|------|-------|
| rawprepare | OK | SENSOR | - | |
| temperature | OK | SENSOR | - | |
| highlights | OK | SENSOR | - | |
| demosaic | TODO | SENSOR | - | method param |
| exposure | OK | LINEAR | - | |
| channelmixer | TODO | LINEAR | - | |
| channelmixerrgb | TODO | LINEAR | - | |
| colorbalancergb | TODO | LINEAR | - | |
| colorin | OK | LINEAR→LAB | - | |
| bilat | TODO | LAB | - | |
| colorout | TODO | LAB→LINEAR | - | |
| gamma | OK | DISPLAY | - | |
| sigmoid | TODO | DISPLAY | - | |
| filmicrgb | TODO | DISPLAY | - | |
| flip | OK | any | - | |

## 2. Noop (Blend Check)

| Module | DT Test | Blend | Runnable |
|--------|---------|-------|----------|
| rawprepare | 0082-demosaic-rcd | - | - |
| temperature | 0082-demosaic-rcd | - | - |
| highlights | 0015-highlights | - | - |
| demosaic | 0082-demosaic-rcd | - | - |
| exposure | 0001-exposure | - | - |
| colorin | 0082-demosaic-rcd | - | - |
| colorout | 0082-demosaic-rcd | - | - |
| gamma | 0082-demosaic-rcd | - | - |

Runnable = blend is normal@100%. Skip module if not.

## 3. Integration (DT Comparison)

| Module | DT Test | dE | Status |
|--------|---------|-----|--------|
| rawprepare | 0082-demosaic-rcd | - | - |
| temperature | 0082-demosaic-rcd | - | - |
| highlights | 0015-highlights | - | - |
| demosaic | 0082-demosaic-rcd | - | - |
| exposure | 0001-exposure | - | - |
| colorin | 0082-demosaic-rcd | - | - |
| colorout | 0082-demosaic-rcd | - | - |
| gamma | 0082-demosaic-rcd | - | - |

Legend: OK = done, TODO = needs work, - = not checked, ✓ = PASS, ✗ = FAIL

---

## TODO

**Prerequisites (gates all tests):**
1. **colorout** - Verify Lab→XYZ→sRGB matches DT
2. **gamma** - Verify sRGB transfer function matches DT
3. **Baseline** - Get 0082-demosaic-rcd to dE < 2.0

**Audit (params extraction):**
4. **demosaic** - Extract method param (5=RCD)
5. **channelmixer** - Extract all params, document unsupported
6. **channelmixerrgb** - Extract matrix params
7. **colorbalancergb** - Extract saturation/vibrance/contrast params
8. **bilat** - Extract sigma/detail params
9. **sigmoid** - Extract contrast/skew params
10. **filmicrgb** - Extract grey/white/black params

**After audit complete:**
11. Noop pass - verify blend normal@100% for each test
12. Integration pass - run DT comparison, fix failures in pipeline order

---

## How To: Run Tests

### DT Unit Test
Test single module against DT's XMP:
```bash
./scripts/unit-test.sh <module>     # Single module
./scripts/unit-test.sh all          # All modules, stop on first failure
```

### DT Integration Test
Test full pipeline against DT output:
```bash
./scripts/dt-compare.sh <xmp>       # Single XMP
./scripts/integration-test.sh all   # All 166 tests
```

Exit codes: 0=PASS (dE<2.0), 1=ERROR, 2=FAIL

---

## Reference

- DT source: `dark/lib/desk/src/iop/`
- DT tests: `dark/lib/desk/src/tests/integration/`
- Test image: `mire1.cr2` (Canon EOS 40D)
- Baseline test: `0082-demosaic-rcd`
