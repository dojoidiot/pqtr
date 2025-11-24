# Geometric

[back](../geometric.md)

**Purpose**: Handles all geometric transformations including cropping, scaling, and rotation. These transformations define the final image composition and framing.

## Sub-Modules

The Geometric module contains 3 sub-modules that work together to transform the image geometry:

### 1. Crop
**Purpose**: Defines the crop rectangle by controlling inset from each edge independently. This allows asymmetric cropping for precise composition control and matching reference image framing.

#### Dials

| Dial           | Range     | Default | Maps To                   | Description                                             |
|----------------|-----------|---------|---------------------------|---------------------------------------------------------|
| `crop_top`       | 0.0 - 1.0 | 0.0     | 0% to 50% inset           | Crops from the top edge inward.                         |
| `crop_right`     | 0.0 - 1.0 | 0.0     | 0% to 50% inset           | Crops from the right edge inward.                       |
| `crop_bottom`    | 0.0 - 1.0 | 0.0     | 0% to 50% inset           | Crops from the bottom edge inward.                      |
| `crop_left`      | 0.0 - 1.0 | 0.0     | 0% to 50% inset           | Crops from the left edge inward.                        |

**Notes**:
- The four dials define an asymmetric crop rectangle with independent control over each edge.
- All default values (0.0) result in no cropping.
- Maximum inset of 50% prevents invalid crop rectangles (top+bottom or left+right cannot exceed 100%).

### 2. Zoom
**Purpose**: Scales the image uniformly from the center. Used for magnifying the subject or adjusting composition scale.

#### Dials

| Dial           | Range     | Default | Maps To                   | Description                                             |
|----------------|-----------|---------|---------------------------|---------------------------------------------------------|
| `zoom`           | 0.0 - 1.0 | 0.0     | 1x to 4x zoom             | Controls the scaling factor. 0.0 is no zoom (1x).       |

**Notes**:
- Zoom is applied uniformly (maintains aspect ratio).
- Zoom is centered on the image center point.
- All default values (0.0) result in no scaling (1x).

### 3. Rotation
**Purpose**: Rotates the image to correct for camera tilt. After rotation, crop may be needed to remove edges.

#### Dials

| Dial           | Range     | Default | Maps To                   | Description                                             |
|----------------|-----------|---------|---------------------------|---------------------------------------------------------|
| `tilt_angle`     | 0.0 - 1.0 | 0.5     | -45° to +45°              | Rotates the image to correct for tilt.                  |

**Notes**:
- Rotation is applied around the image center point.
- Default value (0.5) results in no rotation (0°).
- After rotation, edges may need cropping to remove empty corners (use crop sub-module).

## Total Dials

**6 dials** across all geometric sub-modules (4 + 1 + 1)

## Processing Order

Within the Geometric module, sub-modules are applied in this order:
1. Crop
2. Zoom
3. Rotation

## Implementation Notes

- All geometric transformations are combined into a single affine transformation matrix for efficiency.
- The Geometric module is typically placed in its own dedicated edit step.
- When matching social media images, the geometry step is positioned before tune steps (user sets geometry to match reference framing).
- When doing creative editing, the geometry step is positioned after color grading (final crop/composition).
