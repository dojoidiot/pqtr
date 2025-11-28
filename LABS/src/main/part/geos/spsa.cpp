// spsa.cpp
// SPSA optimizer for color/tone dials
//
// Two modes:
//   BLOCKWISE: 4-phase stepwise optimization
//   FULL_37D:  All 37 dials simultaneously
//
// Algorithm: Simultaneous Perturbation Stochastic Approximation
// See doc/geos.md for theory

#include "spsa.hpp"
#include <random>
#include <array>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace geos::internal
{
    // ============================================================
    // Dial mapping: 41 dials <-> theta vector [0,1]^41
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
    //   [11]    saturation         | Block B (7)
    //   [12]    colourDensity      | GlobalColor (3) +
    //   [13]    shadow_temp        | SplitTone (4)
    //   [14]    shadow_tint        |
    //   [15]    highlight_temp     |
    //   [16]    highlight_tint     |
    //   [17-19] red H/S/L          |
    //   [20-22] orange H/S/L       |
    //   [23-25] yellow H/S/L       | Block C (24)
    //   [26-28] green H/S/L        | SelectiveColour
    //   [29-31] cyan H/S/L         |
    //   [32-34] blue H/S/L         |
    //   [35-37] purple H/S/L       |
    //   [38-40] magenta H/S/L      |

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

        // SplitTone (4)
        theta[13] = link.splitTone().shadows().temperature();
        theta[14] = link.splitTone().shadows().tint();
        theta[15] = link.splitTone().highlights().temperature();
        theta[16] = link.splitTone().highlights().tint();

        // SelectiveColour (24)
        auto readHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
            theta[base + 0] = hsl.hue();
            theta[base + 1] = hsl.saturation();
            theta[base + 2] = hsl.luminance();
        };

        readHSL(link.selectiveColour().red(), 17);
        readHSL(link.selectiveColour().orange(), 20);
        readHSL(link.selectiveColour().yellow(), 23);
        readHSL(link.selectiveColour().green(), 26);
        readHSL(link.selectiveColour().cyan(), 29);
        readHSL(link.selectiveColour().blue(), 32);
        readHSL(link.selectiveColour().purple(), 35);
        readHSL(link.selectiveColour().magenta(), 38);
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

        // SplitTone (4)
        link.splitTone().shadows().temperature(theta[13]);
        link.splitTone().shadows().tint(theta[14]);
        link.splitTone().highlights().temperature(theta[15]);
        link.splitTone().highlights().tint(theta[16]);

        // SelectiveColour (24)
        auto writeHSL = [&](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, int base) {
            hsl.hue(theta[base + 0]);
            hsl.saturation(theta[base + 1]);
            hsl.luminance(theta[base + 2]);
        };

        writeHSL(link.selectiveColour().red(), 17);
        writeHSL(link.selectiveColour().orange(), 20);
        writeHSL(link.selectiveColour().yellow(), 23);
        writeHSL(link.selectiveColour().green(), 26);
        writeHSL(link.selectiveColour().cyan(), 29);
        writeHSL(link.selectiveColour().blue(), 32);
        writeHSL(link.selectiveColour().purple(), 35);
        writeHSL(link.selectiveColour().magenta(), 38);
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

        // Early termination tracking
        int itersSinceImprovement = 0;

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
                itersSinceImprovement = 0;
                if (k % 20 == 0 || newLoss < 0.02f)
                {
                    std::cerr << "[BEST] k=" << k << " loss=" << newLoss << std::endl;
                }
            }
            else
            {
                itersSinceImprovement++;
                if (newLoss > bestLoss * 2.0f)
                {
                    theta = bestTheta;
                    writeDials(link, theta);
                }
            }

            // Early termination if stalled
            if (itersSinceImprovement >= STALL_THRESHOLD)
            {
                std::cerr << "[BLOCK] Early stop: no improvement for "
                          << STALL_THRESHOLD << " iterations" << std::endl;
                break;
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
    // BLOCKWISE mode: 4-phase optimization (3-phase when LUT active)
    // ============================================================
    int optimizeBlockwise(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress,
        bool lutEstimated)
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

        // Phase 2: Block B (7 dials: GlobalColor + SplitTone)
        int phase2Iter = static_cast<int>(totalIter * PHASE2_RATIO);
        std::cerr << "\n[GEOS] === Phase 2: Block B (7 dials: GlobalColor + SplitTone) ===" << std::endl;
        float lossAfterB = optimizeBlock(
            body, link, targetStyle, theta,
            BLOCK_B_START, BLOCK_B_SIZE,
            BLOCK_7D, phase2Iter,
            iterCount, totalIter,
            Progress::Phase::MIDS,
            targetLaplacianVar, progress, rng);

        // Phase 3: Joint A+B (17 dials)
        int phase3Iter = static_cast<int>(totalIter * PHASE3_RATIO);
        std::cerr << "\n[GEOS] === Phase 3: Joint A+B (17 dials) ===" << std::endl;
        float lossAfterAB = optimizeBlock(
            body, link, targetStyle, theta,
            0, BLOCK_AB_SIZE,
            BLOCK_17D, phase3Iter,
            iterCount, totalIter,
            Progress::Phase::TINY,
            targetLaplacianVar, progress, rng);

        // Phase 4: Block C - SelectiveColour (24 dials)
        // Skip when LUT is active - 3D LUT already captures hue-dependent transforms
        float finalLoss = lossAfterAB;
        if (!lutEstimated)
        {
            int phase4Iter = static_cast<int>(totalIter * PHASE4_RATIO);
            std::cerr << "\n[GEOS] === Phase 4: Block C - SelectiveColour (24 dials) ===" << std::endl;
            finalLoss = optimizeBlock(
                body, link, targetStyle, theta,
                BLOCK_C_START, BLOCK_C_SIZE,
                BLOCK_24D, phase4Iter,
                iterCount, totalIter,
                Progress::Phase::TINY,  // Fine-tuning phase
                targetLaplacianVar, progress, rng);
        }
        else
        {
            std::cerr << "\n[GEOS] === Phase 4: SKIPPED (LUT captures hue transforms) ===" << std::endl;
        }

        std::cerr << "\n[GEOS-BLOCKWISE] === FINAL ===" << std::endl;
        std::cerr << "[GEOS] Final loss: " << finalLoss << std::endl;
        std::cerr << "[GEOS] Theta[0..12]: ";
        for (int i = 0; i < BLOCK_AB_SIZE; i++)
            std::cerr << theta[i] << " ";
        std::cerr << std::endl;

        return iterCount;
    }

    // ============================================================
    // LINEAR_ONLY mode: skip ToneMapping (dials 3-9)
    // ============================================================
    // Optimizes only linear operations:
    //   Phase 1: ColorCorrection (3 dials: exposure, temp, tint)
    //   Phase 2: GlobalColor + SplitTone (7 dials)
    //   Phase 3: Joint CC+GC+ST refinement
    //   Phase 4: SelectiveColour (24 dials)
    // ToneMapping dials (3-9) stay at neutral 0.5
    int optimizeLinearOnly(
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
        initNeutral(theta);  // All at 0.5 (neutral)
        writeDials(link, theta);

        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[GEOS-LINEAR] Initial loss: " << initialLoss << std::endl;
        std::cerr << "[GEOS-LINEAR] ToneMapping dials [3-9] locked at neutral 0.5" << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Phase 1: ColorCorrection only (3 dials: 0-2)
        int phase1Iter = static_cast<int>(totalIter * 0.25f);
        std::cerr << "\n[GEOS-LINEAR] === Phase 1: ColorCorrection (3 dials) ===" << std::endl;
        float lossAfterCC = optimizeBlock(
            body, link, targetStyle, theta,
            LINEAR_A_START, LINEAR_A_SIZE,
            BLOCK_3D, phase1Iter,
            iterCount, totalIter,
            Progress::Phase::HUGE,
            targetLaplacianVar, progress, rng);

        // Phase 2: GlobalColor + SplitTone (7 dials: 10-16)
        int phase2Iter = static_cast<int>(totalIter * 0.20f);
        std::cerr << "\n[GEOS-LINEAR] === Phase 2: GlobalColor + SplitTone (7 dials) ===" << std::endl;
        float lossAfterGC = optimizeBlock(
            body, link, targetStyle, theta,
            LINEAR_B_START, LINEAR_B_SIZE,
            BLOCK_7D, phase2Iter,
            iterCount, totalIter,
            Progress::Phase::MIDS,
            targetLaplacianVar, progress, rng);

        // Phase 3: Joint CC + GC+ST refinement
        // We need a custom block optimizer that handles non-contiguous indices
        // For now, re-optimize CC with GC+ST fixed, then GC+ST with CC fixed
        int phase3Iter = static_cast<int>(totalIter * 0.20f);
        std::cerr << "\n[GEOS-LINEAR] === Phase 3: Joint CC+GC+ST refinement ===" << std::endl;
        // Alternate: CC then GC+ST
        optimizeBlock(body, link, targetStyle, theta,
            LINEAR_A_START, LINEAR_A_SIZE,
            BLOCK_3D, phase3Iter / 2,
            iterCount, totalIter,
            Progress::Phase::TINY,
            targetLaplacianVar, progress, rng);
        optimizeBlock(body, link, targetStyle, theta,
            LINEAR_B_START, LINEAR_B_SIZE,
            BLOCK_7D, phase3Iter / 2,
            iterCount, totalIter,
            Progress::Phase::TINY,
            targetLaplacianVar, progress, rng);

        // Phase 4: SelectiveColour (24 dials: 17-40)
        int phase4Iter = static_cast<int>(totalIter * 0.35f);
        std::cerr << "\n[GEOS-LINEAR] === Phase 4: SelectiveColour (24 dials) ===" << std::endl;
        float finalLoss = optimizeBlock(
            body, link, targetStyle, theta,
            LINEAR_C_START, LINEAR_C_SIZE,
            BLOCK_24D, phase4Iter,
            iterCount, totalIter,
            Progress::Phase::TINY,
            targetLaplacianVar, progress, rng);

        std::cerr << "\n[GEOS-LINEAR] === FINAL ===" << std::endl;
        std::cerr << "[GEOS-LINEAR] Final loss: " << finalLoss << std::endl;
        std::cerr << "[GEOS-LINEAR] Linear dials [0-2]: ";
        for (int i = 0; i < 3; i++) std::cerr << theta[i] << " ";
        std::cerr << std::endl;
        std::cerr << "[GEOS-LINEAR] ToneMap dials [3-9] (locked): ";
        for (int i = 3; i < 10; i++) std::cerr << theta[i] << " ";
        std::cerr << std::endl;
        std::cerr << "[GEOS-LINEAR] GlobalColor+SplitTone [10-16]: ";
        for (int i = 10; i < 17; i++) std::cerr << theta[i] << " ";
        std::cerr << std::endl;

        return iterCount;
    }

    // ============================================================
    // FULL_41D mode: all dials simultaneously
    // ============================================================
    int optimizeFull41D(
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
        std::cerr << "[GEOS-FULL41D] Initial loss: " << initialLoss << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Single phase: all 41 dials
        std::cerr << "\n[GEOS] === Full 41D optimization ===" << std::endl;
        float finalLoss = optimizeBlock(
            body, link, targetStyle, theta,
            0, GEOS_DIAL_COUNT,  // All 41 dials
            BLOCK_41D, totalIter,
            iterCount, totalIter,
            Progress::Phase::HUGE,  // Single long phase
            targetLaplacianVar, progress, rng);

        std::cerr << "\n[GEOS-FULL41D] === FINAL ===" << std::endl;
        std::cerr << "[GEOS] Final loss: " << finalLoss << std::endl;
        std::cerr << "[GEOS] Theta[0..16]: ";
        for (int i = 0; i < BLOCK_AB_SIZE; i++)
            std::cerr << theta[i] << " ";
        std::cerr << std::endl;
        std::cerr << "[GEOS] Theta[17..40] (selective): ";
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
        Callback progress,
        bool lutEstimated)
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
        if (config.geos_mode == Mode::FULL_35D)
        {
            return optimizeFull41D(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
        else if (config.geos_mode == Mode::LINEAR_ONLY)
        {
            return optimizeLinearOnly(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
        else
        {
            return optimizeBlockwise(body, link, targetStyle, targetLaplacianVar, config, progress, lutEstimated);
        }
    }

} // namespace geos::internal
