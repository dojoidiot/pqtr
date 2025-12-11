// sigmoid.wgsl - VIBE Sigmoid Module
//
// darktable's generalized log-logistic tone mapping.
// NOTE: loglogistic_sigmoid inlined to avoid WGSL function call issues.

struct Uniforms {
    width: u32,
    height: u32,
    contrast: f32,
    skewness: f32,
    white_target: f32,
    black_target: f32,
    _pad0: f32,
    _pad1: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

const MIDDLE_GREY: f32 = 0.1845;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    let contrast = clamp(uniforms.contrast, 0.1, 10.0);
    let skewness = clamp(uniforms.skewness, -1.0, 1.0);
    let white_target = clamp(uniforms.white_target, 0.5, 1.6);
    let black_target = clamp(uniforms.black_target, 0.0, 0.15);

    // Compute sigmoid parameters
    let ref_film_power = contrast;
    let ref_paper_power = 1.0;
    let ref_magnitude = 1.0;
    let ref_film_fog = 0.0;
    let ref_paper_exp = pow(ref_film_fog + MIDDLE_GREY, ref_film_power)
                        * ((ref_magnitude / MIDDLE_GREY) - 1.0);

    let delta = 1e-6;

    // Inline loglogistic_sigmoid(MIDDLE_GREY + delta, ref_magnitude, ref_paper_exp, ref_film_fog, ref_film_power, ref_paper_power)
    var sig_plus: f32;
    {
        let value = MIDDLE_GREY + delta;
        let clamped = max(value, 0.0);
        let film_response = pow(ref_film_fog + clamped, ref_film_power);
        let paper_response = ref_magnitude * pow(film_response / (ref_paper_exp + film_response), ref_paper_power);
        sig_plus = select(paper_response, ref_magnitude, paper_response != paper_response);
    }

    // Inline loglogistic_sigmoid(MIDDLE_GREY - delta, ...)
    var sig_minus: f32;
    {
        let value = MIDDLE_GREY - delta;
        let clamped = max(value, 0.0);
        let film_response = pow(ref_film_fog + clamped, ref_film_power);
        let paper_response = ref_magnitude * pow(film_response / (ref_paper_exp + film_response), ref_paper_power);
        sig_minus = select(paper_response, ref_magnitude, paper_response != paper_response);
    }

    let ref_slope = (sig_plus - sig_minus) / (2.0 * delta);

    let paper_power = pow(5.0, -skewness);

    let temp_film_power = 1.0;
    let temp_white_grey_relation = pow(white_target / MIDDLE_GREY, 1.0 / paper_power) - 1.0;
    let temp_paper_exp = pow(MIDDLE_GREY, temp_film_power) * temp_white_grey_relation;

    // Inline loglogistic_sigmoid(MIDDLE_GREY + delta, white_target, temp_paper_exp, ref_film_fog, temp_film_power, paper_power)
    var temp_sig_plus: f32;
    {
        let value = MIDDLE_GREY + delta;
        let clamped = max(value, 0.0);
        let film_response = pow(ref_film_fog + clamped, temp_film_power);
        let paper_response = white_target * pow(film_response / (temp_paper_exp + film_response), paper_power);
        temp_sig_plus = select(paper_response, white_target, paper_response != paper_response);
    }

    // Inline loglogistic_sigmoid(MIDDLE_GREY - delta, ...)
    var temp_sig_minus: f32;
    {
        let value = MIDDLE_GREY - delta;
        let clamped = max(value, 0.0);
        let film_response = pow(ref_film_fog + clamped, temp_film_power);
        let paper_response = white_target * pow(film_response / (temp_paper_exp + film_response), paper_power);
        temp_sig_minus = select(paper_response, white_target, paper_response != paper_response);
    }

    let temp_slope = (temp_sig_plus - temp_sig_minus) / (2.0 * delta);

    let film_power = ref_slope / temp_slope;

    let white_grey_relation = pow(white_target / MIDDLE_GREY, 1.0 / paper_power) - 1.0;
    let white_black_relation = pow(black_target / white_target, -1.0 / paper_power) - 1.0;

    let film_fog = MIDDLE_GREY * pow(white_grey_relation, 1.0 / film_power)
                   / (pow(white_black_relation, 1.0 / film_power)
                      - pow(white_grey_relation, 1.0 / film_power));

    let paper_exp = pow(film_fog + MIDDLE_GREY, film_power) * white_grey_relation;

    // Load pixel (BGR)
    var b = input[base_idx + 0u];
    var g = input[base_idx + 1u];
    var r = input[base_idx + 2u];

    // Desaturate negative values
    let avg = max((r + g + b) / 3.0, 0.0);
    let min_val = min(min(r, g), b);
    if (min_val < 0.0) {
        let sat_factor = -avg / (min_val - avg);
        r = avg + sat_factor * (r - avg);
        g = avg + sat_factor * (g - avg);
        b = avg + sat_factor * (b - avg);
    }

    // RGB ratio method - compute mapped_luma
    let luma = (r + g + b) / 3.0;

    // Inline loglogistic_sigmoid(luma, white_target, paper_exp, film_fog, film_power, paper_power)
    var mapped_luma: f32;
    {
        let clamped = max(luma, 0.0);
        let film_response = pow(film_fog + clamped, film_power);
        let paper_response = white_target * pow(film_response / (paper_exp + film_response), paper_power);
        mapped_luma = select(paper_response, white_target, paper_response != paper_response);
    }

    if (luma > 1e-9) {
        let scale = mapped_luma / luma;
        r *= scale;
        g *= scale;
        b *= scale;
    } else {
        r = mapped_luma;
        g = mapped_luma;
        b = mapped_luma;
    }

    // Gamut compression
    let pixel_min = min(min(r, g), b);
    let pixel_max = max(max(r, g), b);
    let epsilon = 1e-6;

    let display_border_white = (white_target - mapped_luma) / (pixel_max - mapped_luma + epsilon);
    let display_border_black = (black_target - mapped_luma) / (pixel_min - mapped_luma - epsilon);
    let display_border = min(display_border_white, display_border_black);
    let chroma_border = (mapped_luma - pixel_min) / (mapped_luma + epsilon);

    let chroma_adj = 1.0 / (chroma_border * display_border + epsilon);
    let hyper_chroma = 2.0 * chroma_border / (1.0 - chroma_border * chroma_border + epsilon) * chroma_adj;

    let hyper_z = sqrt(hyper_chroma * hyper_chroma + 1.0);
    let chroma_factor = hyper_chroma / (1.0 + hyper_z) * display_border;

    r = mapped_luma + chroma_factor * (r - mapped_luma);
    g = mapped_luma + chroma_factor * (g - mapped_luma);
    b = mapped_luma + chroma_factor * (b - mapped_luma);

    output[base_idx + 0u] = clamp(b, 0.0, 1.0);
    output[base_idx + 1u] = clamp(g, 0.0, 1.0);
    output[base_idx + 2u] = clamp(r, 0.0, 1.0);
}
