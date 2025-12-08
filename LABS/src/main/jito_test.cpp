// jito_test.cpp
// Test JITO optimizer on a sample image

#include <pipe.hpp>
#include <pqtr.hpp>
#include "part/geos/jito.hpp"
#include "part/geos/spsa.hpp"
#include "part/geos/diff.hpp"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <chrono>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <image.ARW> <target.jpg> [jacobian.json]\n";
        std::cerr << "\nTests JITO optimizer vs HYBRID on the same image.\n";
        return 1;
    }

    std::string raw_path = argv[1];
    std::string target_path = argv[2];
    std::string jacob_path = argc > 3 ? argv[3] : "etc/jacob.json";

    // Load RAW
    std::cerr << "Loading RAW: " << raw_path << "\n";
    auto sink = pqtr::sink(raw_path);
    auto pipe = pipe::make();
    auto head = pipe->open(sink);

    if (!head)
    {
        std::cerr << "Failed to decode RAW\n";
        return 1;
    }

    // Load target
    std::cerr << "Loading target: " << target_path << "\n";
    cv::Mat target_mat = cv::imread(target_path);
    if (target_mat.empty())
    {
        std::cerr << "Failed to load target\n";
        return 1;
    }
    cv::UMat target;
    target_mat.convertTo(target_mat, CV_32FC3, 1.0/255.0);
    target_mat.copyTo(target);

    // Initialize JITO
    std::cerr << "Initializing JITO with: " << jacob_path << "\n";
    if (!geos::jitoInit(jacob_path))
    {
        std::cerr << "Failed to load Jacobian\n";
        return 1;
    }

    // Create body at working size
    auto& body = head->body(1024);

    // Add Base link
    auto& base = body.add("Base");
    base.colorCorrection().exposure().setDial(0.5f);

    // Get initial dials (neutral)
    geos::internal::Theta initial;
    initial.fill(0.5f);

    // Extract target features for comparison
    auto target_features = geos::internal::extractFeatures(target);
    std::cerr << "\nTarget features:\n";
    std::cerr << "  L: μ=" << target_features[3] << " σ=" << target_features[5] << "\n";
    std::cerr << "  C: μ=" << target_features[4] << " σ=" << target_features[6] << "\n";

    // Test JITO
    std::cerr << "\n=== JITO Optimization ===\n";
    auto jito_start = std::chrono::high_resolution_clock::now();

    auto jito_result = geos::jitoOptimize(body, target, initial, 20, 0.3f, 0.02f);

    auto jito_end = std::chrono::high_resolution_clock::now();
    auto jito_ms = std::chrono::duration_cast<std::chrono::milliseconds>(jito_end - jito_start).count();

    std::cerr << "\nJITO Result:\n";
    std::cerr << "  Iterations: " << jito_result.iterations << "\n";
    std::cerr << "  Final loss: " << (jito_result.final_loss * 100.0f) << "%\n";
    std::cerr << "  Time: " << jito_ms << "ms\n";

    // Show key dial values
    std::cerr << "  Key dials:\n";
    std::cerr << "    exposure:    " << jito_result.dials[0] << "\n";
    std::cerr << "    temperature: " << jito_result.dials[1] << "\n";
    std::cerr << "    contrast:    " << jito_result.dials[3] << "\n";
    std::cerr << "    shadows:     " << jito_result.dials[5] << "\n";
    std::cerr << "    vibrance:    " << jito_result.dials[10] << "\n";
    std::cerr << "    saturation:  " << jito_result.dials[11] << "\n";

    // Save JITO output
    body.tail().save("tmp/jito_output.png", 2048);
    std::cerr << "Saved: tmp/jito_output.png\n";

    // Reset to neutral for HYBRID comparison
    geos::internal::writeDials(body, initial);

    // Test HYBRID for comparison
    std::cerr << "\n=== HYBRID Optimization (for comparison) ===\n";
    auto hybrid_start = std::chrono::high_resolution_clock::now();

    // Run ACEO (simplified - just a few iterations)
    geos::internal::Theta hybrid_dials = initial;
    float hybrid_loss = 1.0f;

    // Simple SPSA-style optimization for comparison
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pert_dist(-0.1f, 0.1f);

    for (int iter = 0; iter < 50; iter++)
    {
        // Perturb
        geos::internal::Theta perturbed = hybrid_dials;
        for (int d = 0; d < 45; d++)
            perturbed[d] = std::clamp(perturbed[d] + pert_dist(rng), 0.0f, 1.0f);

        geos::internal::writeDials(body, perturbed);
        cv::UMat current = body.view(0);
        float loss = geos::internal::geodesicLoss(current, target);

        if (loss < hybrid_loss)
        {
            hybrid_loss = loss;
            hybrid_dials = perturbed;
        }

        if (iter % 10 == 0)
            std::cerr << "[HYBRID] Iter " << iter << ": loss=" << hybrid_loss << "\n";
    }

    auto hybrid_end = std::chrono::high_resolution_clock::now();
    auto hybrid_ms = std::chrono::duration_cast<std::chrono::milliseconds>(hybrid_end - hybrid_start).count();

    std::cerr << "\nHYBRID Result:\n";
    std::cerr << "  Final loss: " << (hybrid_loss * 100.0f) << "%\n";
    std::cerr << "  Time: " << hybrid_ms << "ms\n";

    // Summary
    std::cerr << "\n=== Summary ===\n";
    std::cerr << "JITO:   " << (jito_result.final_loss * 100.0f) << "% in " << jito_ms << "ms"
              << " (" << jito_result.iterations << " iters)\n";
    std::cerr << "HYBRID: " << (hybrid_loss * 100.0f) << "% in " << hybrid_ms << "ms (50 iters)\n";

    if (jito_result.final_loss < hybrid_loss)
        std::cerr << "JITO wins by " << ((hybrid_loss - jito_result.final_loss) * 100.0f) << " points\n";
    else
        std::cerr << "HYBRID wins by " << ((jito_result.final_loss - hybrid_loss) * 100.0f) << " points\n";

    return 0;
}
