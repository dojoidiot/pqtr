// global_color.wgsl - VIBE Global Color Module
//
// Vibrance (with skin protection), Saturation, Color Density
// NOTE: All Lab conversions inlined to avoid WGSL function call issues.

struct Uniforms {
    width: u32,
    height: u32,
    vibrance_dial: f32,
    saturation_dial: f32,
    density_dial: f32,
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

const PI: f32 = 3.14159265359;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    // Load BGR
    var b = input[base_idx + 0u];
    var g = input[base_idx + 1u];
    var r = input[base_idx + 2u];

    // Parameters
    let vibrance = (uniforms.vibrance_dial - 0.5) * 2.0;
    let saturation = uniforms.saturation_dial * 2.0;
    let color_density = 0.5 + uniforms.density_dial;

    // Check if neutral
    let is_neutral = abs(vibrance) <= 0.01 && abs(saturation - 1.0) <= 0.01 && abs(color_density - 1.0) <= 0.01;
    if (is_neutral) {
        output[base_idx + 0u] = b;
        output[base_idx + 1u] = g;
        output[base_idx + 2u] = r;
        return;
    }

    // RGB to Lab (inline)
    // Gamma expand
    let gamma = 2.2;
    let r_lin = clamp(r, 0.0, 1.0);
    let g_lin = clamp(g, 0.0, 1.0);
    let b_lin = clamp(b, 0.0, 1.0);
    let r_gamma = pow(r_lin, 1.0 / gamma);
    let g_gamma = pow(g_lin, 1.0 / gamma);
    let b_gamma = pow(b_lin, 1.0 / gamma);

    // RGB to XYZ (sRGB D65)
    let X = r_gamma * 0.4124564 + g_gamma * 0.3575761 + b_gamma * 0.1804375;
    let Y = r_gamma * 0.2126729 + g_gamma * 0.7151522 + b_gamma * 0.0721750;
    let Z = r_gamma * 0.0193339 + g_gamma * 0.1191920 + b_gamma * 0.9503041;

    // XYZ to Lab (D65 reference)
    let Xn = 0.95047;
    let Yn = 1.0;
    let Zn = 1.08883;

    let delta = 6.0 / 29.0;
    let delta3 = delta * delta * delta;

    var fx: f32;
    var fy: f32;
    var fz: f32;

    let xr = X / Xn;
    if (xr > delta3) { fx = pow(xr, 1.0 / 3.0); }
    else { fx = xr / (3.0 * delta * delta) + 4.0 / 29.0; }

    let yr = Y / Yn;
    if (yr > delta3) { fy = pow(yr, 1.0 / 3.0); }
    else { fy = yr / (3.0 * delta * delta) + 4.0 / 29.0; }

    let zr = Z / Zn;
    if (zr > delta3) { fz = pow(zr, 1.0 / 3.0); }
    else { fz = zr / (3.0 * delta * delta) + 4.0 / 29.0; }

    var L = 116.0 * fy - 16.0;
    var a = 500.0 * (fx - fy);
    var lab_b = 200.0 * (fy - fz);

    // Vibrance (with skin protection)
    if (abs(vibrance) > 0.01) {
        let C = sqrt(a * a + lab_b * lab_b);
        var hue = atan2(lab_b, a) * 180.0 / PI;
        if (hue < 0.0) { hue += 360.0; }

        // Skin tone protection (centered around 45 degrees = orange/skin)
        let skin_center = hue - 45.0;
        let skin_mask = exp(-(skin_center * skin_center) / 450.0);

        // Low saturation weight
        var vib_weight = 1.0 - clamp(C / 100.0, 0.0, 1.0);

        // Protect skin tones from positive vibrance
        if (vibrance > 0.0) {
            let prot = (1.0 - skin_mask) * 0.7 + 0.3;
            vib_weight *= prot;
        }

        let boost = 1.0 + vib_weight * vibrance;
        a *= boost;
        lab_b *= boost;
    }

    // Saturation
    if (abs(saturation - 1.0) > 0.01) {
        a *= saturation;
        lab_b *= saturation;
    }

    // Color density
    if (abs(color_density - 1.0) > 0.01) {
        a *= color_density;
        lab_b *= color_density;
        let l_contrast = 1.0 + (color_density - 1.0) * 0.3;
        L = (L - 50.0) * l_contrast + 50.0;
    }

    L = clamp(L, 0.0, 100.0);

    // Lab to RGB (inline)
    let fy2 = (L + 16.0) / 116.0;
    let fx2 = a / 500.0 + fy2;
    let fz2 = fy2 - lab_b / 200.0;

    var xr2: f32;
    var yr2: f32;
    var zr2: f32;

    if (fx2 > delta) { xr2 = fx2 * fx2 * fx2; }
    else { xr2 = (fx2 - 4.0 / 29.0) * 3.0 * delta * delta; }

    if (fy2 > delta) { yr2 = fy2 * fy2 * fy2; }
    else { yr2 = (fy2 - 4.0 / 29.0) * 3.0 * delta * delta; }

    if (fz2 > delta) { zr2 = fz2 * fz2 * fz2; }
    else { zr2 = (fz2 - 4.0 / 29.0) * 3.0 * delta * delta; }

    let X2 = xr2 * Xn;
    let Y2 = yr2 * Yn;
    let Z2 = zr2 * Zn;

    // XYZ to RGB
    var r_out = X2 *  3.2404542 + Y2 * -1.5371385 + Z2 * -0.4985314;
    var g_out = X2 * -0.9692660 + Y2 *  1.8760108 + Z2 *  0.0415560;
    var b_out = X2 *  0.0556434 + Y2 * -0.2040259 + Z2 *  1.0572252;

    // Gamma compress
    r_out = pow(clamp(r_out, 0.0, 1.0), gamma);
    g_out = pow(clamp(g_out, 0.0, 1.0), gamma);
    b_out = pow(clamp(b_out, 0.0, 1.0), gamma);

    output[base_idx + 0u] = clamp(b_out, 0.0, 1.0);
    output[base_idx + 1u] = clamp(g_out, 0.0, 1.0);
    output[base_idx + 2u] = clamp(r_out, 0.0, 1.0);
}
