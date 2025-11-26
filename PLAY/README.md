# PQTR:PLAY

[back](../README.md)

Consumer mobile app for iOS and Android. Apply DESK-created styles to phone camera DNGs.

## Role in PQTR

PLAY bridges professional workflows to casual users. Photographers create styles in DESK, consumers apply them in PLAY.

```
[DESK] ──► Styles (.pipe.json) ──► [PLAY] ──► Styled photos
                                     │
                                     └── Phone DNG → LABS pipe → Output
```

- **Platform**: iOS and Android (standard app stores)
- **Purpose**: Apply pre-made styles to phone RAW photos
- **Audience**: Consumers, casual photographers

## Concept

| Feature | Description |
|---------|-------------|
| Capture | Use phone camera to shoot DNG |
| Styles | Download/import styles from DESK users |
| Process | Apply style via LABS pipe |
| Share | Export to camera roll, social media |

## User Experience

1. **Shoot**: Take photo with phone camera (DNG mode)
2. **Style**: Browse available styles (from DESK creators)
3. **Apply**: One-tap to apply style
4. **Share**: Export to camera roll or share directly

## Style Marketplace (Future)

- DESK users can publish styles
- PLAY users can browse and download
- Possible monetization for style creators

## Technical Notes

- LABS pipe compiled for ARM (iOS/Android native)
- DNG support via RAWS (phone cameras)
- Styles are `.pipe.json` files
- Lightweight UI (native platform widgets)

## Status

**Planned** - Not yet implemented.

## Dependencies

- LABS pipe library (cross-compiled for ARM)
- RAWS with DNG support
- Platform-specific UI (SwiftUI/Jetpack Compose)
