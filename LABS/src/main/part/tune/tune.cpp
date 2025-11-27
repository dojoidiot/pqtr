// tune.cpp
// Main tune Task implementation
// Delegates to diff, geos, and edge modules
//
// LUT-based luminance curve pre-pass:
// The Link now contains a LutCurve module that can estimate
// the tone curve from base->target and apply it automatically.

#include <tune.hpp>
#include "diff.hpp"
#include "geos.hpp"
#include "edge.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>

namespace tune
{
    using namespace internal;

    // ============================================================
    // TaskImpl - caches target image features
    // ============================================================

    class TaskImpl : public Task
    {
    public:
        explicit TaskImpl(View targetImg)
            : m_targetProxy(resizeProxy(targetImg))
            , m_targetLCH(convertToSafeLCH(m_targetProxy))
            , m_targetStyle(extractStyle(m_targetLCH))
            , m_targetLaplacianVar(laplacianVariance(m_targetProxy))
        {
            targetImg.copyTo(m_targetImage);
        }

        View target() override
        {
            return m_targetImage;
        }

        Data diff(View candidate) override
        {
            cv::UMat candProxy = resizeProxy(candidate);
            cv::UMat candLCH = convertToSafeLCH(candProxy);
            StyleFeatures candStyle = extractStyle(candLCH);
            float candLaplacianVar = laplacianVariance(candProxy);

            Data result;
            result.spectral = geodesicLoss(m_targetStyle, candStyle);

            if (m_targetLaplacianVar < 1e-6f)
                result.frequency = (candLaplacianVar < 1e-6f) ? 0.0f : 1.0f;
            else
                result.frequency = std::abs(candLaplacianVar - m_targetLaplacianVar) / m_targetLaplacianVar;

            return result;
        }

        View view(View candidate, float scale) override
        {
            cv::UMat cand, ref;
            if (candidate.size() != m_targetImage.size())
            {
                cv::resize(candidate, cand, m_targetImage.size());
                m_targetImage.copyTo(ref);
            }
            else
            {
                candidate.copyTo(cand);
                m_targetImage.copyTo(ref);
            }

            cv::UMat candF, refF;
            cand.convertTo(candF, CV_32F);
            ref.convertTo(refF, CV_32F);

            cv::UMat diffImg;
            cv::absdiff(candF, refF, diffImg);

            cv::UMat scaled;
            cv::multiply(diffImg, cv::Scalar(scale, scale, scale), scaled);

            cv::UMat result;
            scaled.convertTo(result, CV_8U);

            return result;
        }

        Result run(pipe::Body& body, pipe::Body::Link& link,
                  const Config& config,
                  Callback progress) override
        {
            Result result;
            result.geos_iterations = 0;
            result.edge_evaluations = 0;

            // Stage 1: LUT Curve Estimation (from raw base to target)
            // Estimate per-channel curves before any dial adjustments
            // Skip if config.skip_lut is true (for true linear-only baseline)
            if (!config.skip_lut && !link.lutCurve().isEstimated())
            {
                View baseView = body.view();
                if (link.lutCurve().estimate(baseView, m_targetImage))
                {
                    std::cout << "[tune] LUT curve estimated" << std::endl;
                }
            }

            // Initial loss measurement (with LUT applied)
            View candidate = body.view();
            result.loss = diff(candidate);

            // Stage 2: GEOS (Color/Tone) - fine-tune after LUT
            if (!config.skip_geos)
            {
                bool lutEstimated = link.lutCurve().isEstimated();
                result.geos_iterations = optimizeGeos(
                    body, link, m_targetStyle, m_targetLaplacianVar, config, progress, lutEstimated);
            }

            // Stage 3: Edge (Sharpness)
            if (!config.skip_edge)
            {
                result.edge_evaluations = optimizeEdge(
                    body, link, m_targetLaplacianVar, config, progress);
            }

            // Final loss measurement
            candidate = body.view();
            result.loss = diff(candidate);

            return result;
        }

    private:
        cv::UMat m_targetImage;
        cv::UMat m_targetProxy;
        cv::UMat m_targetLCH;
        StyleFeatures m_targetStyle;
        float m_targetLaplacianVar;
    };

    // ============================================================
    // Factory
    // ============================================================

    pqtr::Hold<Task> make(View target)
    {
        return pqtr::Hold<Task>(new TaskImpl(target));
    }

} // namespace tune
