// poly_color.wgsl - VIBE Polynomial Color Module
//
// 10-term polynomial color correction per channel.
// Terms: 1, r, g, b, r², g², b², rg, rb, gb

struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
    // Red channel coefficients (10)
    cr0: f32, cr1: f32, cr2: f32, cr3: f32, cr4: f32,
    cr5: f32, cr6: f32, cr7: f32, cr8: f32, cr9: f32,
    _padr0: f32, _padr1: f32,
    // Green channel coefficients (10)
    cg0: f32, cg1: f32, cg2: f32, cg3: f32, cg4: f32,
    cg5: f32, cg6: f32, cg7: f32, cg8: f32, cg9: f32,
    _padg0: f32, _padg1: f32,
    // Blue channel coefficients (10)
    cb0: f32, cb1: f32, cb2: f32, cb3: f32, cb4: f32,
    cb5: f32, cb6: f32, cb7: f32, cb8: f32, cb9: f32,
    _padb0: f32, _padb1: f32,
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

    // Linear to gamma
    let gamma = 2.2;
    let b = pow(clamp(input[base_idx + 0u], 0.0, 1.0), 1.0 / gamma);
    let g = pow(clamp(input[base_idx + 1u], 0.0, 1.0), 1.0 / gamma);
    let r = pow(clamp(input[base_idx + 2u], 0.0, 1.0), 1.0 / gamma);

    // Polynomial terms: 1, r, g, b, r², g², b², rg, rb, gb
    let t0 = 1.0;
    let t1 = r;
    let t2 = g;
    let t3 = b;
    let t4 = r * r;
    let t5 = g * g;
    let t6 = b * b;
    let t7 = r * g;
    let t8 = r * b;
    let t9 = g * b;

    // Apply polynomial for each channel
    var ro = uniforms.cr0 * t0 + uniforms.cr1 * t1 + uniforms.cr2 * t2 + uniforms.cr3 * t3
           + uniforms.cr4 * t4 + uniforms.cr5 * t5 + uniforms.cr6 * t6
           + uniforms.cr7 * t7 + uniforms.cr8 * t8 + uniforms.cr9 * t9;

    var go = uniforms.cg0 * t0 + uniforms.cg1 * t1 + uniforms.cg2 * t2 + uniforms.cg3 * t3
           + uniforms.cg4 * t4 + uniforms.cg5 * t5 + uniforms.cg6 * t6
           + uniforms.cg7 * t7 + uniforms.cg8 * t8 + uniforms.cg9 * t9;

    var bo = uniforms.cb0 * t0 + uniforms.cb1 * t1 + uniforms.cb2 * t2 + uniforms.cb3 * t3
           + uniforms.cb4 * t4 + uniforms.cb5 * t5 + uniforms.cb6 * t6
           + uniforms.cb7 * t7 + uniforms.cb8 * t8 + uniforms.cb9 * t9;

    // Gamma to linear
    ro = pow(clamp(ro, 0.0, 1.0), gamma);
    go = pow(clamp(go, 0.0, 1.0), gamma);
    bo = pow(clamp(bo, 0.0, 1.0), gamma);

    output[base_idx + 0u] = bo;
    output[base_idx + 1u] = go;
    output[base_idx + 2u] = ro;
}
