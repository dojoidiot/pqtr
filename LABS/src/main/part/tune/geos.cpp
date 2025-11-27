// geos.cpp
// Block-wise SPSA optimizer for color/tone dials
//
// Strategy:
//   Phase 1: Optimize Block A (8 dials: exposure, temp, tint, contrast, highlights, shadows, black, white)
//   Phase 2: Optimize Block B (3 dials: vibrance, saturation, colourDensity)
//   Phase 3: Joint refinement of A+B (11 dials)
//
// Algorithm: Simultaneous Perturbation Stochastic Approximation
// See doc/geos.md for theory

#include "geos.hpp"
#include <random>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace tune::internal
{
    // ============================================================
    // Dial mapping: 35 dials ↔ theta vector [0,1]^35
    // ============================================================
    //
    // Index layout:
    //   [0]     exposure          \
    //   [1]     temperature        | Block A (8)
    //   [2]     tint               |
    //   [3]     contrast           |
    //   [4]     highlights         |
    //   [5]     shadows            |
    //   [6]     black              |
    //   [7]     white             /
    //   [8]     vibrance         \
    //   [9]     saturation        | Block B (3)
    //   [10]    colourDensity    /
    //   [11-34] SelectiveColour (not optimized)

    using Theta = std::array<float, GEOS_DIAL_COUNT>;

    // Read current dial values from link into theta
    void readDials(pipe::Body::Link& link, Theta& theta)
    {
        // ColorCorrection (3)
        theta[0] = link.colorCorrection().exposure().get();
        theta[1] = link.colorCorrection().whiteBalance().temperature();
        theta[2] = link.colorCorrection().whiteBalance().tint();

        // ToneMapping (5)
        theta[3] = link.toneMapping().contrast().get();
        theta[4] = link.toneMapping().curveAdjustment().highlights().get();
        theta[5] = link.toneMapping().curveAdjustment().shadows().get();
        theta[6] = link.toneMapping().clippingPoint().black().get();
        theta[7] = link.toneMapping().clippingPoint().white().get();

        // GlobalColor (3)
        theta[8] = link.globalColor().vibrance().get();
        theta[9] = link.globalColor().saturation().get();
        theta[10] = link.globalColor().colourDensity().get();

        // SelectiveColour (24) - read but not optimized
        auto readHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
            theta[base + 0] = hsl.hue();
            theta[base + 1] = hsl.saturation();
            theta[base + 2] = hsl.luminance();
        };

        readHSL(link.selectiveColour().red(), 11);
        readHSL(link.selectiveColour().orange(), 14);
        readHSL(link.selectiveColour().yellow(), 17);
        readHSL(link.selectiveColour().green(), 20);
        readHSL(link.selectiveColour().cyan(), 23);
        readHSL(link.selectiveColour().blue(), 26);
        readHSL(link.selectiveColour().purple(), 29);
        readHSL(link.selectiveColour().magenta(), 32);
    }

    // Write theta values to link dials
    void writeDials(pipe::Body::Link& link, const Theta& theta)
    {
        // ColorCorrection (3)
        link.colorCorrection().exposure().set(theta[0]);
        link.colorCorrection().whiteBalance().temperature(theta[1]);
        link.colorCorrection().whiteBalance().tint(theta[2]);

        // ToneMapping (5)
        link.toneMapping().contrast().set(theta[3]);
        link.toneMapping().curveAdjustment().highlights().set(theta[4]);
        link.toneMapping().curveAdjustment().shadows().set(theta[5]);
        link.toneMapping().clippingPoint().black().set(theta[6]);
        link.toneMapping().clippingPoint().white().set(theta[7]);

        // GlobalColor (3)
        link.globalColor().vibrance().set(theta[8]);
        link.globalColor().saturation().set(theta[9]);
        link.globalColor().colourDensity().set(theta[10]);

        // SelectiveColour (24) - write but not optimized
        auto writeHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
            hsl.hue(theta[base + 0]);
            hsl.saturation(theta[base + 1]);
            hsl.luminance(theta[base + 2]);
        };

        writeHSL(link.selectiveColour().red(), 11);
        writeHSL(link.selectiveColour().orange(), 14);
        writeHSL(link.selectiveColour().yellow(), 17);
        writeHSL(link.selectiveColour().green(), 20);
        writeHSL(link.selectiveColour().cyan(), 23);
        writeHSL(link.selectiveColour().blue(), 26);
        writeHSL(link.selectiveColour().purple(), 29);
        writeHSL(link.selectiveColour().magenta(), 32);
    }

    // Initialize theta to neutral (0.5 for all dials)
    void initNeutral(Theta& theta)
    {
        theta.fill(0.5f);
    }

    // Clip theta to [0, 1]
    void clipTheta(Theta& theta, int start, int size)
    {
        for (int i = start; i < start + size; i++)
        {
            theta[i] = std::max(0.0f, std::min(1.0f, theta[i]));
        }
    }

    // Compute loss for current body state
    float evaluateLoss(
        pipe::Body& body,
        const StyleFeatures& targetStyle)
    {
        View candidate = body.view();
        cv::UMat candProxy = resizeProxy(candidate);
        cv::UMat candLCH = convertToSafeLCH(candProxy);
        StyleFeatures candStyle = extractStyle(candLCH);
        return geodesicLoss(targetStyle, candStyle);
    }

    // SPSA gain schedules
    float computeA(int k, const PhaseParams& p)
    {
        return p.a0 / std::pow(static_cast<float>(k + 1) + p.A, p.alpha);
    }

    float computeC(int k, const PhaseParams& p)
    {
        return p.c0 / std::pow(static_cast<float>(k + 1), p.gamma);
    }

    // ============================================================
    // Block SPSA: optimize a contiguous block of dials
    // ============================================================
    // Returns: final loss after this block's optimization
    float optimizeBlock(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        Theta& theta,           // Full theta, modified in place
        int blockStart,
        int blockSize,
        const PhaseParams& params,
        int maxIter,
        int& iterCount,         // Running iteration count for progress
        int totalMaxIter,       // Total iterations for progress display
        Progress::Phase phase,
        float targetLaplacianVar,
        Callback progress,
        std::mt19937& rng)
    {
        // Best tracking for this block
        Theta bestTheta = theta;
        float bestLoss = evaluateLoss(body, targetStyle);

        std::cerr << "[BLOCK] Start phase=" << static_cast<int>(phase)
                  << " block[" << blockStart << ".." << blockStart + blockSize - 1 << "]"
                  << " startLoss=" << bestLoss << std::endl;

        std::bernoulli_distribution coin(0.5);

        for (int k = 0; k < maxIter; k++)
        {
            // Compute gain coefficients
            float a_k = computeA(k, params);
            float c_k = computeC(k, params);

            // Generate perturbation for this block only
            std::array<float, BLOCK_AB_SIZE> delta;
            for (int i = 0; i < blockSize; i++)
            {
                delta[i] = coin(rng) ? 1.0f : -1.0f;
            }

            // Evaluate L+ (theta + c_k * delta)
            Theta thetaPlus = theta;
            for (int i = 0; i < blockSize; i++)
            {
                thetaPlus[blockStart + i] = theta[blockStart + i] + c_k * delta[i];
            }
            clipTheta(thetaPlus, blockStart, blockSize);
            writeDials(link, thetaPlus);
            float lossPlus = evaluateLoss(body, targetStyle);

            // Evaluate L- (theta - c_k * delta)
            Theta thetaMinus = theta;
            for (int i = 0; i < blockSize; i++)
            {
                thetaMinus[blockStart + i] = theta[blockStart + i] - c_k * delta[i];
            }
            clipTheta(thetaMinus, blockStart, blockSize);
            writeDials(link, thetaMinus);
            float lossMinus = evaluateLoss(body, targetStyle);

            // Estimate gradient and update (only for this block)
            for (int i = 0; i < blockSize; i++)
            {
                float g_i = (lossPlus - lossMinus) / (2.0f * c_k * delta[i]);
                theta[blockStart + i] = theta[blockStart + i] - a_k * g_i;
            }
            clipTheta(theta, blockStart, blockSize);
            writeDials(link, theta);

            // Evaluate new position
            float newLoss = evaluateLoss(body, targetStyle);

            // Track best
            if (newLoss < bestLoss)
            {
                bestLoss = newLoss;
                bestTheta = theta;
                // DEBUG: print what we're recording
                std::cerr << "[BEST] k=" << k << " newLoss=" << newLoss
                          << " theta[0..3]=" << theta[blockStart]
                          << "," << theta[blockStart+1]
                          << "," << theta[blockStart+2]
                          << "," << (blockSize > 3 ? theta[blockStart+3] : 0.0f) << std::endl;
            }
            else if (newLoss > bestLoss * 2.0f)
            {
                // Revert if we've diverged badly
                theta = bestTheta;
                writeDials(link, theta);
            }

            iterCount++;

            // Progress callback
            if (progress)
            {
                cv::UMat candProxy = resizeProxy(body.view());
                cv::UMat candLCH = convertToSafeLCH(candProxy);
                StyleFeatures candStyle = extractStyle(candLCH);
                auto [r, th] = computeDome(targetStyle, candStyle);

                float candVar = laplacianVariance(candProxy);
                float freqLoss = (targetLaplacianVar > 1e-6f)
                    ? std::abs(candVar - targetLaplacianVar) / targetLaplacianVar
                    : 0.0f;

                Progress p;
                p.stage = Progress::Stage::GEOS;
                p.phase = phase;
                p.iteration = iterCount;
                p.max_iterations = totalMaxIter;
                p.loss.spectral = newLoss;
                p.loss.frequency = freqLoss;
                p.dome.r = r;
                p.dome.theta = th;
                p.edge.ratio = 1.0f;

                if (!progress(p))
                {
                    // User cancelled - restore best and return
                    theta = bestTheta;
                    writeDials(link, theta);
                    return bestLoss;
                }
            }

            // Early termination
            if (bestLoss < CONVERGE_THRESHOLD)
            {
                std::cerr << "[BLOCK] Early converge at iter=" << k
                          << " loss=" << bestLoss << std::endl;
                break;
            }
        }

        // Restore best found for this block
        std::cerr << "[RESTORE] bestTheta[0..3]=" << bestTheta[blockStart]
                  << "," << bestTheta[blockStart+1]
                  << "," << bestTheta[blockStart+2]
                  << "," << (blockSize > 3 ? bestTheta[blockStart+3] : 0.0f) << std::endl;
        theta = bestTheta;
        writeDials(link, theta);

        // Verify what was actually written
        Theta verify;
        readDials(link, verify);
        std::cerr << "[VERIFY] readBack[0..3]=" << verify[blockStart]
                  << "," << verify[blockStart+1]
                  << "," << verify[blockStart+2]
                  << "," << (blockSize > 3 ? verify[blockStart+3] : 0.0f) << std::endl;

        float finalLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[BLOCK] End bestLoss=" << bestLoss
                  << " verifyLoss=" << finalLoss << std::endl;

        return finalLoss;
    }

    // ============================================================
    // Main block-wise optimization
    // ============================================================
    int optimizeGeos(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress)
    {
        std::random_device rd;
        std::mt19937 rng(rd());

        // Initialize all dials to neutral
        Theta theta;
        initNeutral(theta);
        writeDials(link, theta);

        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[GEOS] Initial loss (all neutral): " << initialLoss << std::endl;

        // Sanity check: evaluate twice - should be identical
        float check1 = evaluateLoss(body, targetStyle);
        float check2 = evaluateLoss(body, targetStyle);
        std::cerr << "[SANITY] Same dials, two evals: " << check1 << " vs " << check2 << std::endl;

        // Check base image integrity
        View base1 = body.view();
        cv::Scalar sum1 = cv::sum(base1);
        View base2 = body.view();
        cv::Scalar sum2 = cv::sum(base2);
        std::cerr << "[SANITY] After-link image sum: " << sum1[0] << " vs " << sum2[0] << std::endl;

        // Check raw data before links
        cv::Scalar rawSum1 = cv::sum(body.data().view());
        cv::Scalar rawSum2 = cv::sum(body.data().view());
        std::cerr << "[SANITY] Pre-link raw sum: " << rawSum1[0] << " vs " << rawSum2[0] << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Phase 1: Block A (ColorCorrection + ToneMapping = 8 dials)
        int phase1Iter = static_cast<int>(totalIter * PHASE1_RATIO);
        std::cerr << "\n[GEOS] === Phase 1: Block A (8 dials) ===" << std::endl;
        float lossAfterA = optimizeBlock(
            body, link, targetStyle, theta,
            BLOCK_A_START, BLOCK_A_SIZE,
            BLOCK_8D, phase1Iter,
            iterCount, totalIter,
            Progress::Phase::HUGE,
            targetLaplacianVar, progress, rng);

        std::cerr << "[GEOS] After Phase 1: loss=" << lossAfterA << std::endl;

        if (lossAfterA < CONVERGE_THRESHOLD)
        {
            std::cerr << "[GEOS] Converged after Phase 1" << std::endl;
            return iterCount;
        }

        // Phase 2: Block B (GlobalColor = 3 dials)
        int phase2Iter = static_cast<int>(totalIter * PHASE2_RATIO);
        std::cerr << "\n[GEOS] === Phase 2: Block B (3 dials) ===" << std::endl;
        float lossAfterB = optimizeBlock(
            body, link, targetStyle, theta,
            BLOCK_B_START, BLOCK_B_SIZE,
            BLOCK_3D, phase2Iter,
            iterCount, totalIter,
            Progress::Phase::MIDS,
            targetLaplacianVar, progress, rng);

        std::cerr << "[GEOS] After Phase 2: loss=" << lossAfterB << std::endl;

        if (lossAfterB < CONVERGE_THRESHOLD)
        {
            std::cerr << "[GEOS] Converged after Phase 2" << std::endl;
            return iterCount;
        }

        // Phase 3: Joint A+B (11 dials) - final polish
        int phase3Iter = static_cast<int>(totalIter * PHASE3_RATIO);
        std::cerr << "\n[GEOS] === Phase 3: Joint A+B (11 dials) ===" << std::endl;
        float finalLoss = optimizeBlock(
            body, link, targetStyle, theta,
            0, BLOCK_AB_SIZE,  // Start at 0, size 11
            BLOCK_11D, phase3Iter,
            iterCount, totalIter,
            Progress::Phase::TINY,
            targetLaplacianVar, progress, rng);

        std::cerr << "\n[GEOS] === FINAL ===" << std::endl;
        std::cerr << "[GEOS] Final loss: " << finalLoss << std::endl;
        std::cerr << "[GEOS] Theta[0..10]: ";
        for (int i = 0; i < BLOCK_AB_SIZE; i++)
        {
            std::cerr << theta[i] << " ";
        }
        std::cerr << std::endl;

        return iterCount;
    }

} // namespace tune::internal
