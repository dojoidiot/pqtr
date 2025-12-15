// split_tone.wgsl - VIBE Split Tone Module
//
// Applies color grading to shadows and highlights separately.
// Shadow/highlight weights based on luminance.

struct Uniforms {
    width: u32,
    height: u32,
    shadow_temp: f32,
    shadow_tint: f32,
    highlight_temp: f32,
    highlight_tint: f32,
    _pad0: f32,
    _pad1: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    let b = input[base_idx + 0u];
    let g = input[base_idx + 1u];
    let r = input[base_idx + 2u];

    // Rec.709 luminance
    let lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;

    // Shadow weight: high for dark pixels
    let shadow_w = 1.0 - clamp(lum * 2.0, 0.0, 1.0);
    // Highlight weight: high for bright pixels
    let highlight_w = clamp((lum - 0.5) * 2.0, 0.0, 1.0);

    // Shadow color shifts
    let sr = (uniforms.shadow_temp - 0.5) * 0.2;
    let sb = -(uniforms.shadow_temp - 0.5) * 0.2;
    let sg = (uniforms.shadow_tint - 0.5) * 0.1;

    // Highlight color shifts
    let hr = (uniforms.highlight_temp - 0.5) * 0.2;
    let hb = -(uniforms.highlight_temp - 0.5) * 0.2;
    let hg = (uniforms.highlight_tint - 0.5) * 0.1;

    output[base_idx + 0u] = b + shadow_w * sb + highlight_w * hb;
    output[base_idx + 1u] = g + shadow_w * sg + highlight_w * hg;
    output[base_idx + 2u] = r + shadow_w * sr + highlight_w * hr;
}
