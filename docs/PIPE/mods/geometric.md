# Geometric

[back](../pipe.md)

**Purpose**: Handles all geometric transformations including cropping, scaling, and rotation. These transformations define the final image composition and framing.

## Details

See [subm/geometric.md](./subm/geometric.md) for complete sub-module specifications.

The Geometric module contains 3 sub-modules:

### 1. Crop
**Purpose**: Defines the crop rectangle by controlling inset from each edge independently.
**Dials**: 4 (crop_top, crop_right, crop_bottom, crop_left)

### 2. Zoom
**Purpose**: Scales the image uniformly from the center.
**Dials**: 1 (zoom)

### 3. Rotation
**Purpose**: Rotates the image to correct for camera tilt.
**Dials**: 1 (tilt_angle)

## Total Dials

**6 dials** across all geometric sub-modules (4 + 1 + 1)

## Processing Order

Within the Geometric module, sub-modules are applied in this order:
1. Crop
2. Zoom
3. Rotation

## Notes

- All geometric transformations are combined into a single affine transformation matrix for efficiency.
- The Geometric module is typically placed in its own dedicated edit step.
- When matching social media images, the geometry step is positioned before tune steps (user sets geometry to match reference framing).
- When doing creative editing, the geometry step is positioned after color grading (final crop/composition).
