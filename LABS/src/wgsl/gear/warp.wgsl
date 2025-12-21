// warp.wgsl - Lens distortion correction
//
// Applies radial distortion correction using Sony spline coefficients.
// For each output pixel, calculates undistorted source position and bilinear samples.

struct WarpUniforms {
    width: u32,
    height: u32,
    knot_count: u32,
    _pad: u32,
    // Distortion spline knots (up to 16)
    knots: array<f32, 16>,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> u: WarpUniforms;

// Linear interpolate spline value at normalized radius r (0=center, 1=corner)
fn interpolate_spline(r: f32) -> f32 {
    let count = i32(u.knot_count);
    if (count <= 0) { return 0.0; }
    if (count == 1) { return u.knots[0]; }

    let r_clamped = clamp(r, 0.0, 1.0);
    let idx = r_clamped * f32(count - 1);
    let i0 = i32(idx);
    let i1 = min(i0 + 1, count - 1);
    let t = idx - f32(i0);

    return u.knots[i0] * (1.0 - t) + u.knots[i1] * t;
}

// Bilinear sample RGB from input
fn sample_rgb(x: f32, y: f32) -> vec3<f32> {
    let w = i32(u.width);
    let h = i32(u.height);

    let x0 = clamp(i32(x), 0, w - 1);
    let y0 = clamp(i32(y), 0, h - 1);
    let x1 = clamp(x0 + 1, 0, w - 1);
    let y1 = clamp(y0 + 1, 0, h - 1);

    let fx = max(0.0, x - f32(i32(x)));
    let fy = max(0.0, y - f32(i32(y)));

    let idx00 = (u32(y0) * u.width + u32(x0)) * 3u;
    let idx10 = (u32(y0) * u.width + u32(x1)) * 3u;
    let idx01 = (u32(y1) * u.width + u32(x0)) * 3u;
    let idx11 = (u32(y1) * u.width + u32(x1)) * 3u;

    var result: vec3<f32>;
    for (var c = 0u; c < 3u; c++) {
        let v00 = input[idx00 + c];
        let v10 = input[idx10 + c];
        let v01 = input[idx01 + c];
        let v11 = input[idx11 + c];

        let v0 = v00 * (1.0 - fx) + v10 * fx;
        let v1 = v01 * (1.0 - fx) + v11 * fx;
        result[c] = v0 * (1.0 - fy) + v1 * fy;
    }
    return result;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = i32(gid.x);
    let y = i32(gid.y);
    let w = i32(u.width);
    let h = i32(u.height);

    if (x >= w || y >= h) { return; }

    // No distortion if no knots
    if (u.knot_count == 0u) {
        let idx = (gid.y * u.width + gid.x) * 3u;
        output[idx + 0u] = input[idx + 0u];
        output[idx + 1u] = input[idx + 1u];
        output[idx + 2u] = input[idx + 2u];
        return;
    }

    // Image center and max radius
    let cx = f32(w) / 2.0;
    let cy = f32(h) / 2.0;
    let r_max = sqrt(cx * cx + cy * cy);

    // Sony scale factor: 2^-14
    let scale = 1.0 / 16384.0;

    // Compute max g(r) for autoscale (prevents black borders)
    var g_max: f32 = 1.0;
    for (var i = 0u; i < u.knot_count; i++) {
        let g = 1.0 + scale * u.knots[i];
        g_max = max(g_max, g);
    }

    // Distance from center
    let dx = f32(x) - cx;
    let dy = f32(y) - cy;
    let r = sqrt(dx * dx + dy * dy);

    var src_x: f32;
    var src_y: f32;

    if (r < 0.5) {
        // Center pixel, no correction needed
        src_x = f32(x);
        src_y = f32(y);
    } else {
        let r_norm = r / r_max;
        let spline_val = interpolate_spline(r_norm);
        let g = 1.0 + scale * spline_val;
        let g_normalized = g / g_max;

        src_x = cx + dx * g_normalized;
        src_y = cy + dy * g_normalized;
    }

    // Bilinear sample and write output
    let rgb = sample_rgb(src_x, src_y);
    let out_idx = (gid.y * u.width + gid.x) * 3u;
    output[out_idx + 0u] = rgb.x;
    output[out_idx + 1u] = rgb.y;
    output[out_idx + 2u] = rgb.z;
}
