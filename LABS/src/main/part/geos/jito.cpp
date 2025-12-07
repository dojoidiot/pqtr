// jito.cpp
// JITO: Jacobian Inference Transfer Optimiser
//
// Uses the pre-computed Jacobian (45×23) to take direct gradient steps
// toward the target features, rather than random perturbations.
//
// Key insight: The Jacobian J maps dial changes to feature changes:
//   Δfeatures = J · Δdials
//
// So we can invert this to find the dial changes needed:
//   Δdials = J⁺ · Δfeatures  (using pseudoinverse)
//
// Note: The Jacobian is a linear approximation valid near neutral (0.5).
// For large dial changes, the linear model breaks down. JITO works best
// as a warm-start before stochastic optimizers like ACEO/SPSA.

#include "jito.hpp"
#include "spsa.hpp"  // For Theta, readDials, writeDials
#include "diff.hpp"  // For extractFeatures, geodesicLoss
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <array>
#include <algorithm>

namespace geos
{

// Dimensions
constexpr int NUM_DIALS = 45;
constexpr int NUM_FEATURES = 23;

// Jacobian matrix and its pseudoinverse
static std::array<std::array<float, NUM_FEATURES>, NUM_DIALS> g_jacobian;
static std::array<std::array<float, NUM_DIALS>, NUM_FEATURES> g_pseudoinverse;
static bool g_jacobian_loaded = false;

// Load Jacobian from etc/jacob.json
static bool loadJacobian(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[JITO] Cannot open Jacobian file: " << path << "\n";
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Find "matrix" array in JSON
    size_t matrix_pos = content.find("\"matrix\"");
    if (matrix_pos == std::string::npos)
    {
        std::cerr << "[JITO] No 'matrix' field in Jacobian file\n";
        return false;
    }

    // Parse the matrix (45 rows of 23 values each)
    size_t pos = content.find('[', matrix_pos);
    if (pos == std::string::npos) return false;
    pos++; // Skip outer [

    for (int d = 0; d < NUM_DIALS; d++)
    {
        pos = content.find('[', pos);
        if (pos == std::string::npos) return false;
        pos++;

        for (int f = 0; f < NUM_FEATURES; f++)
        {
            // Skip whitespace
            while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n'))
                pos++;

            // Parse number
            size_t end = pos;
            while (end < content.size() && (std::isdigit(content[end]) ||
                   content[end] == '.' || content[end] == '-' || content[end] == 'e'))
                end++;

            std::string num_str = content.substr(pos, end - pos);
            g_jacobian[d][f] = std::stof(num_str);
            pos = end;

            // Skip comma
            if (pos < content.size() && content[pos] == ',') pos++;
        }

        // Skip to end of row
        pos = content.find(']', pos);
        if (pos == std::string::npos) return false;
        pos++;
    }

    std::cerr << "[JITO] Loaded Jacobian " << NUM_DIALS << "×" << NUM_FEATURES << "\n";
    return true;
}

// Compute pseudoinverse using SVD approximation
// For a tall matrix (45×23), J⁺ = (JᵀJ)⁻¹Jᵀ
// We use regularized pseudoinverse: J⁺ = Jᵀ(JJᵀ + λI)⁻¹
static void computePseudoinverse()
{
    const float lambda = 0.01f; // Regularization to prevent instability

    // Compute JJᵀ (23×23)
    std::array<std::array<float, NUM_FEATURES>, NUM_FEATURES> JJt;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        for (int j = 0; j < NUM_FEATURES; j++)
        {
            float sum = 0.0f;
            for (int d = 0; d < NUM_DIALS; d++)
                sum += g_jacobian[d][i] * g_jacobian[d][j];
            JJt[i][j] = sum + (i == j ? lambda : 0.0f);
        }
    }

    // Invert JJᵀ using Gauss-Jordan elimination
    std::array<std::array<float, NUM_FEATURES>, NUM_FEATURES> inv;
    for (int i = 0; i < NUM_FEATURES; i++)
        for (int j = 0; j < NUM_FEATURES; j++)
            inv[i][j] = (i == j) ? 1.0f : 0.0f;

    for (int i = 0; i < NUM_FEATURES; i++)
    {
        // Find pivot
        float max_val = std::abs(JJt[i][i]);
        int max_row = i;
        for (int k = i + 1; k < NUM_FEATURES; k++)
        {
            if (std::abs(JJt[k][i]) > max_val)
            {
                max_val = std::abs(JJt[k][i]);
                max_row = k;
            }
        }

        // Swap rows
        if (max_row != i)
        {
            std::swap(JJt[i], JJt[max_row]);
            std::swap(inv[i], inv[max_row]);
        }

        // Scale pivot row
        float pivot = JJt[i][i];
        if (std::abs(pivot) < 1e-10f) continue;

        for (int j = 0; j < NUM_FEATURES; j++)
        {
            JJt[i][j] /= pivot;
            inv[i][j] /= pivot;
        }

        // Eliminate column
        for (int k = 0; k < NUM_FEATURES; k++)
        {
            if (k == i) continue;
            float factor = JJt[k][i];
            for (int j = 0; j < NUM_FEATURES; j++)
            {
                JJt[k][j] -= factor * JJt[i][j];
                inv[k][j] -= factor * inv[i][j];
            }
        }
    }

    // Compute J⁺ = Jᵀ · (JJᵀ + λI)⁻¹
    for (int f = 0; f < NUM_FEATURES; f++)
    {
        for (int d = 0; d < NUM_DIALS; d++)
        {
            float sum = 0.0f;
            for (int k = 0; k < NUM_FEATURES; k++)
                sum += g_jacobian[d][k] * inv[k][f];
            g_pseudoinverse[f][d] = sum;
        }
    }

    std::cerr << "[JITO] Computed pseudoinverse " << NUM_FEATURES << "×" << NUM_DIALS << "\n";
}

// Initialize JITO with Jacobian file
bool jitoInit(const std::string& jacobian_path)
{
    if (g_jacobian_loaded) return true;

    if (!loadJacobian(jacobian_path))
        return false;

    computePseudoinverse();
    g_jacobian_loaded = true;
    return true;
}

// Compute dial correction from feature error
// Returns suggested dial changes (in 0-1 dial space)
std::array<float, 45> jitoStep(
    const std::array<float, 23>& current_features,
    const std::array<float, 23>& target_features,
    float learning_rate)
{
    std::array<float, NUM_DIALS> delta_dials;
    delta_dials.fill(0.0f);

    if (!g_jacobian_loaded)
    {
        std::cerr << "[JITO] Jacobian not loaded\n";
        return delta_dials;
    }

    // Compute feature error: Δf = target - current
    std::array<float, NUM_FEATURES> delta_features;
    for (int f = 0; f < NUM_FEATURES; f++)
        delta_features[f] = target_features[f] - current_features[f];

    // Compute dial correction: Δθ = J⁺ · Δf
    for (int d = 0; d < NUM_DIALS; d++)
    {
        float sum = 0.0f;
        for (int f = 0; f < NUM_FEATURES; f++)
            sum += g_pseudoinverse[f][d] * delta_features[f];
        delta_dials[d] = sum * learning_rate;
    }

    return delta_dials;
}

// JITO optimization loop
// Takes informed gradient steps until convergence or max iterations
JitoResult jitoOptimize(
    pipe::Body& body,
    pipe::Body::Link& link,
    const cv::UMat& target,
    const internal::Theta& initial,
    int max_iters,
    float learning_rate,
    float tolerance)
{
    JitoResult result;
    result.iterations = 0;
    result.final_loss = 1.0f;

    if (!g_jacobian_loaded)
    {
        std::cerr << "[JITO] Jacobian not loaded, cannot optimize\n";
        return result;
    }

    // Extract target features
    internal::StyleFeatures target_style = internal::extractStyleFromBGR(target);
    std::array<float, NUM_FEATURES> target_features;
    for (int i = 0; i < NUM_FEATURES; i++)
        target_features[i] = target_style.v[i];

    // Start from initial dials
    internal::Theta theta = initial;
    internal::writeDials(link, theta);

    float prev_loss = 1.0f;
    int stall_count = 0;

    for (int iter = 0; iter < max_iters; iter++)
    {
        // Get current output and features (body.view renders through all links)
        cv::UMat current = body.view(0);
        internal::StyleFeatures current_style = internal::extractStyleFromBGR(current);
        std::array<float, NUM_FEATURES> current_features;
        for (int i = 0; i < NUM_FEATURES; i++)
            current_features[i] = current_style.v[i];

        // Compute loss
        float loss = internal::geodesicLoss(current_style, target_style);
        result.final_loss = loss;
        result.iterations = iter + 1;

        std::cerr << "[JITO] Iter " << iter << ": loss=" << (loss * 100.0f) << "%\n";

        // Check convergence
        if (loss < tolerance)
        {
            std::cerr << "[JITO] Converged at iter " << iter << "\n";
            break;
        }

        // Check stall
        if (std::abs(prev_loss - loss) < 0.001f)
        {
            stall_count++;
            if (stall_count > 5)
            {
                std::cerr << "[JITO] Stalled at iter " << iter << "\n";
                break;
            }
        }
        else
        {
            stall_count = 0;
        }
        prev_loss = loss;

        // Compute and apply dial correction
        auto delta = jitoStep(current_features, target_features, learning_rate);

        // Apply with conservative bounds (Jacobian only valid near neutral)
        // Prevent extreme dial values that break the linear approximation
        for (int d = 0; d < NUM_DIALS; d++)
        {
            theta[d] = std::clamp(theta[d] + delta[d], 0.2f, 0.8f);
        }

        internal::writeDials(link, theta);
    }

    // Store final dials
    result.dials = theta;
    return result;
}

} // namespace geos
