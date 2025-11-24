# Selective Color (Hue Mixer)

[back](../selective_color.md)

**Purpose**: Provides selective color manipulation for brand identity and creative look development.

## Sub-Module: HSL Adjustment

This is a parameterized sub-module that adjusts Hue, Saturation, and Luminance for a specific color band. The same function is instantiated 8 times with different color constants.

### Parameters

**Color Constant** (compile-time parameter):
- `RED`, `ORANGE`, `YELLOW`, `GREEN`, `CYAN`, `BLUE`, `PURPLE`, `MAGENTA`

### Dials (per color instance)

| Dial            | Range     | Default | Maps To         | Transfer Function                     |
|-----------------|-----------|---------|-----------------|---------------------------------------|
| `hue`           | 0.0 - 1.0 | 0.5     | -180° to +180°  | Linear centered: `h = (value - 0.5) * 360` |
| `saturation`    | 0.0 - 1.0 | 0.5     | -100 to +100    | Linear centered: `s = (value - 0.5) * 200` |
| `luminance`     | 0.0 - 1.0 | 0.5     | -100 to +100    | Linear centered: `l = (value - 0.5) * 200` |

**Total**: 3 dials per color × 8 colors = **24 dials**

### Color Constants and Hue Ranges

Each color constant defines a hue range:

| Color Constant | Hue Range      |
|----------------|----------------|
| `RED`          | 330° - 30°     |
| `ORANGE`       | 30° - 60°      |
| `YELLOW`       | 60° - 120°     |
| `GREEN`        | 120° - 180°    |
| `CYAN`         | 180° - 210°    |
| `BLUE`         | 210° - 270°    |
| `PURPLE`       | 270° - 300°    |
| `MAGENTA`      | 300° - 330°    |

### JSON Configuration Example

```json
{
  "selective_color": {
    "enabled": true,
    "red": {
      "hue": 0.5,
      "saturation": 0.6,
      "luminance": 0.5
    },
    "orange": {
      "hue": 0.5,
      "saturation": 0.5,
      "luminance": 0.4
    },
    // ... other colors ...
  }
}
```

**Notes**:
- There is a 30° overlap between adjacent hue ranges with a cosine falloff for smooth, automatic blending.
- All defaults at 0.5 = neutral (no adjustment).
- The function is the same for all colors; only the hue range (color constant) differs.
