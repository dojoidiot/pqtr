// local_tone.wgsl - VIBE Local Tone Mapping Module
//
// Simplified bilateral-style local contrast.
// Uses fixed 5x5 window for local mean calculation.

struct Uniforms {
    width: u32,
    height: u32,
    strength: f32,
    delta: f32,
    window_scale: f32,  // Currently unused (fixed 5x5 window)
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

const HALF_WINDOW: i32 = 2;  // 5x5 window

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = i32(gid.x);
    let y = i32(gid.y);
    let w = i32(uniforms.width);
    let h = i32(uniforms.height);

    if (x >= w || y >= h) {
        return;
    }

    let base_idx = u32(y * w + x) * 3u;
    let strength = uniforms.strength;
    let delta = uniforms.delta;

    // Load current pixel (linear RGB)
    let b_lin = input[base_idx + 0u];
    let g_lin = input[base_idx + 1u];
    let r_lin = input[base_idx + 2u];

    // Pixel luminance
    let pixel_lum = max(0.001, 0.2126 * r_lin + 0.7152 * g_lin + 0.0722 * b_lin);

    // Calculate local mean luminance (5x5 window)
    var sum: f32 = 0.0;
    var count: f32 = 0.0;

    for (var dy = -HALF_WINDOW; dy <= HALF_WINDOW; dy++) {
        for (var dx = -HALF_WINDOW; dx <= HALF_WINDOW; dx++) {
            let ny = clamp(y + dy, 0, h - 1);
            let nx = clamp(x + dx, 0, w - 1);
            let idx = u32(ny * w + nx) * 3u;

            let nb = input[idx + 0u];
            let ng = input[idx + 1u];
            let nr = input[idx + 2u];
            let nlum = 0.2126 * nr + 0.7152 * ng + 0.0722 * nb;
            sum += nlum;
            count += 1.0;
        }
    }

    let local_lum = max(0.001, sum / count);

    // Asymmetric weight function
    let aw_num = log(local_lum + delta) - log(delta);
    let aw_den = log(1.0 + delta) - log(delta);
    let asymmetric = aw_num / aw_den;

    // Transform strength based on local luminance
    let f = asymmetric;
    let alpha = 0.5 - 0.5 * tanh(4.0 * f - 2.0);

    // Lift target
    let lift_target = 0.18 + (1.0 - 0.18) * asymmetric;
    let target_lum = local_lum + strength * alpha * (lift_target - local_lum);

    // Scale factor
    var scale = target_lum / local_lum;

    // Suppress highlights
    if (pixel_lum > 0.7) {
        let suppress = max(0.0, 1.0 - (pixel_lum - 0.7) / 0.3);
        scale = 1.0 + (scale - 1.0) * suppress;
    }

    scale = min(scale, 2.0);

    // Apply scale to RGB
    output[base_idx + 0u] = clamp(b_lin * scale, 0.0, 1.0);
    output[base_idx + 1u] = clamp(g_lin * scale, 0.0, 1.0);
    output[base_idx + 2u] = clamp(r_lin * scale, 0.0, 1.0);
}
