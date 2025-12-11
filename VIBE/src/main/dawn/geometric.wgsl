// geometric.wgsl - VIBE Geometric Transform Module
//
// Combined crop, zoom, and rotate using inverse transformation.
// Single pass with bilinear sampling.

struct Uniforms {
    width: u32,
    height: u32,
    crop_top: f32,
    crop_right: f32,
    crop_bottom: f32,
    crop_left: f32,
    zoom_dial: f32,
    tilt_dial: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

const PI: f32 = 3.14159265359;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;
    let w = uniforms.width;
    let h = uniforms.height;

    if (x >= w || y >= h) {
        return;
    }

    let base_idx = (y * w + x) * 3u;

    // Crop parameters
    let ct = uniforms.crop_top * 0.5;
    let cr = uniforms.crop_right * 0.5;
    let cb = uniforms.crop_bottom * 0.5;
    let cl = uniforms.crop_left * 0.5;

    // Zoom and angle
    let zoom = pow(4.0, uniforms.zoom_dial - 0.5);
    let angle = (uniforms.tilt_dial - 0.5) * 90.0 * PI / 180.0;
    let cos_a = cos(angle);
    let sin_a = sin(angle);

    // Cropped region bounds
    let cx = f32(w) * cl;
    let cy = f32(h) * ct;
    let cw_f = max(1.0, f32(w) - f32(w) * (cl + cr));
    let ch_f = max(1.0, f32(h) - f32(h) * (ct + cb));

    // Output center
    let center_x = f32(w) * 0.5;
    let center_y = f32(h) * 0.5;

    // Current position relative to center
    let dx = f32(x) - center_x;
    let dy = f32(y) - center_y;

    // Inverse rotation
    var sx = dx * cos_a + dy * sin_a + center_x;
    var sy = -dx * sin_a + dy * cos_a + center_y;

    // Inverse zoom
    if (zoom > 1.0) {
        // Zoom in: cropped smaller region mapped to full
        let nw = max(1.0, cw_f / zoom);
        let nh = max(1.0, ch_f / zoom);
        let ox = (cw_f - nw) * 0.5;
        let oy = (ch_f - nh) * 0.5;

        sx = ox + sx * nw / cw_f;
        sy = oy + sy * nh / ch_f;
    } else {
        // Zoom out: full region mapped to smaller center
        let nw = max(1.0, cw_f * zoom);
        let nh = max(1.0, ch_f * zoom);
        let ox = (cw_f - nw) * 0.5;
        let oy = (ch_f - nh) * 0.5;

        // Check if inside zoomed region
        if (sx < ox || sx >= ox + nw || sy < oy || sy >= oy + nh) {
            output[base_idx + 0u] = 0.0;
            output[base_idx + 1u] = 0.0;
            output[base_idx + 2u] = 0.0;
            return;
        }

        sx = (sx - ox) * cw_f / nw;
        sy = (sy - oy) * ch_f / nh;
    }

    // Apply crop offset
    sx = sx + cx;
    sy = sy + cy;

    // Clamp to valid range (BORDER_REPLICATE)
    sx = clamp(sx, 0.0, f32(w) - 1.001);
    sy = clamp(sy, 0.0, f32(h) - 1.001);

    // Bilinear interpolation
    let x0 = u32(sx);
    let y0 = u32(sy);
    let x1 = min(x0 + 1u, w - 1u);
    let y1 = min(y0 + 1u, h - 1u);
    let fx = sx - f32(x0);
    let fy = sy - f32(y0);

    let idx00 = (y0 * w + x0) * 3u;
    let idx01 = (y0 * w + x1) * 3u;
    let idx10 = (y1 * w + x0) * 3u;
    let idx11 = (y1 * w + x1) * 3u;

    for (var c = 0u; c < 3u; c++) {
        let v00 = input[idx00 + c];
        let v01 = input[idx01 + c];
        let v10 = input[idx10 + c];
        let v11 = input[idx11 + c];

        let v0 = v00 * (1.0 - fx) + v01 * fx;
        let v1 = v10 * (1.0 - fx) + v11 * fx;
        output[base_idx + c] = v0 * (1.0 - fy) + v1 * fy;
    }
}
