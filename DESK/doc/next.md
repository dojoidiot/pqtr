# DESK Next Steps

## Current Status
Tune button integrated with background threading and geos dome animation.
LUT curve transfer added (extracted from tune, stored in desk::Link, applied on render).

## Working
- Tune button triggers background optimizer
- Geos dome animates during optimization
- Priors load from var/etc/ (aceo_full.json, jacob.json)
- Work area displays correctly after tune (close to camera preview)
- Dial changes affect display, undo works

## Not Working
- tail.png doesn't match work area display
- Export PNG doesn't match work area display
- Both appear to be missing the LUT "pop"

## Debug Logging Added
- `[apply_link_dials] Applied LUT / No LUT` - when DESK applies dials
- `[LUT3D] Applying 3D LUT to WxH image` - when LUT module runs
- `[LUT3D] Not estimated, skipping apply` - when LUT not available

## Hypothesis
Work area may accidentally render from head.view() (camera preview) instead of
processed RAW. Need to verify by checking log output:
- When does LUT apply during tune?
- When does LUT apply for tail.png save?
- When does LUT apply for work area render?

## Next Investigation
1. Run tune, check logs for LUT application at each stage
2. Compare image resolutions in logs (preview vs full res)
3. Verify body uses m_data (decoded RAW) not m_view (camera preview)

## Files Changed This Session
- DESK/src/main/part/state.hpp - Added lut3d vector to Link struct
- DESK/src/main/part/files.cpp - Extract/apply LUT, save base.png, debug logging
- LABS/src/main/part/pipe/link.cpp - LUT apply debug logging

## Diagnostic Images Saved
- `<name>.base.png` - neutral dials + sigmoid + gamma (starting point)
- `<name>.tail.png` - tune result from thread
- `<name>.view.png` - camera preview target
- `<name>.diff.png` - difference visualization
