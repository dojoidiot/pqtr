// diff.cpp
// Loss metric computation helpers for geos optimization

#include "diff.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>

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

        // Build 12D feature vector (was 10D, now includes mu_a, mu_b for color cast)
        features.v = {sigma1, sigma2, sigma3, mu_L, mu_C, std_L, std_C, skew_L, cov_LC, cov_HC, mu_a, mu_b};

        // Normalize to unit hypersphere
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

    float geodesicLoss(const StyleFeatures& a, const StyleFeatures& b)
    {
        float dot = 0.0f;
        for (int i = 0; i < STYLE_DIM; i++)
            dot += a.psi[i] * b.psi[i];

        return 1.0f - dot * dot;
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

        // Extract global features using BGR (full 12D with mu_a, mu_b)
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

        // Global loss (always computed) - use BGR for full 12D features
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

        // Global loss - use BGR for full 12D features
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
