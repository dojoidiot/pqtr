// camera_lut.cpp
// Camera Phase: Direct measurement of camera transform via 3D LUT
//
// Key insight: Don't bin by input (sparse). Bin by OUTPUT (well-distributed).
// Then invert to get input→output mapping.
//
// Algorithm:
// 1. For each unique camera JPEG color, collect all scene-linear inputs that map to it
// 2. Average the inputs per output bin
// 3. This gives us output→input (reverse LUT)
// 4. Invert by swapping: for each input bin, find closest output
//
// Alternative: Direct input→output with non-uniform grid
// - Analyze input distribution
// - Place grid points where data exists
// - Use k-means or percentile-based placement

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <map>

// Tetrahedral interpolation for 3D LUT (more accurate than trilinear)
// Based on the observation that the unit cube can be divided into 6 tetrahedra
void tetrahedral_interp(
    float r, float g, float b,
    const float* lut, int grid,
    float& r_out, float& g_out, float& b_out)
{
    float scale = static_cast<float>(grid - 1);
    float fr = r * scale, fg = g * scale, fb = b * scale;

    int r0 = std::max(0, std::min(grid - 2, static_cast<int>(fr)));
    int g0 = std::max(0, std::min(grid - 2, static_cast<int>(fg)));
    int b0 = std::max(0, std::min(grid - 2, static_cast<int>(fb)));

    float dr = fr - r0, dg = fg - g0, db = fb - b0;

    // Get 8 corner values
    auto idx = [grid](int r, int g, int b, int c) {
        return ((r * grid + g) * grid + b) * 3 + c;
    };

    float c000[3], c001[3], c010[3], c011[3], c100[3], c101[3], c110[3], c111[3];
    for (int c = 0; c < 3; c++) {
        c000[c] = lut[idx(r0, g0, b0, c)];
        c001[c] = lut[idx(r0, g0, b0+1, c)];
        c010[c] = lut[idx(r0, g0+1, b0, c)];
        c011[c] = lut[idx(r0, g0+1, b0+1, c)];
        c100[c] = lut[idx(r0+1, g0, b0, c)];
        c101[c] = lut[idx(r0+1, g0, b0+1, c)];
        c110[c] = lut[idx(r0+1, g0+1, b0, c)];
        c111[c] = lut[idx(r0+1, g0+1, b0+1, c)];
    }

    // Tetrahedral interpolation - 6 cases based on which edge we're closest to
    float result[3];
    if (dr > dg) {
        if (dg > db) {
            // r > g > b
            for (int c = 0; c < 3; c++)
                result[c] = c000[c] + dr*(c100[c]-c000[c]) + dg*(c110[c]-c100[c]) + db*(c111[c]-c110[c]);
        } else if (dr > db) {
            // r > b > g
            for (int c = 0; c < 3; c++)
                result[c] = c000[c] + dr*(c100[c]-c000[c]) + db*(c101[c]-c100[c]) + dg*(c111[c]-c101[c]);
        } else {
            // b > r > g
            for (int c = 0; c < 3; c++)
                result[c] = c000[c] + db*(c001[c]-c000[c]) + dr*(c101[c]-c001[c]) + dg*(c111[c]-c101[c]);
        }
    } else {
        if (db > dg) {
            // b > g > r
            for (int c = 0; c < 3; c++)
                result[c] = c000[c] + db*(c001[c]-c000[c]) + dg*(c011[c]-c001[c]) + dr*(c111[c]-c011[c]);
        } else if (db > dr) {
            // g > b > r
            for (int c = 0; c < 3; c++)
                result[c] = c000[c] + dg*(c010[c]-c000[c]) + db*(c011[c]-c010[c]) + dr*(c111[c]-c011[c]);
        } else {
            // g > r > b
            for (int c = 0; c < 3; c++)
                result[c] = c000[c] + dg*(c010[c]-c000[c]) + dr*(c110[c]-c010[c]) + db*(c111[c]-c110[c]);
        }
    }

    r_out = result[0];
    g_out = result[1];
    b_out = result[2];
}

// Percentile-based grid placement
// Instead of uniform grid, place points at percentiles of actual data distribution
std::vector<float> compute_percentile_grid(const std::vector<float>& values, int grid_size)
{
    std::vector<float> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    std::vector<float> grid(grid_size);
    for (int i = 0; i < grid_size; i++) {
        float pct = static_cast<float>(i) / (grid_size - 1);
        size_t idx = static_cast<size_t>(pct * (sorted.size() - 1));
        grid[i] = sorted[idx];
    }
    return grid;
}

// Find index in non-uniform grid
int find_grid_index(float value, const std::vector<float>& grid)
{
    auto it = std::lower_bound(grid.begin(), grid.end(), value);
    if (it == grid.begin()) return 0;
    if (it == grid.end()) return grid.size() - 1;

    int idx = it - grid.begin();
    // Return the closer one
    if (idx > 0 && (value - grid[idx-1]) < (grid[idx] - value))
        return idx - 1;
    return std::min(idx, static_cast<int>(grid.size()) - 1);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file> [output.png]" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Camera Phase LUT ===" << std::endl;
    std::cout << "RAW: " << raw_path << std::endl;

    // Load RAW and get scene-linear data + camera JPEG
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    // Get scene-linear data (what RAWS decoded)
    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    // Get camera JPEG (embedded preview)
    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    std::cout << "Scene-linear: " << scene_linear.cols << "x" << scene_linear.rows
              << " type=" << scene_linear.type() << std::endl;
    std::cout << "Camera JPEG: " << camera_jpeg.cols << "x" << camera_jpeg.rows
              << " type=" << camera_jpeg.type() << std::endl;

    // Resize scene-linear to match camera JPEG
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Convert scene-linear to gamma space for LUT
    cv::Mat scene_gamma;
    cv::Mat scene_clamped;
    cv::max(scene_resized, 0.0f, scene_clamped);
    cv::min(scene_clamped, 1.0f, scene_clamped);
    cv::pow(scene_clamped, 1.0f/2.2f, scene_gamma);

    // Analyze input distribution
    std::vector<float> r_values, g_values, b_values;
    r_values.reserve(scene_gamma.rows * scene_gamma.cols);
    g_values.reserve(scene_gamma.rows * scene_gamma.cols);
    b_values.reserve(scene_gamma.rows * scene_gamma.cols);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* ptr = scene_gamma.ptr<float>(y);
        for (int x = 0; x < scene_gamma.cols; x++) {
            b_values.push_back(ptr[x*3 + 0]);
            g_values.push_back(ptr[x*3 + 1]);
            r_values.push_back(ptr[x*3 + 2]);
        }
    }

    // Compute percentile-based grids
    const int GRID = 33;
    std::vector<float> r_grid = compute_percentile_grid(r_values, GRID);
    std::vector<float> g_grid = compute_percentile_grid(g_values, GRID);
    std::vector<float> b_grid = compute_percentile_grid(b_values, GRID);

    std::cout << "\nPercentile grid ranges:" << std::endl;
    std::cout << "  R: " << r_grid[0] << " to " << r_grid[GRID-1] << std::endl;
    std::cout << "  G: " << g_grid[0] << " to " << g_grid[GRID-1] << std::endl;
    std::cout << "  B: " << b_grid[0] << " to " << b_grid[GRID-1] << std::endl;

    // Now estimate LUT using percentile-based binning
    int total_cells = GRID * GRID * GRID;
    std::vector<double> sum(total_cells * 3, 0.0);
    std::vector<int> count(total_cells, 0);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* s_ptr = scene_gamma.ptr<float>(y);
        const uchar* t_ptr = camera_jpeg.ptr<uchar>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            float sb = s_ptr[x*3 + 0];
            float sg = s_ptr[x*3 + 1];
            float sr = s_ptr[x*3 + 2];

            // Find grid cell using percentile-based indices
            int ri = find_grid_index(sr, r_grid);
            int gi = find_grid_index(sg, g_grid);
            int bi = find_grid_index(sb, b_grid);

            int cell = (ri * GRID + gi) * GRID + bi;

            // Target values (normalized)
            float tb = t_ptr[x*3 + 0] / 255.0f;
            float tg = t_ptr[x*3 + 1] / 255.0f;
            float tr = t_ptr[x*3 + 2] / 255.0f;

            sum[cell*3 + 0] += tr;
            sum[cell*3 + 1] += tg;
            sum[cell*3 + 2] += tb;
            count[cell]++;
        }
    }

    // Build LUT
    std::vector<float> lut(total_cells * 3);
    int empty = 0, filled = 0;

    for (int ri = 0; ri < GRID; ri++) {
        for (int gi = 0; gi < GRID; gi++) {
            for (int bi = 0; bi < GRID; bi++) {
                int cell = (ri * GRID + gi) * GRID + bi;
                int base = cell * 3;

                if (count[cell] > 0) {
                    lut[base + 0] = sum[base + 0] / count[cell];
                    lut[base + 1] = sum[base + 1] / count[cell];
                    lut[base + 2] = sum[base + 2] / count[cell];
                    filled++;
                } else {
                    // Identity fallback using grid values
                    lut[base + 0] = r_grid[ri];
                    lut[base + 1] = g_grid[gi];
                    lut[base + 2] = b_grid[bi];
                    empty++;
                }
            }
        }
    }

    std::cout << "\nLUT coverage: " << filled << " filled, " << empty << " empty ("
              << (100.0f * filled / total_cells) << "% coverage)" << std::endl;

    // Apply LUT and compute error
    cv::Mat result(scene_gamma.size(), CV_32FC3);

    // For percentile-based LUT, we need to map input values to grid indices
    // This is trickier - we need to interpolate in the non-uniform grid space

    // Simpler approach: convert grid indices back to normalized 0-1 for trilinear interp
    // Create a uniform LUT that maps normalized input to output
    std::vector<float> uniform_lut(GRID * GRID * GRID * 3);

    for (int ri = 0; ri < GRID; ri++) {
        for (int gi = 0; gi < GRID; gi++) {
            for (int bi = 0; bi < GRID; bi++) {
                int cell = (ri * GRID + gi) * GRID + bi;
                // Copy the LUT values (these are already the outputs)
                uniform_lut[cell*3 + 0] = lut[cell*3 + 0];
                uniform_lut[cell*3 + 1] = lut[cell*3 + 1];
                uniform_lut[cell*3 + 2] = lut[cell*3 + 2];
            }
        }
    }

    // Apply using percentile-based lookup
    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* s_ptr = scene_gamma.ptr<float>(y);
        float* r_ptr = result.ptr<float>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            float sb = s_ptr[x*3 + 0];
            float sg = s_ptr[x*3 + 1];
            float sr = s_ptr[x*3 + 2];

            // Map to percentile-based grid coordinates (0-1 range)
            // Find fractional position in the non-uniform grid
            auto grid_pos = [&](float val, const std::vector<float>& grid) -> float {
                if (val <= grid[0]) return 0.0f;
                if (val >= grid[GRID-1]) return 1.0f;

                for (int i = 1; i < GRID; i++) {
                    if (val <= grid[i]) {
                        float t = (val - grid[i-1]) / (grid[i] - grid[i-1]);
                        return (i - 1 + t) / (GRID - 1);
                    }
                }
                return 1.0f;
            };

            float nr = grid_pos(sr, r_grid);
            float ng = grid_pos(sg, g_grid);
            float nb = grid_pos(sb, b_grid);

            float ro, go, bo;
            tetrahedral_interp(nr, ng, nb, uniform_lut.data(), GRID, ro, go, bo);

            r_ptr[x*3 + 0] = bo;
            r_ptr[x*3 + 1] = go;
            r_ptr[x*3 + 2] = ro;
        }
    }

    // Compute error
    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0/255.0);

    cv::Mat diff;
    cv::absdiff(result, target_f, diff);

    cv::Scalar mean_diff = cv::mean(diff);
    float mae = (mean_diff[0] + mean_diff[1] + mean_diff[2]) / 3.0f;

    std::cout << "\n=== RESULT ===" << std::endl;
    std::cout << "Mean Absolute Error: " << (mae * 100.0f) << "%" << std::endl;

    // Save result
    if (argc > 2) {
        cv::Mat result_8u;
        result.convertTo(result_8u, CV_8UC3, 255.0);
        cv::imwrite(argv[2], result_8u);
        std::cout << "Saved: " << argv[2] << std::endl;
    }

    // Also save side-by-side comparison
    cv::Mat comparison;
    cv::Mat target_resized;
    camera_jpeg.convertTo(target_resized, CV_8UC3);

    cv::Mat result_8u;
    result.convertTo(result_8u, CV_8UC3, 255.0);

    cv::hconcat(target_resized, result_8u, comparison);
    cv::imwrite("tmp/var/tune/camera_compare.png", comparison);
    std::cout << "Saved comparison: tmp/var/tune/camera_compare.png" << std::endl;

    return 0;
}
