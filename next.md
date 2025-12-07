# Next Steps (2025-12-07)

## Current State

**The good:** LUT curve estimation works. Gets ~7-8% loss and looks good.

**The problem:** Dial optimization makes things worse. Loss function finds mathematically optimal but perceptually awful settings.

**Root cause:** Cameras use HSV LUTs (HueSatDelta tables), not linear dials. They can do per-hue corrections like "if green, shift hue -5° and boost sat 10%". Our 45 linear dials can't express this.

## Test Code Warning

`LABS/src/main/part/geos/aceo.cpp` has experimental test code at ~line 576 that:
- Returns early after LUT estimation
- Tries greedy dial optimization
- Does NOT run normal ACEO/SPSA

**To restore normal operation:** Remove the test block starting with `// TEST 2:` at line ~576.

## Three Options for Tomorrow

### Option 1: HSV LUT (Most Promising)
Replace dial-based color with learned HSV LUT:
- For each (hue, saturation) cell: learn (Δhue, Δsat, Δval)
- Similar to DCP HueSatDelta tables (90 hue × 25 sat grid)
- Direct measurement from flat→target, no optimization needed

### Option 2: Extract Sony DCP
Adobe ships DCP profiles for cameras. Study Sony's HueSatDelta tables:
- Located in Lightroom's camera profiles
- Would reveal exactly what transforms Sony applies
- Could implement or approximate their approach

### Option 3: Trust LUT, Skip Dials
LUT-only gives good results (~7-8% loss). Maybe:
- Dial optimization is fundamentally wrong approach
- Focus on better LUT estimation instead
- Accept that LUT handles 90% of the work

## Key Files

| File | Purpose |
|------|---------|
| `LABS/doc/tldr.md` | Architecture overview |
| `LABS/doc/todo.md` | Full research notes, experiment results |
| `LABS/src/main/part/geos/aceo.cpp` | ACEO optimizer (has test code) |
| `LABS/src/main/part/pipe/mods/lut_curve.cpp` | LUT curve estimation |

## Build & Test

```bash
./wire.sh && make                    # Build everything
./desk.sh                            # Launch DESK GUI
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune ...  # Run tune CLI
```

## Research Sources

- [DCPTool - DCP Files](https://dcptool.sourceforge.net/DCP%20FIles.html)
- [DCamProf Camera Profiling](http://rawtherapee.com/mirror/dcamprof/camera-profiling.html)
- [Sony Picture Profile Help](https://helpguide.sony.net/di/pp/v1/en/contents/TP0000909106.html)
