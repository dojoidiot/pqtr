// white_balance.wgsl - VIBE White Balance Module
//
// Tanner Helland's Planckian approximation for color temperature.
// NOTE: All code inlined - WGSL function calls cause issues on some Dawn versions.

struct Uniforms {
    width: u32,
    height: u32,
    temp_dial: f32,
    tint_dial: f32,
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

    // Kelvin from dial (piecewise exponential)
    let temp_dial = clamp(uniforms.temp_dial, 0.0, 1.0);
    var kelvin: f32;
    if (temp_dial < 0.5) {
        let t = temp_dial * 2.0;
        kelvin = 2000.0 * pow(3.25, t);
    } else {
        let t = (temp_dial - 0.5) * 2.0;
        kelvin = 6500.0 * pow(1.846153846, t);
    }

    // Inline kelvin_to_rgb
    let temp = kelvin / 100.0;
    var target_r: f32 = 1.0;
    var target_g: f32 = 1.0;
    var target_b: f32 = 1.0;

    // Red: 1.0 for temp<=66, pow formula for temp>66
    if (temp > 66.0) {
        let rt = temp - 60.0;
        target_r = clamp(329.698727446 * pow(rt, -0.1332047592) / 255.0, 0.0, 1.0);
    }

    // Green: log formula for temp<=66, pow for temp>66
    if (temp <= 66.0) {
        let g_raw = 99.4708025861 * log(temp) - 161.1195681661;
        target_g = clamp(g_raw / 255.0, 0.0, 1.0);
    } else {
        let gt = temp - 60.0;
        target_g = clamp(288.1221695283 * pow(gt, -0.0755148492) / 255.0, 0.0, 1.0);
    }

    // Blue: 1 for temp>=66, 0 for temp<=19, log for middle range
    if (temp >= 66.0) {
        target_b = 1.0;
    } else if (temp <= 19.0) {
        target_b = 0.0;
    } else {
        let bt = temp - 10.0;
        let b_raw = 138.5177312231 * log(bt) - 305.0447927307;
        target_b = clamp(b_raw / 255.0, 0.0, 1.0);
    }

    // Normalize to green
    let norm = 1.0 / max(target_g, 0.001);
    let tr = target_r * norm;
    let tg = 1.0;
    let tb = target_b * norm;

    // Tint shift
    let tint_dial = clamp(uniforms.tint_dial, 0.0, 1.0);
    let tint_shift = (tint_dial - 0.5) * 0.4;
    let green_mult = 1.0 - tint_shift;
    let rb_mult = 1.0 + tint_shift * 0.5;

    // Inverse multipliers
    var r_mult = (1.0 / max(tr, 0.001)) * rb_mult;
    var g_mult = (1.0 / max(tg, 0.001)) * green_mult;
    var b_mult = (1.0 / max(tb, 0.001)) * rb_mult;

    // Normalize to preserve mid-gray
    let avg = (r_mult + g_mult + b_mult) / 3.0;
    r_mult = r_mult / avg;
    g_mult = g_mult / avg;
    b_mult = b_mult / avg;

    // Apply (BGR input)
    output[base_idx + 0u] = input[base_idx + 0u] * b_mult;
    output[base_idx + 1u] = input[base_idx + 1u] * g_mult;
    output[base_idx + 2u] = input[base_idx + 2u] * r_mult;
}
