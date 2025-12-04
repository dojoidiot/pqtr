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
#include <fstream>

namespace geos::internal
{
    // ============================================================
    // Covariance accumulator (Welford's online algorithm)
    // ============================================================
    // Accumulates dial samples to build correlation matrix.
    // SPSA explores full 45D space, making it ideal for bootstrapping
    // the covariance prior that ACEO will use.

    using MatrixN = std::array<float, GEOS_DIAL_COUNT * GEOS_DIAL_COUNT>;  // 45x45
    using VectorN = std::array<float, GEOS_DIAL_COUNT>;                     // 45

    struct CovarianceAccumulator
    {
        int n = 0;                    // Sample count
        VectorN mean;                 // Running mean
        MatrixN M2;                   // Sum of squared deviations (for covariance)

        CovarianceAccumulator()
        {
            mean.fill(0.0f);
            M2.fill(0.0f);
        }

        // Add a sample using Welford's online algorithm
        void update(const Theta& sample)
        {
            n++;
            VectorN delta;
            VectorN delta2;

            // Update mean and compute deltas
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                delta[i] = sample[i] - mean[i];
                mean[i] += delta[i] / static_cast<float>(n);
                delta2[i] = sample[i] - mean[i];
            }

            // Update M2 (outer product of deltas)
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                for (int j = 0; j < GEOS_DIAL_COUNT; j++)
                {
                    M2[i * GEOS_DIAL_COUNT + j] += delta[i] * delta2[j];
                }
            }
        }

        // Get covariance matrix (n-1 normalization for unbiased estimate)
        bool getCovariance(MatrixN& cov) const
        {
            if (n < 2) return false;

            float invN = 1.0f / static_cast<float>(n - 1);
            for (int i = 0; i < GEOS_DIAL_COUNT * GEOS_DIAL_COUNT; i++)
            {
                cov[i] = M2[i] * invN;
            }
            return true;
        }

        // Get correlation matrix (normalized covariance)
        bool getCorrelation(MatrixN& corr) const
        {
            MatrixN cov;
            if (!getCovariance(cov)) return false;

            // Extract standard deviations from diagonal
            VectorN stddev;
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                float var = cov[i * GEOS_DIAL_COUNT + i];
                stddev[i] = var > 1e-10f ? std::sqrt(var) : 1e-5f;
            }

            // Normalize: corr[i,j] = cov[i,j] / (std[i] * std[j])
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                for (int j = 0; j < GEOS_DIAL_COUNT; j++)
                {
                    corr[i * GEOS_DIAL_COUNT + j] = cov[i * GEOS_DIAL_COUNT + j] / (stddev[i] * stddev[j]);
                }
            }
            return true;
        }

        // Save to JSON file
        bool saveToJson(const std::string& path) const
        {
            MatrixN corr;
            if (!getCorrelation(corr))
            {
                std::cerr << "[SPSA-COV] Not enough samples to save (n=" << n << ")" << std::endl;
                return false;
            }

            std::ofstream file(path);
            if (!file.is_open())
            {
                std::cerr << "[SPSA-COV] Failed to open: " << path << std::endl;
                return false;
            }

            file << "{\n";
            file << "  \"sample_count\": " << n << ",\n";
            file << "  \"source\": \"spsa\",\n";
            file << "  \"mean\": [";
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                if (i > 0) file << ", ";
                file << mean[i];
            }
            file << "],\n";
            file << "  \"correlation_matrix\": [\n";
            for (int i = 0; i < GEOS_DIAL_COUNT; i++)
            {
                file << "    [";
                for (int j = 0; j < GEOS_DIAL_COUNT; j++)
                {
                    if (j > 0) file << ", ";
                    file << corr[i * GEOS_DIAL_COUNT + j];
                }
                file << "]";
                if (i < GEOS_DIAL_COUNT - 1) file << ",";
                file << "\n";
            }
            file << "  ]\n";
            file << "}\n";

            std::cerr << "[SPSA-COV] Saved covariance (" << n << " samples) to: " << path << std::endl;
            return true;
        }
    };

    // Global covariance accumulator for SPSA runs
    static CovarianceAccumulator g_spsaCovAccum;

    // ============================================================
    // Jacobian-informed gradient (feedforward from features)
    // ============================================================

    // Global Jacobian (loaded once, reused across images)
    static JacobianMatrix g_jacobian;
    static bool g_jacobianLoaded = false;

    bool loadJacobian(const std::string& path, JacobianMatrix& J)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "[SPSA] Failed to load Jacobian: " << path << std::endl;
            return false;
        }

        // Simple JSON parser for our specific format
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Find "matrix": [ and parse the 45x23 array
        size_t matrixStart = content.find("\"matrix\":");
        if (matrixStart == std::string::npos)
        {
            std::cerr << "[SPSA] Invalid Jacobian format (no matrix)" << std::endl;
            return false;
        }

        // Find the opening bracket
        size_t pos = content.find('[', matrixStart);
        if (pos == std::string::npos) return false;
        pos++; // Skip '['

        // Parse 45 rows
        for (int d = 0; d < GEOS_DIAL_COUNT; d++)
        {
            // Find row start
            pos = content.find('[', pos);
            if (pos == std::string::npos) return false;
            pos++; // Skip '['

            // Parse 23 values
            for (int f = 0; f < STYLE_DIM; f++)
            {
                // Skip whitespace and find number
                while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n')) pos++;

                // Parse float
                size_t end;
                J[d][f] = std::stof(content.substr(pos), &end);
                pos += end;

                // Skip comma if present
                while (pos < content.size() && (content[pos] == ',' || content[pos] == ' ')) pos++;
            }

            // Find row end
            pos = content.find(']', pos);
            if (pos == std::string::npos) return false;
            pos++; // Skip ']'
        }

        std::cerr << "[SPSA] Loaded Jacobian from: " << path << std::endl;
        return true;
    }

    void computeJacobianGradient(
        const JacobianMatrix& J,
        const StyleFeatures& target,
        const StyleFeatures& current,
        Theta& gradient)
    {
        // gradient[d] = sum_f( J[d][f] * W[f] * (target[f] - current[f]) )
        // This is J^T @ (W * residual)

        for (int d = 0; d < GEOS_DIAL_COUNT; d++)
        {
            float sum = 0.0f;
            for (int f = 0; f < STYLE_DIM; f++)
            {
                float residual = target.v[f] - current.v[f];
                float weighted = FEATURE_WEIGHTS[f] * residual;
                sum += J[d][f] * weighted;
            }
            gradient[d] = sum;
        }

        // Normalize gradient to unit length (direction only, magnitude from learning rate)
        float norm = 0.0f;
        for (int d = 0; d < GEOS_DIAL_COUNT; d++)
        {
            norm += gradient[d] * gradient[d];
        }
        norm = std::sqrt(norm);
        if (norm > 1e-6f)
        {
            for (int d = 0; d < GEOS_DIAL_COUNT; d++)
            {
                gradient[d] /= norm;
            }
        }
    }

    // ============================================================
    // Dial mapping: 45 dials <-> theta vector [0,1]^45
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
    //   [41]    sharpen_amount     |
    //   [42]    sharpen_radius     | Detail (4)
    //   [43]    denoise_luma       |
    //   [44]    denoise_chroma     |

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

        // Detail (4)
        theta[41] = link.detail().sharpen().amount();
        theta[42] = link.detail().sharpen().radius();
        theta[43] = link.detail().denoise().luminance().get();
        theta[44] = link.detail().denoise().chroma().get();
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

        // Detail (4)
        link.detail().sharpen().amount(theta[41]);
        link.detail().sharpen().radius(theta[42]);
        link.detail().denoise().luminance().set(theta[43]);
        link.detail().denoise().chroma().set(theta[44]);
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

    // Compute loss for current body state (global only)
    float evaluateLoss(
        pipe::Body& body,
        const StyleFeatures& targetStyle)
    {
        View candidate = body.view();
        cv::UMat candProxy = resizeProxy(candidate);
        StyleFeatures candStyle = extractStyleFromBGR(candProxy);  // Use BGR for full 18D features
        return geodesicLoss(targetStyle, candStyle);
    }

    // Compute combined loss: spectral + frequency (for holistic optimization)
    // Used by --full mode where edge dials are optimized together with color/tone
    float evaluateCombinedLoss(
        pipe::Body& body,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        float freqWeight)  // Frequency weight (default 15% - specified in header)
    {
        View candidate = body.view();
        cv::UMat candProxy = resizeProxy(candidate);

        // Spectral loss (color/tone) - use BGR for full 18D features
        StyleFeatures candStyle = extractStyleFromBGR(candProxy);
        float spectral = geodesicLoss(targetStyle, candStyle);

        // Frequency loss (sharpness)
        float candLaplacianVar = laplacianVariance(candProxy);
        float frequency;
        if (targetLaplacianVar < 1e-6f)
            frequency = (candLaplacianVar < 1e-6f) ? 0.0f : 1.0f;
        else
            frequency = std::abs(candLaplacianVar - targetLaplacianVar) / targetLaplacianVar;

        // Combined: spectral dominates, frequency contributes
        // Clamp frequency to reasonable range (can be >1)
        frequency = std::min(frequency, 2.0f);
        return spectral + freqWeight * frequency;
    }

    // Compute loss with regional support (for DISPLAY mode)
    float evaluateLossRegional(
        pipe::Body& body,
        const TargetFeatures& target,
        LossMode mode,
        float globalWeight = 0.3f)
    {
        View candidate = body.view();
        return computeProgressiveLoss(candidate, target, mode, globalWeight);
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
    // Jacobian-informed perturbation direction
    // ============================================================
    // Returns sign of Jacobian gradient for each dial (biased SPSA)
    // If Jacobian unavailable, returns random ±1 (fallback to pure SPSA)
    void computeJacobianDelta(
        pipe::Body& body,
        const StyleFeatures& targetStyle,
        const JacobianMatrix& J,
        bool jacobianValid,
        int blockStart,
        int blockSize,
        std::array<float, GEOS_DIAL_COUNT>& delta,
        std::mt19937& rng,
        float jacobianBlend = 0.7f)  // 70% Jacobian, 30% random
    {
        std::bernoulli_distribution coin(0.5);

        if (!jacobianValid)
        {
            // Fallback: pure random SPSA
            for (int i = 0; i < blockSize; i++)
            {
                delta[i] = coin(rng) ? 1.0f : -1.0f;
            }
            return;
        }

        // Get current features
        View candidate = body.view();
        cv::UMat candProxy = resizeProxy(candidate);
        StyleFeatures currentStyle = extractStyleFromBGR(candProxy);

        // Compute Jacobian gradient for each dial in block
        for (int i = 0; i < blockSize; i++)
        {
            int d = blockStart + i;
            float grad = 0.0f;
            for (int f = 0; f < STYLE_DIM; f++)
            {
                float residual = targetStyle.v[f] - currentStyle.v[f];
                grad += J[d][f] * FEATURE_WEIGHTS[f] * residual;
            }

            // Blend Jacobian direction with random
            float jacobianDir = (grad > 0) ? 1.0f : -1.0f;
            float randomDir = coin(rng) ? 1.0f : -1.0f;

            // Use Jacobian direction most of the time, random occasionally
            std::uniform_real_distribution<float> blend(0.0f, 1.0f);
            delta[i] = (blend(rng) < jacobianBlend) ? jacobianDir : randomDir;
        }
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

        // Load Jacobian once (if available)
        if (!g_jacobianLoaded)
        {
            g_jacobianLoaded = loadJacobian("etc/jacob.json", g_jacobian);
            if (g_jacobianLoaded)
            {
                std::cerr << "[BLOCK] Using Jacobian-informed SPSA (hybrid mode)" << std::endl;
            }
        }

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

            // Generate perturbation - Jacobian-informed if available
            computeJacobianDelta(body, targetStyle, g_jacobian, g_jacobianLoaded,
                                blockStart, blockSize, delta, rng);

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

            // Accumulate covariance sample (full theta vector)
            g_spsaCovAccum.update(theta);

            float newLoss = evaluateLoss(body, targetStyle);

            // Track best - check for MEANINGFUL improvement
            if (newLoss < bestLoss)
            {
                float improvement = bestLoss - newLoss;
                float relativeImprovement = improvement / bestLoss;

                // Only reset stall counter if improvement is meaningful
                if (relativeImprovement >= MIN_RELATIVE_IMPROVEMENT ||
                    improvement >= MIN_ABSOLUTE_IMPROVEMENT)
                {
                    itersSinceImprovement = 0;
                    if (k % 20 == 0 || newLoss < 0.02f)
                    {
                        std::cerr << "[BEST] k=" << k << " loss=" << newLoss << std::endl;
                    }
                }
                else
                {
                    itersSinceImprovement++;  // Marginal improvement = count as stall
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

            // Early termination if stalled (no meaningful improvement)
            if (itersSinceImprovement >= STALL_THRESHOLD)
            {
                std::cerr << "[BLOCK] Early stop: no meaningful improvement for "
                          << STALL_THRESHOLD << " iterations (loss=" << bestLoss << ")" << std::endl;
                break;
            }

            iterCount++;

            // Progress callback
            if (progress)
            {
                cv::UMat candProxy = resizeProxy(body.view());
                StyleFeatures candStyle = extractStyleFromBGR(candProxy);  // Use BGR for full 18D features
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

        // Read current dials (don't reset - preserves ACEO progress in HYBRID mode)
        Theta theta;
        readDials(link, theta);

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
    // SCENE_LINEAR mode: 5 non-contiguous dials for linear link
    // ============================================================
    int optimizeSceneLinear(
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
        std::cerr << "[GEOS-SCENE_LINEAR] Initial loss: " << initialLoss << std::endl;
        std::cerr << "[GEOS-SCENE_LINEAR] Optimizing dials: exposure(0), temp(1), tint(2), black(8), white(9)" << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        Theta bestTheta = theta;
        float bestLoss = initialLoss;

        std::bernoulli_distribution coin(0.5);
        std::array<float, 5> delta;

        int itersSinceImprovement = 0;

        for (int k = 0; k < totalIter; k++)
        {
            float a_k = computeA(k, BLOCK_5D);
            float c_k = computeC(k, BLOCK_5D);

            // Generate perturbation for the 5 scene-linear dials
            for (int i = 0; i < 5; i++)
                delta[i] = coin(rng) ? 1.0f : -1.0f;

            // Evaluate L+ (perturbed positive)
            Theta thetaPlus = theta;
            for (int i = 0; i < 5; i++)
                thetaPlus[SCENE_LINEAR_DIALS[i]] = std::clamp(theta[SCENE_LINEAR_DIALS[i]] + c_k * delta[i], 0.0f, 1.0f);
            writeDials(link, thetaPlus);
            float lossPlus = evaluateLoss(body, targetStyle);

            // Evaluate L- (perturbed negative)
            Theta thetaMinus = theta;
            for (int i = 0; i < 5; i++)
                thetaMinus[SCENE_LINEAR_DIALS[i]] = std::clamp(theta[SCENE_LINEAR_DIALS[i]] - c_k * delta[i], 0.0f, 1.0f);
            writeDials(link, thetaMinus);
            float lossMinus = evaluateLoss(body, targetStyle);

            // Gradient estimate and update
            for (int i = 0; i < 5; i++)
            {
                float g_i = (lossPlus - lossMinus) / (2.0f * c_k * delta[i]);
                theta[SCENE_LINEAR_DIALS[i]] = std::clamp(theta[SCENE_LINEAR_DIALS[i]] - a_k * g_i, 0.0f, 1.0f);
            }
            writeDials(link, theta);

            // Accumulate covariance sample
            g_spsaCovAccum.update(theta);

            float newLoss = evaluateLoss(body, targetStyle);

            // Track best - check for MEANINGFUL improvement
            if (newLoss < bestLoss)
            {
                float improvement = bestLoss - newLoss;
                float relativeImprovement = improvement / bestLoss;

                if (relativeImprovement >= MIN_RELATIVE_IMPROVEMENT ||
                    improvement >= MIN_ABSOLUTE_IMPROVEMENT)
                {
                    itersSinceImprovement = 0;
                    if (k % 20 == 0)
                        std::cerr << "[BEST] k=" << k << " loss=" << newLoss << std::endl;
                }
                else
                {
                    itersSinceImprovement++;  // Marginal improvement = count as stall
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

            if (itersSinceImprovement >= STALL_THRESHOLD)
            {
                std::cerr << "[GEOS-SCENE_LINEAR] Early stop: no meaningful improvement (loss=" << bestLoss << ")" << std::endl;
                break;
            }

            iterCount++;

            if (progress)
            {
                Progress p;
                p.stage = Progress::Stage::GEOS;
                p.phase = Progress::Phase::TINY;
                p.iteration = iterCount;
                p.max_iterations = totalIter;
                p.loss.spectral = newLoss;
                if (!progress(p)) break;
            }
        }

        theta = bestTheta;
        writeDials(link, theta);

        std::cerr << "[GEOS-SCENE_LINEAR] Final loss: " << bestLoss << std::endl;
        std::cerr << "[GEOS-SCENE_LINEAR] Final dials: exp=" << theta[0] << " temp=" << theta[1]
                  << " tint=" << theta[2] << " black=" << theta[8] << " white=" << theta[9] << std::endl;

        return iterCount;
    }

    // ============================================================
    // DISPLAY mode: skip scene-linear dials, optimize rest
    // Uses regional loss for refinement phases
    // ============================================================
    int optimizeDisplay(
        pipe::Body& body,
        pipe::Body::Link& link,
        const StyleFeatures& targetStyle,
        float targetLaplacianVar,
        const Config& config,
        Callback progress,
        bool lutEstimated,
        const TargetFeatures* targetFeatures)
    {
        std::random_device rd;
        std::mt19937 rng(rd());

        Theta theta;
        initNeutral(theta);
        writeDials(link, theta);

        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[GEOS-DISPLAY] Initial loss: " << initialLoss << std::endl;
        std::cerr << "[GEOS-DISPLAY] Skipping scene-linear dials (0,1,2,8,9)" << std::endl;
        if (targetFeatures)
            std::cerr << "[GEOS-DISPLAY] Regional loss ENABLED for refinement" << std::endl;
        else
            std::cerr << "[GEOS-DISPLAY] Regional loss DISABLED (no target features)" << std::endl;

        int totalIter = config.geos_max_iter;
        int iterCount = 0;

        // Phase 1: ToneMapping curves (3-7) - GLOBAL ONLY (fast exploration)
        int phase1Iter = static_cast<int>(totalIter * 0.30f);
        std::cerr << "\n[GEOS-DISPLAY] === Phase 1: ToneMapping curves (5 dials) [GLOBAL] ===" << std::endl;
        optimizeBlock(body, link, targetStyle, theta,
            DISPLAY_A_START, DISPLAY_A_SIZE,
            BLOCK_5D, phase1Iter,
            iterCount, totalIter,
            Progress::Phase::HUGE,
            targetLaplacianVar, progress, rng);

        // Phase 2: GlobalColor + SplitTone (10-16) - GLOBAL ONLY (medium exploration)
        int phase2Iter = static_cast<int>(totalIter * 0.25f);
        std::cerr << "\n[GEOS-DISPLAY] === Phase 2: GlobalColor + SplitTone (7 dials) [GLOBAL] ===" << std::endl;
        optimizeBlock(body, link, targetStyle, theta,
            DISPLAY_B_START, DISPLAY_B_SIZE,
            BLOCK_7D, phase2Iter,
            iterCount, totalIter,
            Progress::Phase::MIDS,
            targetLaplacianVar, progress, rng);

        // Phase 3: Joint refinement - REGIONAL loss if available
        int phase3Iter = static_cast<int>(totalIter * 0.25f);

        if (targetFeatures)
        {
            std::cerr << "\n[GEOS-DISPLAY] === Phase 3: Regional refinement (4x4 grid) ===" << std::endl;

            // Inline SPSA with regional loss for combined dials [3-7] and [10-16]
            // Using SAMPLED mode for speed (8 cells instead of 16)
            std::vector<int> displayDials;
            for (int i = DISPLAY_A_START; i < DISPLAY_A_START + DISPLAY_A_SIZE; i++)
                displayDials.push_back(i);
            for (int i = DISPLAY_B_START; i < DISPLAY_B_START + DISPLAY_B_SIZE; i++)
                displayDials.push_back(i);

            int numDials = static_cast<int>(displayDials.size());
            std::vector<float> delta(numDials);
            std::bernoulli_distribution coin(0.5);

            Theta bestTheta = theta;
            float bestLoss = evaluateLossRegional(body, *targetFeatures, LossMode::SAMPLED);
            int itersSinceImprovement = 0;

            std::cerr << "[REGIONAL] Start loss=" << bestLoss << " (sampled 8 cells)" << std::endl;

            for (int k = 0; k < phase3Iter && itersSinceImprovement < STALL_THRESHOLD; k++)
            {
                float a_k = computeA(k, BLOCK_7D);  // Use 7D params for 12 combined dials
                float c_k = computeC(k, BLOCK_7D);

                // Generate perturbation
                for (int i = 0; i < numDials; i++)
                    delta[i] = coin(rng) ? 1.0f : -1.0f;

                // Evaluate L+
                Theta thetaPlus = theta;
                for (int i = 0; i < numDials; i++)
                    thetaPlus[displayDials[i]] = std::clamp(theta[displayDials[i]] + c_k * delta[i], 0.0f, 1.0f);
                writeDials(link, thetaPlus);
                float lossPlus = evaluateLossRegional(body, *targetFeatures, LossMode::SAMPLED);

                // Evaluate L-
                Theta thetaMinus = theta;
                for (int i = 0; i < numDials; i++)
                    thetaMinus[displayDials[i]] = std::clamp(theta[displayDials[i]] - c_k * delta[i], 0.0f, 1.0f);
                writeDials(link, thetaMinus);
                float lossMinus = evaluateLossRegional(body, *targetFeatures, LossMode::SAMPLED);

                // Gradient estimate and update
                for (int i = 0; i < numDials; i++)
                {
                    float g_i = (lossPlus - lossMinus) / (2.0f * c_k * delta[i]);
                    theta[displayDials[i]] = std::clamp(theta[displayDials[i]] - a_k * g_i, 0.0f, 1.0f);
                }
                writeDials(link, theta);

                // Accumulate covariance sample
                g_spsaCovAccum.update(theta);

                float newLoss = evaluateLossRegional(body, *targetFeatures, LossMode::SAMPLED);

                // Track meaningful improvement
                if (newLoss < bestLoss)
                {
                    float improvement = bestLoss - newLoss;
                    float relativeImprovement = improvement / bestLoss;

                    if (relativeImprovement >= MIN_RELATIVE_IMPROVEMENT ||
                        improvement >= MIN_ABSOLUTE_IMPROVEMENT)
                    {
                        itersSinceImprovement = 0;
                        if (k % 20 == 0)
                            std::cerr << "[REGIONAL] k=" << k << " loss=" << newLoss << std::endl;
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

                iterCount++;
            }

            theta = bestTheta;
            writeDials(link, theta);
            std::cerr << "[REGIONAL] Final loss=" << bestLoss << std::endl;
        }
        else
        {
            // Fallback to global loss
            std::cerr << "\n[GEOS-DISPLAY] === Phase 3: Joint refinement [GLOBAL] ===" << std::endl;
            optimizeBlock(body, link, targetStyle, theta,
                DISPLAY_A_START, DISPLAY_A_SIZE,
                BLOCK_5D, phase3Iter / 2,
                iterCount, totalIter,
                Progress::Phase::TINY,
                targetLaplacianVar, progress, rng);
            optimizeBlock(body, link, targetStyle, theta,
                DISPLAY_B_START, DISPLAY_B_SIZE,
                BLOCK_7D, phase3Iter / 2,
                iterCount, totalIter,
                Progress::Phase::TINY,
                targetLaplacianVar, progress, rng);
        }

        // Phase 4: SelectiveColour (17-40) - skip if LUT active
        if (!lutEstimated)
        {
            int phase4Iter = static_cast<int>(totalIter * 0.20f);
            std::cerr << "\n[GEOS-DISPLAY] === Phase 4: SelectiveColour (24 dials) ===" << std::endl;
            optimizeBlock(body, link, targetStyle, theta,
                DISPLAY_C_START, DISPLAY_C_SIZE,
                BLOCK_24D, phase4Iter,
                iterCount, totalIter,
                Progress::Phase::TINY,
                targetLaplacianVar, progress, rng);
        }
        else
        {
            std::cerr << "\n[GEOS-DISPLAY] === Phase 4: SKIPPED (LUT active) ===" << std::endl;
        }

        float finalLoss = evaluateLoss(body, targetStyle);
        std::cerr << "\n[GEOS-DISPLAY] === FINAL ===" << std::endl;
        std::cerr << "[GEOS-DISPLAY] Final loss: " << finalLoss << std::endl;

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
        bool lutEstimated,
        const TargetFeatures* targetFeatures)
    {
        // Sanity checks
        float check1 = evaluateLoss(body, targetStyle);
        float check2 = evaluateLoss(body, targetStyle);
        std::cerr << "[SANITY] Same dials, two evals: " << check1 << " vs " << check2 << std::endl;

        if (std::abs(check1 - check2) > 0.001f)
        {
            std::cerr << "[WARNING] Non-deterministic loss evaluation detected!" << std::endl;
        }

        // Don't reset accumulator - accumulate across multiple calls
        // (LINEAR + DISPLAY in two-link architecture)
        // The accumulator is reset implicitly when the process starts

        int result = 0;

        // Dispatch based on mode
        if (config.geos_mode == Mode::FULL_35D)
        {
            result = optimizeFull41D(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
        else if (config.geos_mode == Mode::LINEAR_ONLY)
        {
            result = optimizeLinearOnly(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
        else if (config.geos_mode == Mode::SCENE_LINEAR)
        {
            result = optimizeSceneLinear(body, link, targetStyle, targetLaplacianVar, config, progress);
        }
        else if (config.geos_mode == Mode::DISPLAY)
        {
            result = optimizeDisplay(body, link, targetStyle, targetLaplacianVar, config, progress, lutEstimated, targetFeatures);
        }
        else
        {
            result = optimizeBlockwise(body, link, targetStyle, targetLaplacianVar, config, progress, lutEstimated);
        }

        // Save covariance if requested
        if (!config.aceo_save_cov.empty())
        {
            std::cerr << "[SPSA-COV] Accumulated " << g_spsaCovAccum.n << " samples" << std::endl;
            g_spsaCovAccum.saveToJson(config.aceo_save_cov);
        }

        return result;
    }

} // namespace geos::internal
