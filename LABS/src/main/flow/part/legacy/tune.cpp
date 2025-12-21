// tune.cpp - Color correction NN with histogram loss
//
// Small NN learns per-pixel color adjustment after TONE.
// Uses histogram EMD loss to preserve color distribution.
//
// Input features (17):
//   - RGB (3)
//   - HSV (3)
//   - local 3x3 saturation variance (1)
//   - local 3x3 luminance variance (1)
//   - spatial position x, y (2)
//   - multi-scale luminance: 4x4, 16x16, 64x64 (3)
//   - multi-scale color at 16x16: RGB (3)
//   - global mean luminance (1)
//
// Output: RGB gain (multiplicative)
//
// Loss: histogram EMD + blurred color MSE

#include <cmath>
#include <random>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>

namespace tune {

constexpr int IN = 17;
constexpr int H1 = 48;   // Increased for more features
constexpr int H2 = 24;
constexpr int OUT = 3;
constexpr int HIST_BINS = 64;

struct Net {
    float w1[IN * H1];
    float b1[H1];
    float w2[H1 * H2];
    float b2[H2];
    float w3[H2 * OUT];
    float b3[OUT];
};

struct Cache {
    float z1[H1], a1[H1];
    float z2[H2], a2[H2];
    float z3[OUT], a3[OUT];
};

// Multi-scale context - precomputed mip levels
struct Context {
    int w, h;
    float global_lum;
    std::vector<float> scale4;   // 4x4 block averages (RGB)
    std::vector<float> scale16;  // 16x16 block averages (RGB)
    std::vector<float> scale64;  // 64x64 block averages (RGB)
    int w4, h4, w16, h16, w64, h64;
};

// RGB to HSV
static void rgb_to_hsv(float r, float g, float b, float& h, float& s, float& v) {
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;

    v = max_c;
    s = (max_c > 0.0001f) ? delta / max_c : 0.0f;

    if (delta < 0.0001f) {
        h = 0.0f;
    } else if (max_c == r) {
        h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
    } else if (max_c == g) {
        h = (b - r) / delta + 2.0f;
    } else {
        h = (r - g) / delta + 4.0f;
    }
    h /= 6.0f;
}

static inline float relu(float x) { return x > 0 ? x : 0; }

// Forward pass
static void forward(const Net& net, const float* x, float* out, Cache* cache = nullptr) {
    float z1[H1], a1[H1], z2[H2], a2[H2], z3[OUT];

    for (int j = 0; j < H1; j++) {
        z1[j] = net.b1[j];
        for (int i = 0; i < IN; i++)
            z1[j] += x[i] * net.w1[i * H1 + j];
        a1[j] = relu(z1[j]);
    }

    for (int j = 0; j < H2; j++) {
        z2[j] = net.b2[j];
        for (int i = 0; i < H1; i++)
            z2[j] += a1[i] * net.w2[i * H2 + j];
        a2[j] = relu(z2[j]);
    }

    for (int j = 0; j < OUT; j++) {
        z3[j] = net.b3[j];
        for (int i = 0; i < H2; i++)
            z3[j] += a2[i] * net.w3[i * OUT + j];
        // Multiplicative gain centered at 1.0: gain = exp(tanh(z) * log_range)
        float t = std::tanh(z3[j]);
        out[j] = std::exp(t * 2.3f);  // gain in [0.1, 10], centered at 1.0
    }

    if (cache) {
        std::memcpy(cache->z1, z1, sizeof(z1));
        std::memcpy(cache->a1, a1, sizeof(a1));
        std::memcpy(cache->z2, z2, sizeof(z2));
        std::memcpy(cache->a2, a2, sizeof(a2));
        std::memcpy(cache->z3, z3, sizeof(z3));
        std::memcpy(cache->a3, out, sizeof(float) * OUT);
    }
}

// Build multi-scale context
static Context build_context(const float* rgb, int w, int h) {
    Context ctx;
    ctx.w = w;
    ctx.h = h;

    // Global mean luminance
    double sum_lum = 0;
    for (int i = 0; i < w * h; i++) {
        float r = rgb[i * 3], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
        sum_lum += 0.299f * r + 0.587f * g + 0.114f * b;
    }
    ctx.global_lum = static_cast<float>(sum_lum / (w * h));

    // 4x4 block averages
    ctx.w4 = (w + 3) / 4;
    ctx.h4 = (h + 3) / 4;
    ctx.scale4.resize(ctx.w4 * ctx.h4 * 3, 0.0f);
    std::vector<int> count4(ctx.w4 * ctx.h4, 0);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int bx = x / 4, by = y / 4;
            int bi = by * ctx.w4 + bx;
            int pi = y * w + x;
            ctx.scale4[bi * 3 + 0] += rgb[pi * 3 + 0];
            ctx.scale4[bi * 3 + 1] += rgb[pi * 3 + 1];
            ctx.scale4[bi * 3 + 2] += rgb[pi * 3 + 2];
            count4[bi]++;
        }
    }
    for (int i = 0; i < ctx.w4 * ctx.h4; i++) {
        if (count4[i] > 0) {
            ctx.scale4[i * 3 + 0] /= count4[i];
            ctx.scale4[i * 3 + 1] /= count4[i];
            ctx.scale4[i * 3 + 2] /= count4[i];
        }
    }

    // 16x16 block averages
    ctx.w16 = (w + 15) / 16;
    ctx.h16 = (h + 15) / 16;
    ctx.scale16.resize(ctx.w16 * ctx.h16 * 3, 0.0f);
    std::vector<int> count16(ctx.w16 * ctx.h16, 0);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int bx = x / 16, by = y / 16;
            int bi = by * ctx.w16 + bx;
            int pi = y * w + x;
            ctx.scale16[bi * 3 + 0] += rgb[pi * 3 + 0];
            ctx.scale16[bi * 3 + 1] += rgb[pi * 3 + 1];
            ctx.scale16[bi * 3 + 2] += rgb[pi * 3 + 2];
            count16[bi]++;
        }
    }
    for (int i = 0; i < ctx.w16 * ctx.h16; i++) {
        if (count16[i] > 0) {
            ctx.scale16[i * 3 + 0] /= count16[i];
            ctx.scale16[i * 3 + 1] /= count16[i];
            ctx.scale16[i * 3 + 2] /= count16[i];
        }
    }

    // 64x64 block averages
    ctx.w64 = (w + 63) / 64;
    ctx.h64 = (h + 63) / 64;
    ctx.scale64.resize(ctx.w64 * ctx.h64 * 3, 0.0f);
    std::vector<int> count64(ctx.w64 * ctx.h64, 0);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int bx = x / 64, by = y / 64;
            int bi = by * ctx.w64 + bx;
            int pi = y * w + x;
            ctx.scale64[bi * 3 + 0] += rgb[pi * 3 + 0];
            ctx.scale64[bi * 3 + 1] += rgb[pi * 3 + 1];
            ctx.scale64[bi * 3 + 2] += rgb[pi * 3 + 2];
            count64[bi]++;
        }
    }
    for (int i = 0; i < ctx.w64 * ctx.h64; i++) {
        if (count64[i] > 0) {
            ctx.scale64[i * 3 + 0] /= count64[i];
            ctx.scale64[i * 3 + 1] /= count64[i];
            ctx.scale64[i * 3 + 2] /= count64[i];
        }
    }

    return ctx;
}

// Extract features for one pixel with context
static void extract_features(const float* rgb, int w, int h, int x, int y,
                             const Context& ctx, float* feat) {
    int idx = (y * w + x) * 3;
    float r = rgb[idx], g = rgb[idx + 1], b = rgb[idx + 2];

    // RGB (3)
    feat[0] = r;
    feat[1] = g;
    feat[2] = b;

    // HSV (3)
    float hue, sat, val;
    rgb_to_hsv(r, g, b, hue, sat, val);
    feat[3] = hue;
    feat[4] = sat;
    feat[5] = val;

    // Local 3x3 variance (2)
    float sum_s = 0, sum_s2 = 0, sum_v = 0, sum_v2 = 0;
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = std::max(0, std::min(w - 1, x + dx));
            int ny = std::max(0, std::min(h - 1, y + dy));
            int ni = (ny * w + nx) * 3;
            float nr = rgb[ni], ng = rgb[ni + 1], nb = rgb[ni + 2];
            float nh, ns, nv;
            rgb_to_hsv(nr, ng, nb, nh, ns, nv);
            sum_s += ns;
            sum_s2 += ns * ns;
            sum_v += nv;
            sum_v2 += nv * nv;
            count++;
        }
    }
    feat[6] = std::sqrt(std::max(0.0f, sum_s2 / count - (sum_s / count) * (sum_s / count)));
    feat[7] = std::sqrt(std::max(0.0f, sum_v2 / count - (sum_v / count) * (sum_v / count)));

    // Spatial position (2)
    feat[8] = static_cast<float>(x) / (w - 1);
    feat[9] = static_cast<float>(y) / (h - 1);

    // Multi-scale luminance: 4x4, 16x16, 64x64 (3)
    int bx4 = std::min(x / 4, ctx.w4 - 1);
    int by4 = std::min(y / 4, ctx.h4 - 1);
    int bi4 = by4 * ctx.w4 + bx4;
    float lum4 = 0.299f * ctx.scale4[bi4 * 3] + 0.587f * ctx.scale4[bi4 * 3 + 1] + 0.114f * ctx.scale4[bi4 * 3 + 2];
    feat[10] = lum4;

    int bx16 = std::min(x / 16, ctx.w16 - 1);
    int by16 = std::min(y / 16, ctx.h16 - 1);
    int bi16 = by16 * ctx.w16 + bx16;
    float lum16 = 0.299f * ctx.scale16[bi16 * 3] + 0.587f * ctx.scale16[bi16 * 3 + 1] + 0.114f * ctx.scale16[bi16 * 3 + 2];
    feat[11] = lum16;

    int bx64 = std::min(x / 64, ctx.w64 - 1);
    int by64 = std::min(y / 64, ctx.h64 - 1);
    int bi64 = by64 * ctx.w64 + bx64;
    float lum64 = 0.299f * ctx.scale64[bi64 * 3] + 0.587f * ctx.scale64[bi64 * 3 + 1] + 0.114f * ctx.scale64[bi64 * 3 + 2];
    feat[12] = lum64;

    // Multi-scale color at 16x16: RGB (3)
    feat[13] = ctx.scale16[bi16 * 3 + 0];
    feat[14] = ctx.scale16[bi16 * 3 + 1];
    feat[15] = ctx.scale16[bi16 * 3 + 2];

    // Global mean luminance (1)
    feat[16] = ctx.global_lum;
}

// Build histogram
static void build_hist(const float* rgb, int w, int h, int channel, double* hist) {
    std::fill(hist, hist + HIST_BINS, 0.0);
    for (int i = 0; i < w * h; i++) {
        float v = std::max(0.0f, std::min(1.0f, rgb[i * 3 + channel]));
        int bin = static_cast<int>(v * (HIST_BINS - 1) + 0.5f);
        hist[bin] += 1.0;
    }
    double total = w * h;
    for (int i = 0; i < HIST_BINS; i++)
        hist[i] /= total;
}

// Earth Mover's Distance between histograms
static double emd(const double* h1, const double* h2) {
    double emd_val = 0;
    double cumsum = 0;
    for (int i = 0; i < HIST_BINS; i++) {
        cumsum += h1[i] - h2[i];
        emd_val += std::abs(cumsum);
    }
    return emd_val / HIST_BINS;
}

// Box blur for local color matching (radius 4)
static void blur(const float* in, float* out, int w, int h) {
    constexpr int R = 4;  // Blur radius - small for local matching
    std::vector<float> tmp(w * h * 3);

    // Horizontal pass
    for (int y = 0; y < h; y++) {
        for (int c = 0; c < 3; c++) {
            float sum = 0;
            int count = 0;
            // Initialize window
            for (int x = 0; x <= R && x < w; x++) {
                sum += in[(y * w + x) * 3 + c];
                count++;
            }
            for (int x = 0; x < w; x++) {
                tmp[(y * w + x) * 3 + c] = sum / count;
                // Slide window
                int left = x - R;
                int right = x + R + 1;
                if (left >= 0) {
                    sum -= in[(y * w + left) * 3 + c];
                    count--;
                }
                if (right < w) {
                    sum += in[(y * w + right) * 3 + c];
                    count++;
                }
            }
        }
    }

    // Vertical pass
    for (int x = 0; x < w; x++) {
        for (int c = 0; c < 3; c++) {
            float sum = 0;
            int count = 0;
            // Initialize window
            for (int y = 0; y <= R && y < h; y++) {
                sum += tmp[(y * w + x) * 3 + c];
                count++;
            }
            for (int y = 0; y < h; y++) {
                out[(y * w + x) * 3 + c] = sum / count;
                // Slide window
                int top = y - R;
                int bottom = y + R + 1;
                if (top >= 0) {
                    sum -= tmp[(top * w + x) * 3 + c];
                    count--;
                }
                if (bottom < h) {
                    sum += tmp[(bottom * w + x) * 3 + c];
                    count++;
                }
            }
        }
    }
}

void init(Net& net, unsigned seed) {
    std::mt19937 rng(seed);
    // Xavier initialization scaled for input size
    float scale1 = std::sqrt(2.0f / IN);
    float scale2 = std::sqrt(2.0f / H1);
    float scale3 = std::sqrt(2.0f / H2);
    std::normal_distribution<float> dist1(0.0f, scale1);
    std::normal_distribution<float> dist2(0.0f, scale2);
    std::normal_distribution<float> dist3(0.0f, scale3);

    for (int i = 0; i < IN * H1; i++) net.w1[i] = dist1(rng);
    for (int i = 0; i < H1; i++) net.b1[i] = 0.0f;
    for (int i = 0; i < H1 * H2; i++) net.w2[i] = dist2(rng);
    for (int i = 0; i < H2; i++) net.b2[i] = 0.0f;
    for (int i = 0; i < H2 * OUT; i++) net.w3[i] = dist3(rng) * 0.1f; // Small init for output
    for (int i = 0; i < OUT; i++) net.b3[i] = 0.0f;
}

// sRGB conversions
static inline float srgb_to_linear(float v) {
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

static inline float linear_to_srgb(float v) {
    v = std::max(0.0f, std::min(1.0f, v));
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

void train(Net& net, const float* tone, const uint8_t* ref, int w, int h,
           int epochs, int samples_per_epoch, float lr) {

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> rand_x(1, w - 2);
    std::uniform_int_distribution<int> rand_y(1, h - 2);

    // Build context for input
    Context ctx = build_context(tone, w, h);

    // Convert reference to linear
    std::vector<float> ref_f(w * h * 3);
    for (int i = 0; i < w * h * 3; i++)
        ref_f[i] = srgb_to_linear(ref[i] / 255.0f);

    // Blur reference for color loss
    std::vector<float> ref_blur(w * h * 3);
    blur(ref_f.data(), ref_blur.data(), w, h);

    // Build target histograms
    double tgt_hist_r[HIST_BINS], tgt_hist_g[HIST_BINS], tgt_hist_b[HIST_BINS];
    build_hist(ref_f.data(), w, h, 0, tgt_hist_r);
    build_hist(ref_f.data(), w, h, 1, tgt_hist_g);
    build_hist(ref_f.data(), w, h, 2, tgt_hist_b);

    float feat[IN], out[OUT], target[OUT];
    Cache cache;
    Net grad;

    // Storage for computing output histograms
    std::vector<float> output_img(w * h * 3);

    for (int epoch = 0; epoch < epochs; epoch++) {
        std::memset(&grad, 0, sizeof(grad));
        double pixel_loss = 0;

        // Generate full output for histogram loss (multiplicative gain)
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                extract_features(tone, w, h, x, y, ctx, feat);
                forward(net, feat, out);
                int idx = (y * w + x) * 3;
                output_img[idx + 0] = std::max(0.0f, std::min(1.0f, feat[0] * out[0]));
                output_img[idx + 1] = std::max(0.0f, std::min(1.0f, feat[1] * out[1]));
                output_img[idx + 2] = std::max(0.0f, std::min(1.0f, feat[2] * out[2]));
            }
        }

        // Compute histogram EMD
        double out_hist_r[HIST_BINS], out_hist_g[HIST_BINS], out_hist_b[HIST_BINS];
        build_hist(output_img.data(), w, h, 0, out_hist_r);
        build_hist(output_img.data(), w, h, 1, out_hist_g);
        build_hist(output_img.data(), w, h, 2, out_hist_b);

        double hist_loss = emd(out_hist_r, tgt_hist_r) +
                          emd(out_hist_g, tgt_hist_g) +
                          emd(out_hist_b, tgt_hist_b);

        // Blur output for color loss
        std::vector<float> out_blur(w * h * 3);
        blur(output_img.data(), out_blur.data(), w, h);

        // Compute blurred color loss
        double color_loss = 0;
        for (int i = 0; i < w * h * 3; i++) {
            float d = out_blur[i] - ref_blur[i];
            color_loss += d * d;
        }
        color_loss /= (w * h * 3);

        // Sample-based gradient update (pixel loss + implicit histogram gradient)
        for (int s = 0; s < samples_per_epoch; s++) {
            int x = rand_x(rng);
            int y = rand_y(rng);

            extract_features(tone, w, h, x, y, ctx, feat);

            int idx = (y * w + x) * 3;
            target[0] = ref_f[idx + 0];
            target[1] = ref_f[idx + 1];
            target[2] = ref_f[idx + 2];

            forward(net, feat, out, &cache);

            // Compute gradient using ONLY blurred color loss (no pixel MSE!)
            // Multiplicative: output = input * gain, gain = exp(tanh(z))
            float d3[OUT];
            for (int j = 0; j < OUT; j++) {
                float gain = cache.a3[j];  // gain in [0.37, 2.72]
                float out_val = std::max(0.0f, std::min(1.0f, feat[j] * gain));

                // Track pixel error for monitoring only
                float pixel_diff = out_val - target[j];
                pixel_loss += pixel_diff * pixel_diff;

                // Blurred color gradient - the ONLY training signal
                float blur_diff = out_blur[idx + j] - ref_blur[idx + j];

                // Gradient: d(loss)/d(gain) = d(loss)/d(out) * input
                // d(gain)/d(z) = gain * 2.3 * (1 - tanh²(z))
                float t = std::log(gain) / 2.3f;  // recover tanh(z) since gain = exp(tanh(z)*2.3)
                float tanh_grad = 2.3f * (1.0f - t * t);
                float gain_grad = gain * tanh_grad;
                d3[j] = 2.0f * blur_diff * feat[j] * gain_grad;
            }

            // Backprop
            float d2[H2] = {};
            for (int i = 0; i < H2; i++) {
                for (int j = 0; j < OUT; j++)
                    d2[i] += d3[j] * net.w3[i * OUT + j];
                d2[i] *= (cache.z2[i] > 0 ? 1.0f : 0.0f);
            }

            float d1[H1] = {};
            for (int i = 0; i < H1; i++) {
                for (int j = 0; j < H2; j++)
                    d1[i] += d2[j] * net.w2[i * H2 + j];
                d1[i] *= (cache.z1[i] > 0 ? 1.0f : 0.0f);
            }

            // Accumulate gradients
            for (int i = 0; i < H2; i++)
                for (int j = 0; j < OUT; j++)
                    grad.w3[i * OUT + j] += cache.a2[i] * d3[j];
            for (int j = 0; j < OUT; j++)
                grad.b3[j] += d3[j];

            for (int i = 0; i < H1; i++)
                for (int j = 0; j < H2; j++)
                    grad.w2[i * H2 + j] += cache.a1[i] * d2[j];
            for (int j = 0; j < H2; j++)
                grad.b2[j] += d2[j];

            for (int i = 0; i < IN; i++)
                for (int j = 0; j < H1; j++)
                    grad.w1[i * H1 + j] += feat[i] * d1[j];
            for (int j = 0; j < H1; j++)
                grad.b1[j] += d1[j];
        }

        // SGD update
        float scale = lr / samples_per_epoch;
        for (int i = 0; i < IN * H1; i++) net.w1[i] -= scale * grad.w1[i];
        for (int i = 0; i < H1; i++) net.b1[i] -= scale * grad.b1[i];
        for (int i = 0; i < H1 * H2; i++) net.w2[i] -= scale * grad.w2[i];
        for (int i = 0; i < H2; i++) net.b2[i] -= scale * grad.b2[i];
        for (int i = 0; i < H2 * OUT; i++) net.w3[i] -= scale * grad.w3[i];
        for (int i = 0; i < OUT; i++) net.b3[i] -= scale * grad.b3[i];

        if (epoch % 10 == 0) {
            std::cerr << "[tune] epoch=" << epoch
                      << " pixel=" << (pixel_loss / (samples_per_epoch * 3) * 100) << "%"
                      << " hist=" << (hist_loss * 100) << "%"
                      << " color=" << (color_loss * 100) << "%"
                      << std::endl;
        }
    }
}

void apply(const Net& net, const float* tone, float* out, int w, int h) {
    // Build context for input
    Context ctx = build_context(tone, w, h);

    float feat[IN], gain[OUT];

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            extract_features(tone, w, h, x, y, ctx, feat);
            forward(net, feat, gain);

            int idx = (y * w + x) * 3;
            out[idx + 0] = std::max(0.0f, std::min(1.0f, feat[0] * gain[0]));
            out[idx + 1] = std::max(0.0f, std::min(1.0f, feat[1] * gain[1]));
            out[idx + 2] = std::max(0.0f, std::min(1.0f, feat[2] * gain[2]));
        }
    }
    std::cerr << "[tune] Applied to " << w << "x" << h << std::endl;
}

} // namespace tune
