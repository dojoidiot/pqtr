// diff.cpp
// Loss metric computation helpers for tune optimization

#include "diff.hpp"
#include <opencv2/imgproc.hpp>
#include <cmath>

namespace tune::internal
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

    cv::UMat convertToSafeLCH(const cv::UMat& bgr)
    {
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

        cv::UMat result;
        lch.copyTo(result);
        return result;
    }

    StyleFeatures extractStyle(const cv::UMat& lch)
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

        // Build feature vector
        features.v = {sigma1, sigma2, sigma3, mu_L, mu_C, std_L, std_C, skew_L, cov_LC, cov_HC};

        // Normalize to unit hypersphere
        float norm = 0.0f;
        for (float f : features.v)
            norm += f * f;
        norm = std::sqrt(norm);

        if (norm > 1e-6f)
        {
            for (int i = 0; i < 10; i++)
                features.psi[i] = features.v[i] / norm;
        }
        else
        {
            features.psi.fill(0.0f);
            features.psi[0] = 1.0f;
        }

        return features;
    }

    float geodesicLoss(const StyleFeatures& a, const StyleFeatures& b)
    {
        float dot = 0.0f;
        for (int i = 0; i < 10; i++)
            dot += a.psi[i] * b.psi[i];

        return 1.0f - dot * dot;
    }

    std::pair<float, float> computeDome(const StyleFeatures& target, const StyleFeatures& candidate)
    {
        // Dot product
        float dot = 0.0f;
        for (int i = 0; i < 10; i++)
            dot += target.psi[i] * candidate.psi[i];

        // Radial distance
        float r = std::sqrt(1.0f - dot * dot);

        // Residual in tangent plane
        std::array<float, 10> residual;
        for (int i = 0; i < 10; i++)
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

} // namespace tune::internal
