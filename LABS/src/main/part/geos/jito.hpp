// jito.hpp
// JITO: Jacobian Inference Transfer Optimiser
//
// Fast gradient-based optimizer using pre-computed Jacobian (45×23).
// Infers dial changes from feature differences via pseudoinverse.
// Best as warm-start; linear model breaks down far from neutral.

#pragma once

#include <pipe.hpp>
#include <opencv2/core.hpp>
#include <array>
#include <string>
#include "spsa.hpp"  // For Theta

namespace geos
{

// Result of JITO optimization
struct JitoResult
{
    internal::Theta dials;   // Final dial values (45 dials)
    float final_loss;        // Final loss value
    int iterations;          // Number of iterations taken
};

// Initialize JITO with Jacobian file
// Must be called before jitoOptimize
// Returns false if Jacobian cannot be loaded
bool jitoInit(const std::string& jacobian_path);

// Compute single dial correction step
// Returns suggested dial changes given current vs target features
std::array<float, 45> jitoStep(
    const std::array<float, 23>& current_features,
    const std::array<float, 23>& target_features,
    float learning_rate = 0.5f);

// Full JITO optimization loop
// Takes informed gradient steps until convergence
// body: For rendering (body.view()), link: For setting dials
JitoResult jitoOptimize(
    pipe::Body& body,
    pipe::Body::Link& link,
    const cv::UMat& target,
    const internal::Theta& initial,
    int max_iters = 20,
    float learning_rate = 0.3f,
    float tolerance = 0.02f);

} // namespace geos
