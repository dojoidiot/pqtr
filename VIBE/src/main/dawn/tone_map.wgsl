// tone_map.wgsl - VIBE Tone Mapping Module
//
// Filmic HDR to SDR compression with 7 dials:
// - contrast, highlights, shadows
// - toe_pivot, shoulder_pivot
// - white_point, black_point

struct Uniforms {
    width: u32,
    height: u32,
    contrast_dial: f32,
    highlights_dial: f32,
    shadows_dial: f32,
    toe_pivot_dial: f32,
    shoulder_pivot_dial: f32,
    white_point_dial: f32,
    black_point_dial: f32,
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
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

    // Convert dials to working values
    let contrast = 0.5 * exp(uniforms.contrast_dial * 1.792);
    let highlights = (uniforms.highlights_dial - 0.5) * 2.0;
    let shadows = (uniforms.shadows_dial - 0.5) * 2.0;
    let toe_pivot = 0.1 + uniforms.toe_pivot_dial * 0.4;
    let shoulder_pivot = 0.5 + uniforms.shoulder_pivot_dial * 0.4;
    let bypass_reinhard = (uniforms.white_point_dial > 0.45 && uniforms.white_point_dial < 0.55);
    let white_point = 2.0 + uniforms.white_point_dial * 4.0;
    let black_point = (uniforms.black_point_dial - 0.5) * 0.5;
    let mask_steepness = 12.0;

    // Load pixel (BGR)
    let b = input[base_idx + 0u];
    let g = input[base_idx + 1u];
    let r = input[base_idx + 2u];

    // Rec.709 luminance
    var L = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    let L_orig = max(L, 0.0001);

    // Black point
    if (abs(black_point) > 0.001) {
        if (black_point > 0.0) {
            L = max(L - black_point, 0.0);
            L *= 1.0 / (1.0 - black_point);
        } else {
            let abs_bp = abs(black_point);
            L = L * (1.0 - abs_bp) + abs_bp;
        }
    }

    // Extended Reinhard
    if (!bypass_reinhard) {
        let w2 = white_point * white_point;
        L = (L + L * L / w2) / (1.0 + L);
    }

    // Shadow adjustment
    if (abs(shadows) > 0.01) {
        let shadow_mask = 1.0 / (1.0 + exp(mask_steepness * (L - toe_pivot)));
        let gamma = clamp(1.0 - shadows * 0.5, 0.3, 2.0);
        let adjusted = pow(max(L + 0.001, 0.001), gamma);
        L = adjusted * shadow_mask + L * (1.0 - shadow_mask);
    }

    // Highlight adjustment
    if (abs(highlights) > 0.01) {
        let highlight_mask = 1.0 / (1.0 + exp(-mask_steepness * (L - shoulder_pivot)));
        let gamma = clamp(1.0 + highlights * 0.5, 0.3, 2.0);
        let inv_L = max(1.0 - L, 0.001);
        let adjusted = 1.0 - pow(inv_L, gamma);
        L = adjusted * highlight_mask + L * (1.0 - highlight_mask);
    }

    // Contrast
    if (abs(contrast - 1.0) > 0.01) {
        let centered = L - 0.5;
        let sign_val = select(-1.0, 1.0, centered >= 0.0);
        let abs_val = abs(centered) * 2.0 + 0.001;
        let powered = pow(abs_val, contrast) * 0.5;
        L = sign_val * powered + 0.5;
    }

    L = clamp(L, 0.0, 1.0);

    // Scale RGB by luminance ratio
    let scale = L / L_orig;
    output[base_idx + 0u] = clamp(b * scale, 0.0, 1.0);
    output[base_idx + 1u] = clamp(g * scale, 0.0, 1.0);
    output[base_idx + 2u] = clamp(r * scale, 0.0, 1.0);
}
