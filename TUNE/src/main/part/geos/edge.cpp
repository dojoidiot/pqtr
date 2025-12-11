// edge.cpp
// Golden section optimizer for detail dials (4 parameters)
//
// Optimizes: sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma
// Algorithm: Golden section search per dial, targeting Laplacian variance match
// See doc/edge.md for theory

#include "edge.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>

namespace geos::internal
{
    // Golden ratio constants
    constexpr float PHI = 1.618033988749895f;
    constexpr float INV_PHI = 0.618033988749895f;  // 1/φ = φ-1
    constexpr float TOLERANCE = 0.02f;  // Stop when bracket < 2%

    // Dial names for logging
    static const char* DIAL_NAMES[] = {
        "sharpen_amount",
        "sharpen_radius",
        "denoise_luma",
        "denoise_chroma"
    };

    // Get current dial value from link
    static float getDial(pipe::Body::Link& link, int dial)
    {
        auto& detail = link.detail();
        switch (dial)
        {
            case 0: return detail.sharpen().amount();
            case 1: return detail.sharpen().radius();
            case 2: return detail.denoise().luminance().get();
            case 3: return detail.denoise().chroma().get();
            default: return 0.5f;
        }
    }

    // Set dial value on link
    static void setDial(pipe::Body::Link& link, int dial, float value)
    {
        auto& detail = link.detail();
        switch (dial)
        {
            case 0: detail.sharpen().amount(value); break;
            case 1: detail.sharpen().radius(value); break;
            case 2: detail.denoise().luminance().set(value); break;
            case 3: detail.denoise().chroma().set(value); break;
        }
    }

    // Evaluate frequency loss (ratio error) for current settings
    static float evaluateLoss(
        pipe::Body& body,
        float targetLaplacianVar,
        int& evalCount)
    {
        View candidate = body.view();
        cv::UMat candProxy = resizeProxy(candidate);
        float candVar = laplacianVariance(candProxy);
        evalCount++;

        if (targetLaplacianVar < 1e-6f)
            return (candVar < 1e-6f) ? 0.0f : 1.0f;

        // Return absolute relative error
        return std::abs(candVar - targetLaplacianVar) / targetLaplacianVar;
    }

    // Golden section search for a single dial
    // Returns optimal value and updates evalCount
    static float goldenSearch(
        pipe::Body& body,
        pipe::Body::Link& link,
        int dialIndex,
        float targetLaplacianVar,
        int& evalCount,
        Callback progress,
        int& iteration,
        int maxIterations)
    {
        float a = 0.0f;
        float b = 1.0f;

        // Compute interior points
        float c = b - (b - a) * INV_PHI;
        float d = a + (b - a) * INV_PHI;

        // Evaluate at c
        setDial(link, dialIndex, c);
        float fc = evaluateLoss(body, targetLaplacianVar, evalCount);

        // Evaluate at d
        setDial(link, dialIndex, d);
        float fd = evaluateLoss(body, targetLaplacianVar, evalCount);

        std::cerr << "[EDGE] " << DIAL_NAMES[dialIndex]
                  << " init: a=" << std::fixed << std::setprecision(3) << a
                  << " b=" << b << " fc=" << fc << " fd=" << fd << std::endl;

        while ((b - a) > TOLERANCE)
        {
            iteration++;

            if (fc < fd)
            {
                // Minimum is in [a, d]
                b = d;
                d = c;
                fd = fc;
                c = b - (b - a) * INV_PHI;
                setDial(link, dialIndex, c);
                fc = evaluateLoss(body, targetLaplacianVar, evalCount);
            }
            else
            {
                // Minimum is in [c, b]
                a = c;
                c = d;
                fc = fd;
                d = a + (b - a) * INV_PHI;
                setDial(link, dialIndex, d);
                fd = evaluateLoss(body, targetLaplacianVar, evalCount);
            }

            // Progress callback
            if (progress)
            {
                View candidate = body.view();
                cv::UMat candProxy = resizeProxy(candidate);
                float candVar = laplacianVariance(candProxy);
                float ratio = (targetLaplacianVar > 1e-6f)
                    ? candVar / targetLaplacianVar
                    : 1.0f;

                Progress p;
                p.stage = Progress::Stage::EDGE;
                p.iteration = iteration;
                p.max_iterations = maxIterations;
                p.loss.spectral = 0.0f;
                p.loss.frequency = std::min(fc, fd);
                p.dome.r = 0.0f;
                p.dome.theta = 0.0f;
                p.edge.ratio = ratio;

                if (!progress(p))
                {
                    // Cancelled - return current best
                    float best = (a + b) / 2.0f;
                    setDial(link, dialIndex, best);
                    return best;
                }
            }
        }

        // Return midpoint of final bracket
        float optimal = (a + b) / 2.0f;
        setDial(link, dialIndex, optimal);

        std::cerr << "[EDGE] " << DIAL_NAMES[dialIndex]
                  << " result: " << std::fixed << std::setprecision(3) << optimal
                  << " (loss=" << std::min(fc, fd) << ")" << std::endl;

        return optimal;
    }

    int optimizeEdge(
        pipe::Body& body,
        pipe::Body::Link& link,
        float targetLaplacianVar,
        const Config& config,
        Callback progress)
    {
        int evaluations = 0;
        int iteration = 0;
        int maxIterations = 60;  // ~15 evals × 4 dials

        std::cerr << "\n[EDGE] === Detail optimization ===" << std::endl;
        std::cerr << "[EDGE] Target Laplacian variance: " << targetLaplacianVar << std::endl;

        // Report initial state
        if (progress)
        {
            View candidate = body.view();
            cv::UMat candProxy = resizeProxy(candidate);
            float candVar = laplacianVariance(candProxy);
            float ratio = (targetLaplacianVar > 1e-6f)
                ? candVar / targetLaplacianVar
                : 1.0f;

            Progress p;
            p.stage = Progress::Stage::EDGE;
            p.iteration = 0;
            p.max_iterations = maxIterations;
            p.loss.spectral = 0.0f;
            p.loss.frequency = std::abs(ratio - 1.0f);
            p.dome.r = 0.0f;
            p.dome.theta = 0.0f;
            p.edge.ratio = ratio;

            std::cerr << "[EDGE] Initial ratio: " << std::fixed << std::setprecision(3)
                      << ratio << " (loss=" << p.loss.frequency << ")" << std::endl;

            if (!progress(p))
            {
                return evaluations;
            }
        }

        // Initialize all dials to default (0.5 for sharpen, 0 for denoise)
        setDial(link, 0, 0.3f);  // sharpen_amount: moderate
        setDial(link, 1, 0.4f);  // sharpen_radius: ~1.5px
        setDial(link, 2, 0.0f);  // denoise_luma: none
        setDial(link, 3, 0.0f);  // denoise_chroma: none

        // Optimize sharpen_amount first (most impact on sharpness)
        goldenSearch(body, link, 0, targetLaplacianVar, evaluations,
                     progress, iteration, maxIterations);

        // Optimize sharpen_radius
        goldenSearch(body, link, 1, targetLaplacianVar, evaluations,
                     progress, iteration, maxIterations);

        // Skip denoise for now - we're matching sharpness, not reducing noise
        // The target image from camera already has its processing applied
        // Adding denoise would make our output softer than target

        // Final evaluation
        View candidate = body.view();
        cv::UMat candProxy = resizeProxy(candidate);
        float candVar = laplacianVariance(candProxy);
        float finalRatio = (targetLaplacianVar > 1e-6f)
            ? candVar / targetLaplacianVar
            : 1.0f;
        float finalLoss = std::abs(finalRatio - 1.0f);

        std::cerr << "[EDGE] Final ratio: " << std::fixed << std::setprecision(3)
                  << finalRatio << " (loss=" << finalLoss << ")" << std::endl;
        std::cerr << "[EDGE] Sharpen: amount=" << getDial(link, 0)
                  << " radius=" << getDial(link, 1) << std::endl;

        return evaluations;
    }

} // namespace geos::internal
