// test_jito.cpp
// Quick test of JITO optimizer

#include <pipe.hpp>
#include <sink.hpp>
#include <tool.hpp>
#include <hold.hpp>
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
        std::cerr << "Usage: " << argv[0] << " <image.ARW> <target.jpg>\n";
        return 1;
    }

    std::string raw_path = argv[1];
    std::string target_path = argv[2];

    // Load RAW
    std::cerr << "Loading RAW: " << raw_path << "\n";
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

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
    std::cerr << "Initializing JITO...\n";
    if (!geos::jitoInit("etc/jacob.json"))
    {
        std::cerr << "Failed to load Jacobian\n";
        return 1;
    }

    // Create body at working size
    auto& body = head->body(1024);

    // Add Base link
    auto& link = body.add("Base");

    // Get initial dials (neutral)
    geos::internal::Theta initial;
    initial.fill(0.5f);

    // Run JITO
    std::cerr << "\n=== JITO Optimization ===\n";
    auto start = std::chrono::high_resolution_clock::now();

    // Try smaller learning rate - Jacobian only valid locally
    auto result = geos::jitoOptimize(body, link, target, initial, 50, 0.1f, 0.02f);

    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cerr << "\nResult:\n";
    std::cerr << "  Iterations: " << result.iterations << "\n";
    std::cerr << "  Final loss: " << (result.final_loss * 100.0f) << "%\n";
    std::cerr << "  Time: " << ms << "ms\n";

    // Show key dial values
    std::cerr << "\nKey dials (0.5 = neutral):\n";
    std::cerr << "  exposure:    " << result.dials[0] << "\n";
    std::cerr << "  temperature: " << result.dials[1] << "\n";
    std::cerr << "  contrast:    " << result.dials[3] << "\n";
    std::cerr << "  shadows:     " << result.dials[5] << "\n";
    std::cerr << "  vibrance:    " << result.dials[10] << "\n";
    std::cerr << "  saturation:  " << result.dials[11] << "\n";

    // Save output
    body.tail().save("tmp/jito_test.png", 2048);
    std::cerr << "\nSaved: tmp/jito_test.png\n";

    return 0;
}
