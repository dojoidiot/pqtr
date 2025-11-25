# Data Persistence Specification

[back](../README.md)

## Purpose

This document defines data persistence formats for the PQTR:LABS system. It covers:
- **Pipe Data**: Link configurations and dial values (complete specification)
- **Style Sidecars**: GeoS (.geos.json) and Edge (.edge.json) tuned presets
- **Image Storage**: Linear RGB and display RGB formats
- **Diff Data**: Perceptual difference metrics
- **Tune Data**: Optimization history

---

## Pipe JSON Schema

### Overview

Pipe data persists the complete pipeline configuration from RAW decode (HEAD) through processing steps (BODY) to output (TAIL). The format is designed for:
- Headless operation (diff and tune tools)
- Sparse storage (only set dials are saved)
- Human readability (version control friendly)

**Scope**: This section completely defines the pipe persistence format. Diff and tune formats will reference pipe data but are separate entities (see Forward References).

### Structure

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": [
    {
      "name": "tune_optimize",
      "modules": {
        "geometric": { /* dial values */ },
        "color_correction": { /* dial values */ },
        "tone_mapping": { /* dial values */ },
        "global_color": { /* dial values */ },
        "selective_color": { /* dial values */ },
        "detail": { /* dial values */ }
      }
    }
  ]
}
```

### Dial Encoding

All dial values are normalized to the range `[0.0, 1.0]`:
- **Default**: 0.5 (neutral, no adjustment)
- **Minimum**: 0.0 (maximum negative adjustment)
- **Maximum**: 1.0 (maximum positive adjustment)

**Omitted dials**: If a dial is not present in the JSON, it is treated as unset (not active). Only dials that have been explicitly set are saved.

---

## HEAD Configuration

### Decoder Selection

The `decoder` field specifies which RAW decoder to use in the pipeline HEAD:

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": []
}
```

**Supported decoders:**
- `"sony_arw2"`: Sony .ARW format (default)
- Future: `"nikon_nef"`, `"canon_cr2"`, `"fuji_raf"`, etc.

**Default behavior**: If `decoder` is omitted, the system selects based on file extension.

---

## Module Schemas

### Geometric

```json
"geometric": {
  "crop": {
    "top": 0.0,     // 0.0 = no crop, 1.0 = full crop
    "right": 0.0,
    "bottom": 0.0,
    "left": 0.0
  },
  "zoom": {
    "scale": 0.5    // 0.0 = 0.1x, 0.5 = 1.0x, 1.0 = 10.0x
  },
  "rotation": {
    "tilt_angle": 0.5  // 0.0 = -45°, 0.5 = 0°, 1.0 = +45°
  }
}
```

**Total**: 6 dials

### Color Correction

```json
"color_correction": {
  "white_balance": {
    "temperature": 0.5,  // 0.0 = 2000K, 0.5 = 5500K, 1.0 = 12000K
    "tint": 0.5          // 0.0 = -100, 0.5 = 0, 1.0 = +100
  },
  "exposure": {
    "value": 0.5  // 0.0 = -4 EV, 0.5 = 0 EV, 1.0 = +4 EV
  }
}
```

**Total**: 3 dials

### Tone Mapping

```json
"tone_mapping": {
  "contrast": {
    "value": 0.5  // 0.0 = flat, 0.5 = neutral, 1.0 = maximum
  },
  "curve_adjustment": {
    "highlights": 0.5,  // 0.0 = -2 EV, 0.5 = 0 EV, 1.0 = +2 EV
    "shadows": 0.5
  },
  "clipping_point": {
    "black": 0.15,  // Scene luminance threshold for black
    "white": 0.85   // Scene luminance threshold for white
  }
}
```

**Total**: 5 dials

### Global Color

```json
"global_color": {
  "vibrance": 0.5,      // 0.0 = -1.0, 0.5 = 0, 1.0 = +1.0
  "saturation": 0.5,    // 0.0 = 0.0×, 0.5 = 1.0×, 1.0 = 2.0×
  "color_density": 0.5  // 0.0 = 0.5×, 0.5 = 1.0×, 1.0 = 1.5×
}
```

**Total**: 3 dials

### Selective Color

```json
"selective_color": {
  "red": {
    "hue": 0.5,         // 0.0 = -30°, 0.5 = 0°, 1.0 = +30°
    "saturation": 0.5,  // 0.0 = -1.0, 0.5 = 0, 1.0 = +1.0
    "luminance": 0.5    // 0.0 = -1.0, 0.5 = 0, 1.0 = +1.0
  },
  "orange": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
  "yellow": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
  "green": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
  "cyan": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
  "blue": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
  "purple": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
  "magenta": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 }
}
```

**Total**: 24 dials (8 colors × 3 dials each)

### Detail

```json
"detail": {
  "sharpen": {
    "amount": 0.5,  // 0.0 = none, 1.0 = maximum
    "radius": 0.5   // 0.0 = 0.5px, 0.5 = 1.0px, 1.0 = 2.0px
  },
  "denoise": {
    "luminance": 0.5,  // 0.0 = none, 1.0 = maximum
    "chroma": 0.5
  }
}
```

**Total**: 4 dials

**Grand Total**: 6 + 3 + 5 + 3 + 24 + 4 = **45 dials**

---

## Complete Example

### Minimal Configuration (Bare Defaults)

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": []
}
```

This represents a pipeline with:
- Sony ARW decoder in HEAD
- No processing steps in BODY (direct pass-through to TAIL)
- All modules at default/neutral settings

### Full Configuration Example

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": [
    {
      "name": "geometric_adjust",
      "modules": {
        "geometric": {
          "crop": {
            "top": 0.1,
            "left": 0.05
          },
          "zoom": {
            "scale": 0.6
          }
        }
      }
    },
    {
      "name": "tune_optimize",
      "modules": {
        "color_correction": {
          "exposure": {
            "value": 0.65
          },
          "white_balance": {
            "temperature": 0.52
          }
        },
        "global_color": {
          "saturation": 0.68
        },
        "tone_mapping": {
          "contrast": {
            "value": 0.58
          }
        }
      }
    }
  ]
}
```

**Notes**:
- Link "geometric_adjust" has only 3 dials set (crop top/left, zoom scale)
- Link "tune_optimize" has 4 dials set (exposure, temperature, saturation, contrast)
- All other dials are unset (neutral/inactive)
- Links execute in array order

---

## Save Process

### Determining Set Dials

The `Task::set()` method returns `true` if any dial in that task has been modified from its default state. The save process:

1. **Iterate Links**: Use `Body::all()` to traverse all links
2. **Query Modules**: For each Link, check each module's `Task::set()` state
3. **Serialize Active**: Only serialize modules/dials where `set()` returns `true`
4. **Preserve Order**: Save links in the order they were added to Body

### Implementation Pattern

```cpp
// Pseudocode for save process
json output;
output["version"] = "1.0";
output["links"] = json::array();

for (Link link : body.all()) {
    json linkData;
    linkData["name"] = link.name();
    linkData["modules"] = json::object();

    // Check each module's set() state
    if (link.geometric().set()) {
        linkData["modules"]["geometric"] = serializeGeometric(link.geometric());
    }
    if (link.colorCorrection().set()) {
        linkData["modules"]["color_correction"] = serializeColorCorrection(link.colorCorrection());
    }
    // ... other modules ...

    output["links"].push_back(linkData);
}

sink.save(output.dump());
```

### Sparse Format

Only dials that have been explicitly set are saved:
- **Unset dials**: Omitted from JSON
- **Default dials**: Omitted if value equals default (0.5 for most)
- **Set dials**: Always saved with full precision

This reduces file size and makes configurations easier to read.

---

## Load Process

### Reconstruction

The load process reverses the save process:

1. **Parse JSON**: Load and validate JSON structure
2. **Create Links**: Use `Body::add(name)` for each link in array order
3. **Apply Dials**: For each module present, set dial values via module methods
4. **Validate**: Ensure all values are in `[0.0, 1.0]` range

### Implementation Pattern

```cpp
// Pseudocode for load process
json input = json::parse(sink.read());

if (input["version"] != "1.0") {
    throw std::runtime_error("Unsupported version");
}

for (const auto& linkData : input["links"]) {
    Link link = body.add(linkData["name"]);

    if (linkData["modules"].contains("color_correction")) {
        auto cc = link.colorCorrection();
        auto data = linkData["modules"]["color_correction"];

        if (data.contains("exposure")) {
            cc.exposure().set(data["exposure"]["value"]);
        }
        if (data.contains("white_balance")) {
            cc.whiteBalance().temperature(data["white_balance"]["temperature"]);
            cc.whiteBalance().tint(data["white_balance"]["tint"]);
        }
    }
    // ... other modules ...
}
```

### Validation Requirements

- **Version**: Must be "1.0"
- **Link Names**: Must be unique within the file
- **Dial Values**: Must be in range `[0.0, 1.0]`
- **Module Names**: Must match schema exactly
- **Dial Names**: Must match schema exactly

**Error Handling**: Invalid data should throw exceptions with clear error messages indicating which link/module/dial failed validation.

---

## Image Storage Formats

### Linear RGB Storage

**Purpose**: Store scene-referred, high-dynamic-range linear RGB data after RAW decoding.

- **Format**: OpenEXR (.exr)
- **Encoding**: `CV_32FC3` (float32, 3 channels)
- **Color space**: Camera RGB (white-balanced, scene-referred)
- **Range**: `[0.0, unlimited]` (preserves highlights and full dynamic range)
- **Metadata**: Can embed camera info and white balance coefficients
- **Compression**: Supports lossless compression (e.g., ZIP, PIZ)

**Example Write**:
```cpp
cv::UMat linear_rgb;  // Output from Head.data().view()
cv::imwrite("linear.exr", linear_rgb);
```

**Example Read**:
```cpp
cv::UMat linear_rgb = cv::imread("linear.exr", cv::IMREAD_COLOR | cv::IMREAD_ANYDEPTH);
```

### Display RGB Storage

**Purpose**: Store final, display-referred images ready for output.

- **Format**: PNG (.png) or JPEG (.jpg)
- **Encoding**: `CV_8UC3` (uint8, 3 channels)
- **Color space**: sRGB (display-referred)
- **Range**: `[0, 255]`

**Example Write**:
```cpp
cv::UMat display_rgb;  // Output from Tail.save() via sink
cv::UMat output_8bit;
display_rgb.convertTo(output_8bit, CV_8UC3, 255.0);
cv::imwrite("output.png", output_8bit);
```

---

## Style Sidecar Files

### Purpose

Style presets from the tune tool are stored as **pipe Link sidecars**. Each sidecar is a standard Link that can be added directly to any pipe.json.

See [tune.md](./tune.md) for the tuning process.

### File Types

| File | Content | Dials |
|------|---------|-------|
| `<name>.geos.json` | Color/tone Link | 35 |
| `<name>.edge.json` | Sharpness Link | 4 |

**NOT included:** Geometric dials (6) - user sets per image.

### GeoS Sidecar (.geos.json)

```json
{
  "name": "geos",
  "meta": {
    "loss": 0.0156,
    "iterations": 342,
    "reference": "sunset_beach.png"
  },
  "modules": {
    "color_correction": {
      "white_balance": { "temperature": 0.55, "tint": 0.48 },
      "exposure": { "value": 0.65 }
    },
    "tone_mapping": {
      "contrast": { "value": 0.72 },
      "curve_adjustment": { "highlights": 0.45, "shadows": 0.55 },
      "clipping_point": { "black": 0.15, "white": 0.85 }
    },
    "global_color": {
      "vibrance": 0.55,
      "saturation": 0.68,
      "color_density": 0.52
    },
    "selective_color": {
      "red": { "hue": 0.52, "saturation": 0.55, "luminance": 0.5 },
      "orange": { "hue": 0.5, "saturation": 0.6, "luminance": 0.5 },
      "yellow": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
      "green": { "hue": 0.5, "saturation": 0.45, "luminance": 0.5 },
      "cyan": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
      "blue": { "hue": 0.48, "saturation": 0.55, "luminance": 0.5 },
      "purple": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 },
      "magenta": { "hue": 0.5, "saturation": 0.5, "luminance": 0.5 }
    }
  }
}
```

### Edge Sidecar (.edge.json)

```json
{
  "name": "edge",
  "meta": {
    "loss": 0.0234,
    "reference": "sunset_beach.png"
  },
  "modules": {
    "detail": {
      "sharpen": { "amount": 0.4, "radius": 0.5 },
      "denoise": { "luminance": 0.3, "chroma": 0.5 }
    }
  }
}
```

### Using Style Sidecars

Style sidecars are standard Links - add them to any pipe.json:

```json
{
  "version": "1.0",
  "decoder": "sony_arw2",
  "links": [
    { "name": "framing", "modules": { "geometric": { /* user values */ } } },
    { /* contents of style.geos.json */ },
    { /* contents of style.edge.json */ }
  ]
}
```

### Meta Fields

| Field | Type | Description |
|-------|------|-------------|
| `loss` | float | Final optimization loss |
| `iterations` | int | GeoS only: SPSA iterations |
| `reference` | string | Reference image filename |

### Validation

- **name**: Required, typically "geos" or "edge"
- **modules**: Must match pipe Link schema
- **Values**: All in range [0.0, 1.0]

---

## Diff Output Format

### Purpose

Store perceptual and spectral comparison results for analysis and logging.

### Perceptual Mode Output

```json
{
  "mode": "perceptual",
  "timestamp": "2024-05-21T10:00:00Z",
  "metrics": {
    "ssim": 0.9876,
    "color_diff": 0.0234,
    "lum_diff": 0.0156,
    "total_loss": 0.0234
  },
  "grid_report": {
    "dimensions": "4x4",
    "cells": [
      [0.01, 0.02, 0.15, 0.12],
      [0.01, 0.02, 0.14, 0.11],
      [0.03, 0.03, 0.08, 0.05],
      [0.04, 0.04, 0.06, 0.04]
    ]
  }
}
```

### Spectral Mode Output

```json
{
  "mode": "spectral",
  "timestamp": "2024-05-21T10:00:00Z",
  "spectral_loss": 0.0156,
  "candidate_features": {
    "singular_values": [0.82, 0.31, 0.12],
    "mean_L": 0.45,
    "mean_C": 0.32,
    "std_L": 0.18,
    "std_C": 0.15,
    "skew_L": -0.12,
    "cov_LC": 0.08,
    "cov_HC": 0.05
  },
  "target_features": {
    "singular_values": [0.80, 0.33, 0.11],
    "mean_L": 0.47,
    "mean_C": 0.35,
    "std_L": 0.17,
    "std_C": 0.14,
    "skew_L": -0.10,
    "cov_LC": 0.07,
    "cov_HC": 0.04
  }
}
```

---

## Tune History Format

### Purpose

Store optimization history for analysis, debugging, and reproducibility.

### Greedy Algorithm History

```json
{
  "algorithm": "greedy",
  "timestamp": "2024-05-21T10:00:00Z",
  "config": {
    "sensitivity_threshold": 0.05
  },
  "sensitivity_analysis": {
    "baseline_loss": 0.45,
    "dial_sensitivities": [
      { "dial": "exposure", "sensitivity": 0.32 },
      { "dial": "contrast", "sensitivity": 0.28 },
      { "dial": "saturation", "sensitivity": 0.15 }
    ]
  },
  "optimization": {
    "dials_optimized": 15,
    "iterations_per_dial": [20, 18, 22, 15],
    "final_loss": 0.0234
  },
  "result": {
    "dials": { },
    "computation_time": 7.5
  }
}
```

### SPSA Algorithm History

```json
{
  "algorithm": "spsa",
  "timestamp": "2024-05-21T10:00:00Z",
  "config": {
    "max_iterations": 500,
    "multi_starts": 5,
    "a0": 0.16,
    "c0": 0.05,
    "alpha": 0.602,
    "gamma": 0.101,
    "stability_A": 100
  },
  "runs": [
    {
      "start_index": 0,
      "initial_loss": 0.85,
      "final_loss": 0.0234,
      "iterations": 342,
      "converged": true
    },
    {
      "start_index": 1,
      "initial_loss": 0.78,
      "final_loss": 0.0156,
      "iterations": 298,
      "converged": true
    }
  ],
  "best_run": 1,
  "result": {
    "dials": { },
    "final_loss": 0.0156,
    "computation_time": 45.2
  }
}
```

---

## Implementation Notes

### File Locations

Pipe JSON files should be saved with `.json` extension and stored alongside processed images or in a dedicated configuration directory.

### Encoding

- **Character Set**: UTF-8
- **Indentation**: 2 spaces (for human readability)
- **Precision**: Float values should use at least 4 decimal places

### Compatibility

This format is designed to be forward-compatible. Future versions may add:
- Additional metadata fields
- New module types (with version checks)
- Compression options

Version "1.0" readers should ignore unknown fields gracefully.

---

## Next Steps

To complete the pipe::data implementation:

1. **Implement Serialization**: Add save/load methods to pipe implementation
2. **Add Validation**: Implement strict JSON schema validation
3. **Write Tests**: Test save/load roundtrip with various configurations
4. **Define Diff Format**: Specify diff data schema and pipe data references
5. **Define Tune Format**: Specify tune data schema and optimization tracking
