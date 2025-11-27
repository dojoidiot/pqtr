// geos.cpp
// SPSA optimizer for color/tone dials
//
// Two modes:
//   BLOCKWISE: 4-phase stepwise optimization
//   FULL_37D:  All 37 dials simultaneously
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
    // Dial mapping: 37 dials <-> theta vector [0,1]^37
    // ============================================================
    //
    // Index layout:
    //   [0]     exposure           |
    //   [1]     temperature        | Block A (10)
    //   [2]     tint               | ColorCorrection (3) +
    //   [3]     contrast           | ToneMapping (7)
    //   [4]     highlights         |
    //   [5]     shadows            |
    //   [6]     toe_pivot          |
    //   [7]     shoulder_pivot     |
    //   [8]     black              |
    //   [9]     white              |
    //   [10]    vibrance           |
    //   [11]    saturation         | Block B (3)
    //   [12]    colourDensity      |
    //   [13-15] red H/S/L          |
    //   [16-18] orange H/S/L       |
    //   [19-21] yellow H/S/L       | Block C (24)
    //   [22-24] green H/S/L        | SelectiveColour
    //   [25-27] cyan H/S/L         |
    //   [28-30] blue H/S/L         |
    //   [31-33] purple H/S/L       |
    //   [34-36] magenta H/S/L      |

    using Theta = std::array<float, GEOS_DIAL_COUNT>;

    // Read current dial values from link into theta
    void readDials(pipe::Body::Link& link, Theta& theta)
    {
        // ColorCorrection (3)
        theta[0] = link.colorCorrection().exposure().get();
        theta[1] = link.colorCorrection().whiteBalance().temperature();
        theta[2] = link.colorCorrection().whiteBalance().tint();

        // ToneMapping (7)
        theta[3] = link.toneMapping().contrast().get();
        theta[4] = link.toneMapping().curveAdjustment().highlights().get();
        theta[5] = link.toneMapping().curveAdjustment().shadows().get();
        theta[6] = link.toneMapping().curveAdjustment().toePivot().get();
        theta[7] = link.toneMapping().curveAdjustment().shoulderPivot().get();
        theta[8] = link.toneMapping().clippingPoint().black().get();
        theta[9] = link.toneMapping().clippingPoint().white().get();

        // GlobalColor (3)
        theta[10] = link.globalColor().vibrance().get();
        theta[11] = link.globalColor().saturation().get();
        theta[12] = link.globalColor().colourDensity().get();

        // SelectiveColour (24)
        auto readHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
            theta[base + 0] = hsl.hue();
            theta[base + 1] = hsl.saturation();
            theta[base + 2] = hsl.luminance();
        };

        readHSL(link.selectiveColour().red(), 13);
        readHSL(link.selectiveColour().orange(), 16);
        readHSL(link.selectiveColour().yellow(), 19);
        readHSL(link.selectiveColour().green(), 22);
        readHSL(link.selectiveColour().cyan(), 25);
        readHSL(link.selectiveColour().blue(), 28);
        readHSL(link.selectiveColour().purple(), 31);
        readHSL(link.selectiveColour().magenta(), 34);
    }

    // Write theta values to link dials
    void writeDials(pipe::Body::Link& link, const Theta& theta)
    {
        // ColorCorrection (3)
        link.colorCorrection().exposure().set(theta[0]);
        link.colorCorrection().whiteBalance().temperature(theta[1]);
        link.colorCorrection().whiteBalance().tint(theta[2]);

        // ToneMapping (7)
        link.toneMapping().contrast().set(theta[3]);
        link.toneMapping().curveAdjustment().highlights().set(theta[4]);
        link.toneMapping().curveAdjustment().shadows().set(theta[5]);
        link.toneMapping().curveAdjustment().toePivot().set(theta[6]);
        link.toneMapping().curveAdjustment().shoulderPivot().set(theta[7]);
        link.toneMapping().clippingPoint().black().set(theta[8]);
        link.toneMapping().clippingPoint().white().set(theta[9]);

        // GlobalColor (3)
        link.globalColor().vibrance().set(theta[10]);
        link.globalColor().saturation().set(theta[11]);
        link.globalColor().colourDensity().set(theta[12]);

        // SelectiveColour (24)
        auto writeHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
            hsl.hue(theta[base + 0]);
            hsl.saturation(theta[base + 1]);
            hsl.luminance(theta[base + 2]);
        };

        writeHSL(link.selectiveColour().red(), 13);
        writeHSL(link.selectiveColour().orange(), 16);
        writeHSL(link.selectiveColour().yellow(), 19);
        writeHSL(link.selectiveColour().green(), 22);
        writeHSL(link.selectiveColour().cyan(), 25);
        writeHSL(link.selectiveColour().blue(), 28);
        writeHSL(link.selectiveColour().purple(), 31);
        writeHSL(link.selectiveColour().magenta(), 34);
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
    float optimizeBlock(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        Theta& theta,
        int blockStart,
        int blockSize,
        const PhaseParams& params,
        int maxIter,
        int& iterCount,
        int totalMaxIter,
        Progress::Phase phase,
        float targetLaplacianVar,
        Callback progress,
        std::mt19937& rng)
    {
        Theta bestTheta = theta;
        float bestLoss = evaluateLoss(body, targetStyle);

        std::cerr << "[BLOCK] Start phase=" << static_cast<int>(phase)
                  << " block[" << blockStart << ".." << blockStart + blockSize - 1 << "]"
                  << " startLoss=" << bestLoss << std::endl;

        std::bernoulli_distribution coin(0.5);

        // Dynamic delta array (supports up to 35 dims)
        std::array<float, GEOS_DIAL_COUNT> delta;

        for (int k = 0; k < maxIter; k++)
        {
            float a_k = computeA(k, params);
            float c_k = computeC(k, params);

            // Generate perturbation for this block
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

            // Estimate gradient and update
            for (int i = 0; i < blockSize; i++)
            {
                float g_i = (lossPlus - lossMinus) / (2.0f * c_k * delta[i]);
                theta[blockStart + i] = theta[blockStart + i] - a_k * g_i;
            }
            clipTheta(theta, blockStart, blockSize);
            writeDials(link, theta);

            float newLoss = evaluateLoss(body, targetStyle);

            // Track best
            if (newLoss < bestLoss)
            {
                bestLoss = newLoss;
                bestTheta = theta;
                if (k % 20 == 0 || newLoss < 0.02f)
                {
                    std::cerr << "[BEST] k=" << k << " loss=" << newLoss << std::endl;
                }
            }
            else if (newLoss > bestLoss * 2.0f)
            {
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
                    theta = bestTheta;
                    writeDials(link, theta);
                    return bestLoss;
                }
            }

            // Early termination disabled for testing all phases
            // if (bestLoss < CONVERGE_THRESHOLD)
            // {
            //     std::cerr << "[BLOCK] Early converge at iter=" << k
            //               << " loss=" << bestLoss << std::endl;
            //     break;
            // }
        }

        // Restore best
        theta = bestTheta;
        writeDials(link, theta);

        float finalLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[BLOCK] End bestLoss=" << bestLoss
                  << " verifyLoss=" << finalLoss << std::endl;

        return finalLoss;
    }

    // ============================================================
    // BLOCKWISE mode: 4-phase optimization
    // ============================================================
    int optimizeBlockwise(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress)
    {
        std::random_device rd;
        std::mt19937 rng(rd());

        Theta theta;
        initNeutral(theta);
        writeDials(link, theta);

        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[GEOS-BLOCKWISE] Initial loss: " << initialLoss << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Phase 1: Block A (10 dials)
        int phase1Iter = static_cast<int>(totalIter * PHASE1_RATIO);
        std::cerr << "\n[GEOS] === Phase 1: Block A (10 dials) ===" << std::endl;
        float lossAfterA = optimizeBlock(
            body, link, targetStyle, theta,
            BLOCK_A_START, BLOCK_A_SIZE,
            BLOCK_10D, phase1Iter,
            iterCount, totalIter,
            Progress::Phase::HUGE,
            targetLaplacianVar, progress, rng);

        // Phase 2: Block B (3 dials)
        int phase2Iter = static_cast<int>(totalIter * PHASE2_RATIO);
        std::cerr << "\n[GEOS] === Phase 2: Block B (3 dials) ===" << std::endl;
        float lossAfterB = optimizeBlock(
            body, link, targetStyle, theta,
            BLOCK_B_START, BLOCK_B_SIZE,
            BLOCK_3D, phase2Iter,
            iterCount, totalIter,
            Progress::Phase::MIDS,
            targetLaplacianVar, progress, rng);

        // Phase 3: Joint A+B (13 dials)
        int phase3Iter = static_cast<int>(totalIter * PHASE3_RATIO);
        std::cerr << "\n[GEOS] === Phase 3: Joint A+B (13 dials) ===" << std::endl;
        float lossAfterAB = optimizeBlock(
            body, link, targetStyle, theta,
            0, BLOCK_AB_SIZE,
            BLOCK_13D, phase3Iter,
            iterCount, totalIter,
            Progress::Phase::TINY,
            targetLaplacianVar, progress, rng);

        // Always run Phase 4 for selective color polish
        // (even if converged - per-hue adjustments may help saturation)

        // Phase 4: Block C - SelectiveColour (24 dials)
        int phase4Iter = static_cast<int>(totalIter * PHASE4_RATIO);
        std::cerr << "\n[GEOS] === Phase 4: Block C - SelectiveColour (24 dials) ===" << std::endl;
        float finalLoss = optimizeBlock(
            body, link, targetStyle, theta,
            BLOCK_C_START, BLOCK_C_SIZE,
            BLOCK_24D, phase4Iter,
            iterCount, totalIter,
            Progress::Phase::TINY,  // Fine-tuning phase
            targetLaplacianVar, progress, rng);

        std::cerr << "\n[GEOS-BLOCKWISE] === FINAL ===" << std::endl;
        std::cerr << "[GEOS] Final loss: " << finalLoss << std::endl;
        std::cerr << "[GEOS] Theta[0..12]: ";
        for (int i = 0; i < BLOCK_AB_SIZE; i++)
            std::cerr << theta[i] << " ";
        std::cerr << std::endl;

        return iterCount;
    }

    // ============================================================
    // FULL_37D mode: all dials simultaneously
    // ============================================================
    int optimizeFull37D(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress)
    {
        std::random_device rd;
        std::mt19937 rng(rd());

        Theta theta;
        initNeutral(theta);
        writeDials(link, theta);

        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[GEOS-FULL37D] Initial loss: " << initialLoss << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Single phase: all 37 dials
        std::cerr << "\n[GEOS] === Full 37D optimization ===" << std::endl;
        float finalLoss = optimizeBlock(
            body, link, targetStyle, theta,
            0, GEOS_DIAL_COUNT,  // All 37 dials
            BLOCK_37D, totalIter,
            iterCount, totalIter,
            Progress::Phase::HUGE,  // Single long phase
            targetLaplacianVar, progress, rng);

        std::cerr << "\n[GEOS-FULL37D] === FINAL ===" << std::endl;
        std::cerr << "[GEOS] Final loss: " << finalLoss << std::endl;
        std::cerr << "[GEOS] Theta[0..12]: ";
        for (int i = 0; i < BLOCK_AB_SIZE; i++)
            std::cerr << theta[i] << " ";
        std::cerr << std::endl;
        std::cerr << "[GEOS] Theta[13..36] (selective): ";
        for (int i = BLOCK_C_START; i < GEOS_DIAL_COUNT; i++)
            std::cerr << theta[i] << " ";
        std::cerr << std::endl;

        return iterCount;
    }

    // ============================================================
    // Entry point: dispatch based on config.geos_mode
    // ============================================================
    int optimizeGeos(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress)
    {
        // Sanity checks
        float check1 = evaluateLoss(body, targetStyle);
        float check2 = evaluateLoss(body, targetStyle);
        std::cerr << "[SANITY] Same dials, two evals: " << check1 << " vs " << check2 << std::endl;

        if (std::abs(check1 - check2) > 0.001f)
        {
            std::cerr << "[WARNING] Non-deterministic loss evaluation detected!" << std::endl;
        }

        // Dispatch based on mode
        if (config.geos_mode == GeosMode::FULL_35D)
        {
            return optimizeFull37D(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
        else
        {
            return optimizeBlockwise(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
    }

} // namespace tune::internal
