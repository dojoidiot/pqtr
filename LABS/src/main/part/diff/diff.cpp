// diff.cpp
// Implementation of perceptual difference metrics with caching
//
// TaskImpl caches base image features (style vector, laplacian variance)
// for efficient repeated comparisons during tune optimization.

#include <diff.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <array>

namespace diff
{
    // Constants
    constexpr int PROXY_SIZE = 512;
    constexpr float CHROMA_WEIGHT_K = 0.1f; // tanh scaling for safe hue

    // ============================================================
    // Internal helpers
    // ============================================================

    namespace internal
    {
        // Resize image to proxy size for efficient processing
        cv::UMat resizeProxy(const cv::UMat& image, int size = PROXY_SIZE)
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

        // Convert BGR 8-bit to LCH float with chroma-weighted hue
        // Returns 3-channel float: [L, C, H_safe]
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

        // Style feature vector (10 dimensions)
        struct StyleFeatures
        {
            std::array<float, 10> v;   // Feature vector
            std::array<float, 10> psi; // Normalized (unit hypersphere)
        };

        // Extract style features from LCH image
        StyleFeatures extractStyle(const cv::UMat& lch)
        {
            StyleFeatures features;

            cv::Mat lchMat;
            lch.copyTo(lchMat);

            // Reshape to N x 3 matrix for SVD
            cv::Mat data = lchMat.reshape(1, lchMat.rows * lchMat.cols);

            // SVD for singular values
            cv::Mat w; // singular values
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
            cv::Scalar meanL, stdL, meanC, stdC, meanH, stdH;
            cv::meanStdDev(L, meanL, stdL);
            cv::meanStdDev(C, meanC, stdC);

            float mu_L = static_cast<float>(meanL[0]);
            float mu_C = static_cast<float>(meanC[0]);
            float std_L = static_cast<float>(stdL[0]);
            float std_C = static_cast<float>(stdC[0]);

            // Skewness of L (simplified: mean of (x - mean)^3 / std^3)
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
                features.psi[0] = 1.0f; // Default to unit vector
            }

            return features;
        }

        // Geodesic loss: 1 - |<a|b>|^2
        float geodesicLoss(const StyleFeatures& a, const StyleFeatures& b)
        {
            float dot = 0.0f;
            for (int i = 0; i < 10; i++)
                dot += a.psi[i] * b.psi[i];

            return 1.0f - dot * dot;
        }

        // Laplacian variance for sharpness measurement
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

            // Variance = stddev^2
            return static_cast<float>(stddev[0] * stddev[0]);
        }

    } // namespace internal

    // ============================================================
    // TaskImpl - caches base image features
    // ============================================================

    class TaskImpl : public Task
    {
    public:
        explicit TaskImpl(View base)
            : m_baseProxy(internal::resizeProxy(base))
            , m_baseLCH(internal::convertToSafeLCH(m_baseProxy))
            , m_baseStyle(internal::extractStyle(m_baseLCH))
            , m_baseLaplacianVar(internal::laplacianVariance(m_baseProxy))
        {
            base.copyTo(m_baseImage); // Keep full res for visual diff
        }

        View base() override
        {
            return m_baseImage;
        }

        Data diff(View test) override
        {
            cv::UMat testProxy = internal::resizeProxy(test);
            cv::UMat testLCH = internal::convertToSafeLCH(testProxy);
            internal::StyleFeatures testStyle = internal::extractStyle(testLCH);

            float testLaplacianVar = internal::laplacianVariance(testProxy);

            Data result;
            result.spectral = internal::geodesicLoss(m_baseStyle, testStyle);

            if (m_baseLaplacianVar < 1e-6f)
                result.frequency = (testLaplacianVar < 1e-6f) ? 0.0f : 1.0f;
            else
                result.frequency = std::abs(testLaplacianVar - m_baseLaplacianVar) / m_baseLaplacianVar;

            return result;
        }

        View view(View test, float scale) override
        {
            // Ensure same size
            cv::UMat cand, ref;
            if (test.size() != m_baseImage.size())
            {
                cv::resize(test, cand, m_baseImage.size());
                m_baseImage.copyTo(ref);
            }
            else
            {
                test.copyTo(cand);
                m_baseImage.copyTo(ref);
            }

            // Convert to float
            cv::UMat candF, refF;
            cand.convertTo(candF, CV_32F);
            ref.convertTo(refF, CV_32F);

            // Absolute difference
            cv::UMat diffImg;
            cv::absdiff(candF, refF, diffImg);

            // Scale
            cv::UMat scaled;
            cv::multiply(diffImg, cv::Scalar(scale, scale, scale), scaled);

            // Clamp to 8-bit
            cv::UMat result;
            scaled.convertTo(result, CV_8U);

            return result;
        }

    private:
        cv::UMat m_baseImage;                    // Full resolution for visual diff
        cv::UMat m_baseProxy;                    // 512x512 proxy
        cv::UMat m_baseLCH;                      // LCH conversion (cached)
        internal::StyleFeatures m_baseStyle;    // Style features (cached)
        float m_baseLaplacianVar;               // Laplacian variance (cached)
    };

    // ============================================================
    // Factory
    // ============================================================

    pqtr::Hold<Task> make(View base)
    {
        return pqtr::Hold<Task>(new TaskImpl(base));
    }

} // namespace diff
