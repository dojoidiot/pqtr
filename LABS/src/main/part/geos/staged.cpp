// staged.cpp
// Stage-aware optimizer: VIEW then POPS
//
// Hypothesis: Dial interference occurs because all 45 dials optimize against
// one unified loss. VIEW dials (exposure, contrast) fight POPS dials
// (saturation, vibrance) because they both affect the same loss features.
//
// Solution: Optimize in two stages with stage-specific loss functions.
//   1. VIEW phase: 6 tone dials with viewLoss() (luminance-heavy)
//   2. POPS phase: 39 color dials with popsLoss() (chroma-heavy)
//   3. Optional joint refinement with geodesicLoss()

#include "spsa.hpp"
#include "diff.hpp"
#include <random>
#include <iostream>

namespace geos::internal
{
    // SPSA gain schedules (copied from spsa.cpp - internal implementation detail)
    static float computeA(int k, const PhaseParams& p)
    {
        return p.a0 / std::pow(static_cast<float>(k + 1) + p.A, p.alpha);
    }

    static float computeC(int k, const PhaseParams& p)
    {
        return p.c0 / std::pow(static_cast<float>(k + 1), p.gamma);
    }
    // ============================================================
    // VIEW dials: establish tone structure (6 dials)
    // ============================================================
    // These dials control brightness, contrast, and tone curve shape.
    // Optimized first with viewLoss() which heavily weights luminance features.

    constexpr int VIEW_DIAL_COUNT = 6;
    constexpr std::array<int, VIEW_DIAL_COUNT> VIEW_DIALS = {
        0,  // exposure
        3,  // contrast
        4,  // highlights
        5,  // shadows
        8,  // black
        9   // white
    };

    // SPSA parameters for VIEW phase (6D - can be aggressive)
    constexpr PhaseParams VIEW_PARAMS = { 0.25f, 0.15f, 0.602f, 0.101f, 10.0f };

    // ============================================================
    // POPS: Optimize by color axis groups (opponent color theory)
    // ============================================================
    // Instead of 39 dials at once, optimize in compensating groups:
    //
    // 1. GLOBAL: vibrance, saturation, colourDensity (3 dials)
    // 2. SPLIT:  shadow_temp/tint, highlight_temp/tint (4 dials)
    // 3. R-C:    Red + Cyan HSL (6 dials) - opponent axis
    // 4. G-M:    Green + Magenta HSL (6 dials) - opponent axis
    // 5. B-Y:    Blue + Yellow HSL (6 dials) - opponent axis
    // 6. O-P:    Orange + Purple HSL (6 dials) - secondary opponents
    //
    // Dial indices:
    //   GlobalColor: 10,11,12
    //   SplitTone:   13,14,15,16
    //   Red:    17,18,19   Cyan:    29,30,31
    //   Orange: 20,21,22   Purple:  35,36,37
    //   Yellow: 23,24,25   Blue:    32,33,34
    //   Green:  26,27,28   Magenta: 38,39,40

    // Group definitions
    constexpr std::array<int, 3> GLOBAL_DIALS = {10, 11, 12};
    constexpr std::array<int, 4> SPLIT_DIALS = {13, 14, 15, 16};
    constexpr std::array<int, 6> RC_DIALS = {17, 18, 19, 29, 30, 31};  // Red + Cyan
    constexpr std::array<int, 6> GM_DIALS = {26, 27, 28, 38, 39, 40};  // Green + Magenta
    constexpr std::array<int, 6> BY_DIALS = {32, 33, 34, 23, 24, 25};  // Blue + Yellow
    constexpr std::array<int, 6> OP_DIALS = {20, 21, 22, 35, 36, 37};  // Orange + Purple

    // SPSA parameters for small groups (3-6 dials - can be aggressive)
    constexpr PhaseParams POPS_PARAMS = { 0.20f, 0.12f, 0.602f, 0.101f, 15.0f };

    // Joint refinement params (45D)
    constexpr PhaseParams JOINT_PARAMS = { 0.08f, 0.05f, 0.602f, 0.101f, 50.0f };

    // ============================================================
    // Helper: check if dial is VIEW dial
    // ============================================================
    static bool isViewDial(int dialIndex)
    {
        for (int d : VIEW_DIALS)
        {
            if (d == dialIndex) return true;
        }
        return false;
    }

    // ============================================================
    // Evaluate VIEW loss (viewLoss wrapper)
    // ============================================================
    static float evaluateViewLoss(pipe::Body& body, const StyleFeatures& target)
    {
        cv::UMat candidate;
        body.view().copyTo(candidate);  // Explicit copy to avoid dangling ref
        cv::UMat proxy = resizeProxy(candidate);
        StyleFeatures candStyle = extractStyleFromBGR(proxy);
        return viewLoss(target, candStyle);
    }

    // ============================================================
    // Evaluate POPS loss (popsLoss wrapper)
    // ============================================================
    static float evaluatePopsLoss(pipe::Body& body, const StyleFeatures& target)
    {
        cv::UMat candidate;
        body.view().copyTo(candidate);  // Explicit copy to avoid dangling ref
        cv::UMat proxy = resizeProxy(candidate);
        StyleFeatures candStyle = extractStyleFromBGR(proxy);
        return popsLoss(target, candStyle);
    }

    // ============================================================
    // VIEW phase: Grid search over 6 tone dials
    // ============================================================
    // Instead of SPSA (which oscillates on non-convex landscapes),
    // exhaustively search the VIEW space. 5 values per dial = 15,625 combos.

    // Grid values for each VIEW dial (centered around neutral 0.5)
    constexpr std::array<float, 5> VIEW_GRID = {0.35f, 0.425f, 0.5f, 0.575f, 0.65f};

    // Structure to hold a VIEW candidate
    struct ViewCandidate
    {
        std::array<float, VIEW_DIAL_COUNT> values;
        float loss;
    };

    static float optimizeViewPhase(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& target,
        Theta& theta,
        int maxIter,  // Not used for grid search, kept for API compat
        int& totalIters,
        Callback progress)
    {
        std::cerr << "[VIEW] Grid search (5^6 = 15625 combinations)" << std::endl;

        // Track best candidates
        constexpr int TOP_N = 5;
        std::vector<ViewCandidate> topCandidates;
        float worstTopLoss = 999.0f;

        int evalCount = 0;
        int totalCombos = 15625;  // 5^6

        // Exhaustive grid search over all 6 VIEW dials
        for (int i0 = 0; i0 < 5; i0++)
        {
            for (int i1 = 0; i1 < 5; i1++)
            {
                for (int i2 = 0; i2 < 5; i2++)
                {
                    for (int i3 = 0; i3 < 5; i3++)
                    {
                        for (int i4 = 0; i4 < 5; i4++)
                        {
                            for (int i5 = 0; i5 < 5; i5++)
                            {
                                // Set VIEW dials
                                theta[VIEW_DIALS[0]] = VIEW_GRID[i0];  // exposure
                                theta[VIEW_DIALS[1]] = VIEW_GRID[i1];  // contrast
                                theta[VIEW_DIALS[2]] = VIEW_GRID[i2];  // highlights
                                theta[VIEW_DIALS[3]] = VIEW_GRID[i3];  // shadows
                                theta[VIEW_DIALS[4]] = VIEW_GRID[i4];  // black
                                theta[VIEW_DIALS[5]] = VIEW_GRID[i5];  // white

                                writeDials(link, theta);
                                float loss = evaluateViewLoss(body, target);

                                evalCount++;
                                totalIters++;

                                // Track top N candidates
                                if (topCandidates.size() < TOP_N)
                                {
                                    ViewCandidate c;
                                    c.values = {VIEW_GRID[i0], VIEW_GRID[i1], VIEW_GRID[i2],
                                                VIEW_GRID[i3], VIEW_GRID[i4], VIEW_GRID[i5]};
                                    c.loss = loss;
                                    topCandidates.push_back(c);

                                    // Update worst
                                    worstTopLoss = 0;
                                    for (const auto& tc : topCandidates)
                                        if (tc.loss > worstTopLoss) worstTopLoss = tc.loss;
                                }
                                else if (loss < worstTopLoss)
                                {
                                    // Replace worst candidate
                                    int worstIdx = 0;
                                    for (int w = 0; w < TOP_N; w++)
                                    {
                                        if (topCandidates[w].loss == worstTopLoss)
                                        {
                                            worstIdx = w;
                                            break;
                                        }
                                    }

                                    topCandidates[worstIdx].values = {
                                        VIEW_GRID[i0], VIEW_GRID[i1], VIEW_GRID[i2],
                                        VIEW_GRID[i3], VIEW_GRID[i4], VIEW_GRID[i5]};
                                    topCandidates[worstIdx].loss = loss;

                                    // Update worst
                                    worstTopLoss = 0;
                                    for (const auto& tc : topCandidates)
                                        if (tc.loss > worstTopLoss) worstTopLoss = tc.loss;
                                }

                                // Progress every 1000 evals
                                if (evalCount % 1000 == 0)
                                {
                                    float bestSoFar = 999.0f;
                                    for (const auto& tc : topCandidates)
                                        if (tc.loss < bestSoFar) bestSoFar = tc.loss;

                                    std::cerr << "[VIEW] " << evalCount << "/" << totalCombos
                                              << " best=" << bestSoFar << std::endl;

                                    if (progress)
                                    {
                                        Progress p;
                                        p.stage = Progress::Stage::GEOS;
                                        p.phase = Progress::Phase::HUGE;
                                        p.iteration = evalCount;
                                        p.max_iterations = totalCombos;
                                        p.loss.spectral = bestSoFar;
                                        if (!progress(p)) goto done;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    done:

        // Find best candidate
        float bestLoss = 999.0f;
        int bestIdx = 0;
        for (int i = 0; i < (int)topCandidates.size(); i++)
        {
            if (topCandidates[i].loss < bestLoss)
            {
                bestLoss = topCandidates[i].loss;
                bestIdx = i;
            }
        }

        // Apply best VIEW settings
        for (int i = 0; i < VIEW_DIAL_COUNT; i++)
        {
            theta[VIEW_DIALS[i]] = topCandidates[bestIdx].values[i];
        }
        writeDials(link, theta);

        std::cerr << "[VIEW] Grid search complete. Best loss=" << bestLoss << std::endl;
        std::cerr << "[VIEW] Best dials: ";
        for (int i = 0; i < VIEW_DIAL_COUNT; i++)
            std::cerr << topCandidates[bestIdx].values[i] << " ";
        std::cerr << std::endl;

        // Report top 5
        std::cerr << "[VIEW] Top " << topCandidates.size() << " candidates:" << std::endl;
        for (const auto& c : topCandidates)
        {
            std::cerr << "  loss=" << c.loss << " dials=[";
            for (int i = 0; i < VIEW_DIAL_COUNT; i++)
                std::cerr << c.values[i] << (i < 5 ? "," : "");
            std::cerr << "]" << std::endl;
        }

        return bestLoss;
    }

    // ============================================================
    // POPS: optimize one dial group with SPSA
    // ============================================================
    template<size_t N>
    static float optimizeGroup(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& target,
        Theta& theta,
        const std::array<int, N>& dials,
        const char* groupName,
        int maxIter,
        int& totalIters,
        Callback progress)
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::bernoulli_distribution coin(0.5);

        Theta bestTheta = theta;
        float bestLoss = evaluatePopsLoss(body, target);

        std::array<float, N> delta;
        int itersSinceImprovement = 0;

        for (int k = 0; k < maxIter && itersSinceImprovement < STALL_THRESHOLD; k++)
        {
            float a_k = computeA(k, POPS_PARAMS);
            float c_k = computeC(k, POPS_PARAMS);

            // Generate perturbation for this group only
            for (size_t i = 0; i < N; i++)
                delta[i] = coin(rng) ? 1.0f : -1.0f;

            // Evaluate L+
            Theta thetaPlus = theta;
            for (size_t i = 0; i < N; i++)
            {
                int d = dials[i];
                thetaPlus[d] = std::clamp(theta[d] + c_k * delta[i], 0.0f, 1.0f);
            }
            writeDials(link, thetaPlus);
            float lossPlus = evaluatePopsLoss(body, target);

            // Evaluate L-
            Theta thetaMinus = theta;
            for (size_t i = 0; i < N; i++)
            {
                int d = dials[i];
                thetaMinus[d] = std::clamp(theta[d] - c_k * delta[i], 0.0f, 1.0f);
            }
            writeDials(link, thetaMinus);
            float lossMinus = evaluatePopsLoss(body, target);

            // Gradient estimate and update
            for (size_t i = 0; i < N; i++)
            {
                int d = dials[i];
                float g_i = (lossPlus - lossMinus) / (2.0f * c_k * delta[i]);
                theta[d] = std::clamp(theta[d] - a_k * g_i, 0.0f, 1.0f);
            }
            writeDials(link, theta);

            float newLoss = evaluatePopsLoss(body, target);

            if (newLoss < bestLoss)
            {
                float improvement = bestLoss - newLoss;
                if (improvement / bestLoss >= MIN_RELATIVE_IMPROVEMENT ||
                    improvement >= MIN_ABSOLUTE_IMPROVEMENT)
                {
                    itersSinceImprovement = 0;
                }
                else
                {
                    itersSinceImprovement++;
                }
                bestLoss = newLoss;
                bestTheta = theta;
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

            totalIters++;

            if (progress)
            {
                Progress p;
                p.stage = Progress::Stage::GEOS;
                p.phase = Progress::Phase::MIDS;
                p.iteration = totalIters;
                p.max_iterations = maxIter * 6;  // 6 groups
                p.loss.spectral = newLoss;
                if (!progress(p)) break;
            }
        }

        theta = bestTheta;
        writeDials(link, theta);
        return bestLoss;
    }

    // ============================================================
    // POPS phase: optimize by color axis groups (opponent pairs)
    // ============================================================
    static float optimizePopsPhase(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& target,
        Theta& theta,
        int maxIter,
        int& totalIters,
        Callback progress)
    {
        float startLoss = evaluatePopsLoss(body, target);
        std::cerr << "[POPS] Start loss=" << startLoss << " (axis-based)" << std::endl;

        // Allocate iterations per group (proportional to dial count)
        // Total: 3+4+6+6+6+6 = 31 dials, but we give each group decent budget
        int iterPerAxis = maxIter / 8;  // ~6 axis groups + some slack

        // 1. Global saturation (vibrance, saturation, colourDensity)
        std::cerr << "[POPS] GLOBAL (3 dials)..." << std::endl;
        float loss = optimizeGroup(body, link, target, theta, GLOBAL_DIALS, "GLOBAL", iterPerAxis, totalIters, progress);

        // 2. Split tone (shadow/highlight balance)
        std::cerr << "[POPS] SPLIT (4 dials)..." << std::endl;
        loss = optimizeGroup(body, link, target, theta, SPLIT_DIALS, "SPLIT", iterPerAxis, totalIters, progress);

        // 3. Red-Cyan axis (opponent pair)
        std::cerr << "[POPS] R-C axis (6 dials)..." << std::endl;
        loss = optimizeGroup(body, link, target, theta, RC_DIALS, "R-C", iterPerAxis, totalIters, progress);

        // 4. Green-Magenta axis (opponent pair)
        std::cerr << "[POPS] G-M axis (6 dials)..." << std::endl;
        loss = optimizeGroup(body, link, target, theta, GM_DIALS, "G-M", iterPerAxis, totalIters, progress);

        // 5. Blue-Yellow axis (opponent pair)
        std::cerr << "[POPS] B-Y axis (6 dials)..." << std::endl;
        loss = optimizeGroup(body, link, target, theta, BY_DIALS, "B-Y", iterPerAxis, totalIters, progress);

        // 6. Orange-Purple axis (secondary opponents)
        std::cerr << "[POPS] O-P axis (6 dials)..." << std::endl;
        loss = optimizeGroup(body, link, target, theta, OP_DIALS, "O-P", iterPerAxis, totalIters, progress);

        float endLoss = evaluatePopsLoss(body, target);
        std::cerr << "[POPS] End loss=" << endLoss << std::endl;
        return endLoss;
    }

    // ============================================================
    // Joint refinement: all 45 dials with unified loss
    // ============================================================
    static float optimizeJointPhase(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& target,
        Theta& theta,
        int maxIter,
        int& totalIters,
        Callback progress)
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::bernoulli_distribution coin(0.5);

        Theta bestTheta = theta;
        float bestLoss = evaluateLoss(body, target);

        std::cerr << "[JOINT] Start loss=" << bestLoss << " (45 dials)" << std::endl;

        std::array<float, GEOS_DIAL_COUNT> delta;
        int itersSinceImprovement = 0;

        for (int k = 0; k < maxIter && itersSinceImprovement < STALL_THRESHOLD; k++)
        {
            float a_k = computeA(k, JOINT_PARAMS);
            float c_k = computeC(k, JOINT_PARAMS);

            // Generate perturbation for all dials
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
                delta[i] = coin(rng) ? 1.0f : -1.0f;

            // Evaluate L+
            Theta thetaPlus = theta;
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
                thetaPlus[i] = std::clamp(theta[i] + c_k * delta[i], 0.0f, 1.0f);
            writeDials(link, thetaPlus);
            float lossPlus = evaluateLoss(body, target);

            // Evaluate L-
            Theta thetaMinus = theta;
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
                thetaMinus[i] = std::clamp(theta[i] - c_k * delta[i], 0.0f, 1.0f);
            writeDials(link, thetaMinus);
            float lossMinus = evaluateLoss(body, target);

            // Gradient estimate and update
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                float g_i = (lossPlus - lossMinus) / (2.0f * c_k * delta[i]);
                theta[i] = std::clamp(theta[i] - a_k * g_i, 0.0f, 1.0f);
            }
            writeDials(link, theta);

            float newLoss = evaluateLoss(body, target);

            // Track improvement
            if (newLoss < bestLoss)
            {
                float improvement = bestLoss - newLoss;
                if (improvement / bestLoss >= MIN_RELATIVE_IMPROVEMENT ||
                    improvement >= MIN_ABSOLUTE_IMPROVEMENT)
                {
                    itersSinceImprovement = 0;
                    if (k % 25 == 0)
                        std::cerr << "[JOINT] k=" << k << " loss=" << newLoss << std::endl;
                }
                else
                {
                    itersSinceImprovement++;
                }
                bestLoss = newLoss;
                bestTheta = theta;
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

            totalIters++;

            if (progress)
            {
                Progress p;
                p.stage = Progress::Stage::GEOS;
                p.phase = Progress::Phase::TINY;
                p.iteration = totalIters;
                p.max_iterations = maxIter * 3;
                p.loss.spectral = newLoss;
                if (!progress(p)) break;
            }
        }

        theta = bestTheta;
        writeDials(link, theta);

        std::cerr << "[JOINT] End loss=" << bestLoss << std::endl;
        return bestLoss;
    }

    // ============================================================
    // STAGED mode entry point
    // ============================================================
    int optimizeStaged(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress)
    {
        // Initialize all dials to neutral
        Theta theta;
        initNeutral(theta);
        writeDials(link, theta);

        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "\n[STAGED] === Stage-Aware Optimization ===" << std::endl;
        std::cerr << "[STAGED] Initial unified loss: " << initialLoss << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Phase allocation: VIEW 30%, POPS 50%, Joint 20%
        int viewIter = static_cast<int>(totalIter * 0.30f);
        int popsIter = static_cast<int>(totalIter * 0.50f);
        int jointIter = static_cast<int>(totalIter * 0.20f);

        // Phase 1: VIEW - establish tone structure
        std::cerr << "\n[STAGED] === Phase 1: VIEW (tone structure) ===" << std::endl;
        float viewLossVal = optimizeViewPhase(body, link, targetStyle, theta, viewIter, iterCount, progress);

        // Phase 2: POPS - match color/saturation
        std::cerr << "\n[STAGED] === Phase 2: POPS (color/saturation) ===" << std::endl;
        float popsLossVal = optimizePopsPhase(body, link, targetStyle, theta, popsIter, iterCount, progress);

        // Phase 3: Joint refinement
        std::cerr << "\n[STAGED] === Phase 3: Joint refinement ===" << std::endl;
        float jointLoss = optimizeJointPhase(body, link, targetStyle, theta, jointIter, iterCount, progress);

        // Final report
        float finalLoss = evaluateLoss(body, targetStyle);
        std::cerr << "\n[STAGED] === FINAL ===" << std::endl;
        std::cerr << "[STAGED] VIEW loss (after phase 1): " << viewLossVal << std::endl;
        std::cerr << "[STAGED] POPS loss (after phase 2): " << popsLossVal << std::endl;
        std::cerr << "[STAGED] Final unified loss: " << finalLoss << std::endl;
        std::cerr << "[STAGED] Improvement: " << (initialLoss - finalLoss) / initialLoss * 100.0f << "%" << std::endl;

        // Print dial values
        std::cerr << "[STAGED] VIEW dials: ";
        for (int d : VIEW_DIALS)
            std::cerr << theta[d] << " ";
        std::cerr << std::endl;

        return iterCount;
    }

} // namespace geos::internal
