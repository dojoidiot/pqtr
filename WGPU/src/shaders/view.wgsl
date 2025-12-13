// view.wgsl - Display preview compute shader
//
// Quantizes float32 RGBA to uint8 RGBA for display texture.
// This is for PREVIEW ONLY - not saved, maintains pipeline fidelity.
//
// The float32 source buffer remains unchanged (lossless).
// The uint8 output is a temporary display texture.

// ============================================================
// Bindings
// ============================================================

struct Params {
    width: u32,
    height: u32,
    gamma: f32,      // Display gamma (typically 2.2 or 1.0 for linear)
    exposure: f32,   // Exposure adjustment (typically 1.0)
}

// Input: float32 RGBA (pipeline data - NOT modified)
@group(0) @binding(0) var<storage, read> pixels: array<vec4f>;

// Output: uint8 RGBA packed as u32 (display texture - temporary)
@group(0) @binding(1) var<storage, read_write> display: array<u32>;

@group(0) @binding(2) var<uniform> params: Params;

// ============================================================
// Tone mapping helpers
// ============================================================

// Simple Reinhard tone mapping
fn tonemap_reinhard(x: f32) -> f32 {
    return x / (1.0 + x);
}

// sRGB gamma encoding
fn gamma_encode(linear: f32, gamma: f32) -> f32 {
    if (gamma == 1.0) {
        return linear;
    }
    return pow(max(linear, 0.0), 1.0 / gamma);
}

// ============================================================
// Main compute kernel
// ============================================================

@compute @workgroup_size(16, 16)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let x = gid.x;
    let y = gid.y;

    if (x >= params.width || y >= params.height) {
        return;
    }

    let idx = y * params.width + x;
    let pixel = pixels[idx];

    // Apply exposure
    var rgb = pixel.rgb * params.exposure;

    // Optional: tone mapping for HDR content
    // rgb = vec3f(tonemap_reinhard(rgb.r), tonemap_reinhard(rgb.g), tonemap_reinhard(rgb.b));

    // Apply gamma encoding for display
    rgb = vec3f(
        gamma_encode(rgb.r, params.gamma),
        gamma_encode(rgb.g, params.gamma),
        gamma_encode(rgb.b, params.gamma)
    );

    // Quantize to 8-bit (this is the display-only lossy step)
    let r = u32(clamp(rgb.r, 0.0, 1.0) * 255.0 + 0.5);
    let g = u32(clamp(rgb.g, 0.0, 1.0) * 255.0 + 0.5);
    let b = u32(clamp(rgb.b, 0.0, 1.0) * 255.0 + 0.5);
    let a = u32(clamp(pixel.a, 0.0, 1.0) * 255.0 + 0.5);

    // Pack as RGBA (little-endian: R in low byte)
    display[idx] = r | (g << 8u) | (b << 16u) | (a << 24u);
}
