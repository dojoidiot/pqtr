// diff.cpp
// Loss metric computation helpers for geos optimization

#include "diff.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <cstdio>

namespace geos::internal
{

    cv::UMat resizeProxy(const cv::UMat& image, int size)
    {
        cv::UMat proxy;
        int maxDim = std::max(image.cols, image.rows);
        if (maxDim <= size)
        {
            image.copyTo(proxy);
        }
        else
        {
            float scale = static_cast<float>(size) / maxDim;
            cv::resize(image, proxy, cv::Size(), scale, scale, cv::INTER_AREA);
        }
        return proxy;
    }

    // Internal: convert BGR to LCH and also return Lab a/b means for color cast
    struct LCHResult
    {
        cv::UMat lch;
        float mu_a;  // Mean Lab a* (green-magenta axis), signed
        float mu_b;  // Mean Lab b* (blue-yellow axis), signed
    };

    LCHResult convertToSafeLCH_impl(const cv::UMat& bgr)
    {
        LCHResult result;

        cv::UMat lab;
        cv::cvtColor(bgr, lab, cv::COLOR_BGR2Lab);

        cv::Mat labMat;
        lab.copyTo(labMat);
        labMat.convertTo(labMat, CV_32F);

        // Split into L, a, b
        std::vector<cv::Mat> channels(3);
        cv::split(labMat, channels);

        cv::Mat L = channels[0]; // [0, 255] in OpenCV Lab
        cv::Mat a = channels[1]; // [0, 255], 128 = neutral
        cv::Mat b = channels[2]; // [0, 255], 128 = neutral

        // Shift a, b to signed range
        a -= 128.0f;
        b -= 128.0f;

        // Capture Lab a/b means BEFORE any further transformation
        // Normalize to [-1, 1] range (Lab a/b range is roughly [-128, 127])
        result.mu_a = static_cast<float>(cv::mean(a)[0]) / 128.0f;
        result.mu_b = static_cast<float>(cv::mean(b)[0]) / 128.0f;

        // Compute Chroma and Hue
        cv::Mat C, H;
        cv::magnitude(a, b, C);
        cv::phase(a, b, H, true); // degrees

        // Apply chroma weighting to hue: H_safe = H * tanh(k * C)
        cv::Mat scaledC = C * CHROMA_WEIGHT_K;
        cv::Mat weight = cv::Mat(scaledC.size(), CV_32F);
        for (int i = 0; i < scaledC.rows; i++)
        {
            float* wPtr = weight.ptr<float>(i);
            const float* cPtr = scaledC.ptr<float>(i);
            for (int j = 0; j < scaledC.cols; j++)
            {
                wPtr[j] = std::tanh(cPtr[j]);
            }
        }

        cv::Mat H_safe;
        cv::multiply(H, weight, H_safe);

        // Normalize L to [0, 1]
        L /= 255.0f;
        // Normalize C (max ~180 in Lab)
        C /= 180.0f;
        // Normalize H_safe (max 360 degrees, but weighted down)
        H_safe /= 360.0f;

        // Merge to LCH
        std::vector<cv::Mat> lchChannels = {L, C, H_safe};
        cv::Mat lch;
        cv::merge(lchChannels, lch);

        lch.copyTo(result.lch);
        return result;
    }

    cv::UMat convertToSafeLCH(const cv::UMat& bgr)
    {
        return convertToSafeLCH_impl(bgr).lch;
    }

    // Helper: compute percentile from a flattened cv::Mat
    static float computePercentile(const cv::Mat& data, float p)
    {
        cv::Mat flat = data.reshape(1, data.total());
        cv::Mat sorted;
        cv::sort(flat, sorted, cv::SORT_ASCENDING);

        int idx = static_cast<int>(p * (sorted.total() - 1));
        idx = std::max(0, std::min(idx, static_cast<int>(sorted.total() - 1)));
        return sorted.at<float>(idx);
    }

    // Internal helper: extract style from LCH with pre-computed Lab a/b means
    StyleFeatures extractStyleImpl(const cv::UMat& lch, float mu_a, float mu_b)
    {
        StyleFeatures features;

        cv::Mat lchMat;
        lch.copyTo(lchMat);

        // Reshape to N x 3 matrix for SVD
        cv::Mat data = lchMat.reshape(1, lchMat.rows * lchMat.cols);

        // SVD for singular values
        cv::Mat w;
        cv::SVD::compute(data, w, cv::noArray(), cv::noArray(), cv::SVD::NO_UV);

        // Normalize singular values by sqrt(N) for scale invariance
        float sqrtN = std::sqrt(static_cast<float>(data.rows));
        float sigma1 = w.at<float>(0) / sqrtN;
        float sigma2 = w.at<float>(1) / sqrtN;
        float sigma3 = w.at<float>(2) / sqrtN;

        // Split channels
        std::vector<cv::Mat> channels(3);
        cv::split(lchMat, channels);
        cv::Mat L = channels[0];
        cv::Mat C = channels[1];
        cv::Mat H = channels[2];

        // Statistics
        cv::Scalar meanL, stdL, meanC, stdC;
        cv::meanStdDev(L, meanL, stdL);
        cv::meanStdDev(C, meanC, stdC);

        float mu_L = static_cast<float>(meanL[0]);
        float mu_C = static_cast<float>(meanC[0]);
        float std_L = static_cast<float>(stdL[0]);
        float std_C = static_cast<float>(stdC[0]);

        // Skewness of L
        cv::Mat L_centered = L - mu_L;
        cv::Mat L_cubed;
        cv::pow(L_centered, 3, L_cubed);
        float skew_L = static_cast<float>(cv::mean(L_cubed)[0]);
        if (std_L > 1e-6f)
            skew_L /= (std_L * std_L * std_L);

        // Covariances
        cv::Mat LC_prod, HC_prod;
        cv::multiply(L - mu_L, C - mu_C, LC_prod);
        float cov_LC = static_cast<float>(cv::mean(LC_prod)[0]);

        cv::Scalar meanH_sc = cv::mean(H);
        float mu_H = static_cast<float>(meanH_sc[0]);
        cv::multiply(H - mu_H, C - mu_C, HC_prod);
        float cov_HC = static_cast<float>(cv::mean(HC_prod)[0]);

        // Histogram percentiles for tone curve shape
        float L_p10 = computePercentile(L, 0.10f);
        float L_p25 = computePercentile(L, 0.25f);
        float L_p75 = computePercentile(L, 0.75f);
        float L_p90 = computePercentile(L, 0.90f);
        float C_p50 = computePercentile(C, 0.50f);
        float C_p90 = computePercentile(C, 0.90f);

        // Shadow chroma: mean chroma where L < L_p25 (dark pixels)
        // This directly measures color preservation in shadows
        float C_shadow = 0.0f;
        int shadowCount = 0;
        for (int row = 0; row < L.rows; row++)
        {
            const float* lPtr = L.ptr<float>(row);
            const float* cPtr = C.ptr<float>(row);
            for (int col = 0; col < L.cols; col++)
            {
                if (lPtr[col] < L_p25)
                {
                    C_shadow += cPtr[col];
                    shadowCount++;
                }
            }
        }
        if (shadowCount > 0)
            C_shadow /= shadowCount;

        // Build 19D feature vector
        features.v = {
            sigma1, sigma2, sigma3,           // [0-2]  SVD
            mu_L, mu_C,                       // [3-4]  means
            std_L, std_C,                     // [5-6]  stds
            skew_L,                           // [7]    skewness
            cov_LC, cov_HC,                   // [8-9]  covariances
            mu_a, mu_b,                       // [10-11] color cast
            L_p10, L_p25, L_p75, L_p90,       // [12-15] L percentiles (tone curve)
            C_p50, C_p90,                     // [16-17] C percentiles (saturation)
            C_shadow                          // [18]   shadow chroma
        };

        // Normalize to unit hypersphere (kept for backward compat, not used in loss)
        float norm = 0.0f;
        for (float f : features.v)
            norm += f * f;
        norm = std::sqrt(norm);

        if (norm > 1e-6f)
        {
            for (int i = 0; i < STYLE_DIM; i++)
                features.psi[i] = features.v[i] / norm;
        }
        else
        {
            features.psi.fill(0.0f);
            features.psi[0] = 1.0f;
        }

        return features;
    }

    // Legacy API: extract style from pre-converted LCH (mu_a=0, mu_b=0)
    // Note: This doesn't capture color cast - use extractStyleFromBGR for full features
    StyleFeatures extractStyle(const cv::UMat& lch)
    {
        return extractStyleImpl(lch, 0.0f, 0.0f);
    }

    // New API: extract style directly from BGR, capturing Lab a/b means for color cast
    StyleFeatures extractStyleFromBGR(const cv::UMat& bgr)
    {
        LCHResult lchResult = convertToSafeLCH_impl(bgr);
        return extractStyleImpl(lchResult.lch, lchResult.mu_a, lchResult.mu_b);
    }

    // Feature names for diagnostic output
    const char* FEATURE_NAMES[STYLE_DIM] = {
        "sigma1", "sigma2", "sigma3",  // [0-2]
        "mu_L", "mu_C",                // [3-4]
        "std_L", "std_C",              // [5-6]
        "skew_L",                      // [7]
        "cov_LC", "cov_HC",            // [8-9]
        "mu_a", "mu_b",                // [10-11]
        "L_p10", "L_p25", "L_p75", "L_p90",  // [12-15]
        "C_p50", "C_p90",              // [16-17]
        "C_shadow"                     // [18]
    };

    // Diagnostic: compute per-feature errors (unweighted absolute difference)
    std::array<float, STYLE_DIM> perFeatureError(const StyleFeatures& target, const StyleFeatures& candidate)
    {
        std::array<float, STYLE_DIM> errors;
        for (int i = 0; i < STYLE_DIM; i++)
        {
            errors[i] = std::abs(target.v[i] - candidate.v[i]);
        }
        return errors;
    }

    // Diagnostic: print per-feature analysis
    void printFeatureAnalysis(const StyleFeatures& target, const StyleFeatures& candidate)
    {
        auto errors = perFeatureError(target, candidate);
        float totalWeightedLoss = 0.0f;
        float totalWeight = 0.0f;

        std::cout << "\n[FEATURE ANALYSIS]\n";
        std::cout << "Feature        Target    Output    Error     Weight   Contrib\n";
        std::cout << "--------------------------------------------------------------\n";

        for (int i = 0; i < STYLE_DIM; i++)
        {
            float w = FEATURE_WEIGHTS[i];
            float contrib = w * errors[i] * errors[i];
            totalWeightedLoss += contrib;
            totalWeight += w;

            printf("%-12s   %7.4f   %7.4f   %7.4f   %5.2f    %7.4f\n",
                   FEATURE_NAMES[i],
                   target.v[i],
                   candidate.v[i],
                   errors[i],
                   w,
                   contrib);
        }

        float avgLoss = std::sqrt(totalWeightedLoss / totalWeight);
        std::cout << "--------------------------------------------------------------\n";
        printf("Weighted L2 loss: %.4f (%.2f%%)\n\n", avgLoss, avgLoss * 100.0f);
    }

    // Weighted L2 loss on raw features (replaces geodesic)
    // This preserves magnitude information that distinguishes flat from punchy
    float geodesicLoss(const StyleFeatures& a, const StyleFeatures& b)
    {
        float sumWeightedSq = 0.0f;
        float sumWeights = 0.0f;

        for (int i = 0; i < STYLE_DIM; i++)
        {
            float diff = a.v[i] - b.v[i];
            float w = FEATURE_WEIGHTS[i];
            sumWeightedSq += w * diff * diff;
            sumWeights += w;
        }

        // Normalize by sum of weights and return sqrt for L2 distance
        // Scale to roughly same range as old geodesic [0, 1]
        float loss = std::sqrt(sumWeightedSq / sumWeights);

        // Clamp to [0, 1] for consistency with old API
        return std::min(1.0f, loss);
    }

    std::pair<float, float> computeDome(const StyleFeatures& target, const StyleFeatures& candidate)
    {
        // Dot product
        float dot = 0.0f;
        for (int i = 0; i < STYLE_DIM; i++)
            dot += target.psi[i] * candidate.psi[i];

        // Radial distance
        float r = std::sqrt(1.0f - dot * dot);

        // Residual in tangent plane
        std::array<float, STYLE_DIM> residual;
        for (int i = 0; i < STYLE_DIM; i++)
            residual[i] = candidate.psi[i] - dot * target.psi[i];

        // Project onto semantic axes
        float x = residual[IDX_MU_L];
        float y = residual[IDX_MU_C];
        float theta = std::atan2(y, x);

        if (theta < 0)
            theta += 2.0f * static_cast<float>(M_PI);

        return {r, theta};
    }

    float laplacianVariance(const cv::UMat& image)
    {
        cv::UMat gray;
        if (image.channels() == 3)
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        else
            image.copyTo(gray);

        cv::UMat laplacian;
        cv::Laplacian(gray, laplacian, CV_64F);

        cv::Scalar mean, stddev;
        cv::meanStdDev(laplacian, mean, stddev);

        return static_cast<float>(stddev[0] * stddev[0]);
    }

    // ============================================================
    // Regional loss computation (Phase 2)
    // ============================================================

    StyleFeatures extractCellFeatures(const cv::UMat& lch, int cellIdx)
    {
        int row = cellIdx / GRID_SIZE;
        int col = cellIdx % GRID_SIZE;

        int cellH = lch.rows / GRID_SIZE;
        int cellW = lch.cols / GRID_SIZE;

        cv::Rect roi(col * cellW, row * cellH, cellW, cellH);
        cv::UMat cell = lch(roi);

        return extractStyle(cell);
    }

    TargetFeatures extractTargetFeatures(const cv::UMat& target)
    {
        TargetFeatures tf;

        // Resize to proxy and convert to LCH
        cv::UMat proxy = resizeProxy(target);
        tf.lch = convertToSafeLCH(proxy);

        // Extract global features using BGR (full 18D with percentiles)
        tf.global = extractStyleFromBGR(proxy);

        // Extract regional features (4x4 grid = 16 cells)
        // Note: Regional features use LCH (won't have mu_a/mu_b - they'll be 0)
        // This is acceptable since global features dominate the loss
        for (int i = 0; i < GRID_CELLS; i++)
        {
            tf.regions[i] = extractCellFeatures(tf.lch, i);
        }

        return tf;
    }

    float computeProgressiveLoss(
        const cv::UMat& candidate,
        const TargetFeatures& target,
        LossMode mode,
        float globalWeight)
    {
        // Resize candidate to proxy
        cv::UMat candProxy = resizeProxy(candidate);

        // Global loss (always computed) - use BGR for full 18D features
        StyleFeatures candGlobal = extractStyleFromBGR(candProxy);
        float globalLoss = geodesicLoss(target.global, candGlobal);

        // GLOBAL_ONLY mode: return just global loss
        if (mode == LossMode::GLOBAL_ONLY)
        {
            return globalLoss;
        }

        // Convert to LCH for regional analysis
        cv::UMat candLCH = convertToSafeLCH(candProxy);

        // Compute regional losses
        float localSum = 0.0f;
        int numCells = 0;

        if (mode == LossMode::SAMPLED)
        {
            // Sample 8 cells: corners + center quad
            for (int idx : SAMPLED_CELLS)
            {
                StyleFeatures candCell = extractCellFeatures(candLCH, idx);
                localSum += geodesicLoss(target.regions[idx], candCell);
                numCells++;
            }
        }
        else // FULL_REGIONAL
        {
            // All 16 cells
            for (int i = 0; i < GRID_CELLS; i++)
            {
                StyleFeatures candCell = extractCellFeatures(candLCH, i);
                localSum += geodesicLoss(target.regions[i], candCell);
                numCells++;
            }
        }

        float localMean = localSum / static_cast<float>(numCells);

        // Combined loss: weighted average of global and local
        return globalWeight * globalLoss + (1.0f - globalWeight) * localMean;
    }

    RegionalAnalysis computeRegionalAnalysis(
        const cv::UMat& candidate,
        const TargetFeatures& target)
    {
        RegionalAnalysis ra;

        // Resize candidate to proxy
        cv::UMat candProxy = resizeProxy(candidate);

        // Global loss - use BGR for full 18D features
        StyleFeatures candGlobal = extractStyleFromBGR(candProxy);
        ra.global = geodesicLoss(target.global, candGlobal);

        // Convert to LCH for regional analysis
        cv::UMat candLCH = convertToSafeLCH(candProxy);

        // All 16 regional losses
        for (int i = 0; i < GRID_CELLS; i++)
        {
            StyleFeatures candCell = extractCellFeatures(candLCH, i);
            ra.local[i] = geodesicLoss(target.regions[i], candCell);
        }

        return ra;
    }

} // namespace geos::internal
