// selective_color.wgsl - VIBE Selective Color Module
//
// 8-band hue-selective HSL adjustments (24 dials total).
// All helper functions inlined to avoid WGSL function call issues.

struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
    // 8 hue dials
    hue0: f32, hue1: f32, hue2: f32, hue3: f32,
    hue4: f32, hue5: f32, hue6: f32, hue7: f32,
    // 8 saturation dials
    sat0: f32, sat1: f32, sat2: f32, sat3: f32,
    sat4: f32, sat5: f32, sat6: f32, sat7: f32,
    // 8 luminance dials
    lum0: f32, lum1: f32, lum2: f32, lum3: f32,
    lum4: f32, lum5: f32, lum6: f32, lum7: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

const PI: f32 = 3.14159265359;
const HUE_RANGE: f32 = 45.0;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    // Load BGR and convert to gamma
    let gamma = 2.2;
    var r = pow(clamp(input[base_idx + 2u], 0.0, 1.0), 1.0 / gamma);
    var g = pow(clamp(input[base_idx + 1u], 0.0, 1.0), 1.0 / gamma);
    var b = pow(clamp(input[base_idx + 0u], 0.0, 1.0), 1.0 / gamma);

    // Hue adjustment values (dial-0.5)*60
    var hue_adj: array<f32, 8>;
    hue_adj[0] = (clamp(uniforms.hue0, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[1] = (clamp(uniforms.hue1, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[2] = (clamp(uniforms.hue2, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[3] = (clamp(uniforms.hue3, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[4] = (clamp(uniforms.hue4, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[5] = (clamp(uniforms.hue5, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[6] = (clamp(uniforms.hue6, 0.0, 1.0) - 0.5) * 60.0;
    hue_adj[7] = (clamp(uniforms.hue7, 0.0, 1.0) - 0.5) * 60.0;

    // Saturation adjustment values (dial-0.5)*2
    var sat_adj: array<f32, 8>;
    sat_adj[0] = (clamp(uniforms.sat0, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[1] = (clamp(uniforms.sat1, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[2] = (clamp(uniforms.sat2, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[3] = (clamp(uniforms.sat3, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[4] = (clamp(uniforms.sat4, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[5] = (clamp(uniforms.sat5, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[6] = (clamp(uniforms.sat6, 0.0, 1.0) - 0.5) * 2.0;
    sat_adj[7] = (clamp(uniforms.sat7, 0.0, 1.0) - 0.5) * 2.0;

    // Luminance adjustment values (dial-0.5)*2
    var lum_adj: array<f32, 8>;
    lum_adj[0] = (clamp(uniforms.lum0, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[1] = (clamp(uniforms.lum1, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[2] = (clamp(uniforms.lum2, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[3] = (clamp(uniforms.lum3, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[4] = (clamp(uniforms.lum4, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[5] = (clamp(uniforms.lum5, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[6] = (clamp(uniforms.lum6, 0.0, 1.0) - 0.5) * 2.0;
    lum_adj[7] = (clamp(uniforms.lum7, 0.0, 1.0) - 0.5) * 2.0;

    // Hue centers: Red, Orange, Yellow, Green, Cyan, Blue, Purple, Magenta
    var hue_centers: array<f32, 8>;
    hue_centers[0] = 0.0;
    hue_centers[1] = 45.0;
    hue_centers[2] = 90.0;
    hue_centers[3] = 150.0;
    hue_centers[4] = 195.0;
    hue_centers[5] = 240.0;
    hue_centers[6] = 285.0;
    hue_centers[7] = 315.0;

    // RGB to HLS (inline)
    let max_c = max(max(r, g), b);
    let min_c = min(min(r, g), b);
    var l = (max_c + min_c) * 0.5;
    var h: f32 = 0.0;
    var s: f32 = 0.0;

    if (max_c != min_c) {
        let d = max_c - min_c;
        if (l > 0.5) {
            s = d / (2.0 - max_c - min_c);
        } else {
            s = d / (max_c + min_c);
        }

        if (max_c == r) {
            h = (g - b) / d;
            if (g < b) { h += 6.0; }
        } else if (max_c == g) {
            h = (b - r) / d + 2.0;
        } else {
            h = (r - g) / d + 4.0;
        }
        h *= 60.0;
    }

    // Apply selective adjustments
    var total_h: f32 = 0.0;
    var total_s: f32 = 0.0;
    var total_l: f32 = 0.0;
    var total_w: f32 = 0.0;

    for (var band = 0u; band < 8u; band++) {
        // Calculate hue weight (inline)
        var pixel_hue = h;
        if (pixel_hue < 0.0) { pixel_hue += 360.0; }
        if (pixel_hue >= 360.0) { pixel_hue -= 360.0; }

        var diff = abs(pixel_hue - hue_centers[band]);
        if (diff > 180.0) { diff = 360.0 - diff; }

        var w: f32 = 0.0;
        if (diff <= HUE_RANGE) {
            w = 0.5 * (1.0 + cos(PI * diff / HUE_RANGE));
        }

        if (w > 0.001) {
            total_h += w * hue_adj[band];
            total_s += w * sat_adj[band];
            total_l += w * lum_adj[band];
            total_w += w;
        }
    }

    if (total_w > 0.001) {
        let norm = 1.0 / total_w;
        total_h *= norm;
        total_s *= norm;
        total_l *= norm;

        h += total_h;
        if (h < 0.0) { h += 360.0; }
        if (h >= 360.0) { h -= 360.0; }

        if (total_s > 0.0) {
            s = s + (1.0 - s) * total_s;
        } else {
            s = s * (1.0 + total_s);
        }

        if (total_l > 0.0) {
            l = l + (1.0 - l) * total_l * 0.5;
        } else {
            l = l * (1.0 + total_l * 0.5);
        }
    }

    s = clamp(s, 0.0, 1.0);
    l = clamp(l, 0.0, 1.0);

    // HLS to RGB (inline)
    if (s == 0.0) {
        r = l; g = l; b = l;
    } else {
        var q: f32;
        if (l < 0.5) {
            q = l * (1.0 + s);
        } else {
            q = l + s - l * s;
        }
        let p = 2.0 * l - q;
        let h_norm = h / 360.0;

        // Red
        var t = h_norm + 1.0/3.0;
        if (t < 0.0) { t += 1.0; }
        if (t > 1.0) { t -= 1.0; }
        if (t < 1.0/6.0) { r = p + (q - p) * 6.0 * t; }
        else if (t < 0.5) { r = q; }
        else if (t < 2.0/3.0) { r = p + (q - p) * (2.0/3.0 - t) * 6.0; }
        else { r = p; }

        // Green
        t = h_norm;
        if (t < 0.0) { t += 1.0; }
        if (t > 1.0) { t -= 1.0; }
        if (t < 1.0/6.0) { g = p + (q - p) * 6.0 * t; }
        else if (t < 0.5) { g = q; }
        else if (t < 2.0/3.0) { g = p + (q - p) * (2.0/3.0 - t) * 6.0; }
        else { g = p; }

        // Blue
        t = h_norm - 1.0/3.0;
        if (t < 0.0) { t += 1.0; }
        if (t > 1.0) { t -= 1.0; }
        if (t < 1.0/6.0) { b = p + (q - p) * 6.0 * t; }
        else if (t < 0.5) { b = q; }
        else if (t < 2.0/3.0) { b = p + (q - p) * (2.0/3.0 - t) * 6.0; }
        else { b = p; }
    }

    // Gamma to linear
    r = pow(clamp(r, 0.0, 1.0), gamma);
    g = pow(clamp(g, 0.0, 1.0), gamma);
    b = pow(clamp(b, 0.0, 1.0), gamma);

    output[base_idx + 0u] = b;
    output[base_idx + 1u] = g;
    output[base_idx + 2u] = r;
}
