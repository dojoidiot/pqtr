// task.cpp
// Main geos Task implementation
// Delegates to diff, spsa, and edge modules
//
// LUT-based luminance curve pre-pass:
// The Link now contains a LutCurve module that can estimate
// the tone curve from base->target and apply it automatically.

#include <geos.hpp>
#include "diff.hpp"
#include "spsa.hpp"
#include "aceo.hpp"
#include "edge.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>

namespace geos
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
            , m_targetStyle(extractStyleFromBGR(m_targetProxy))  // Use BGR for full 18D features (incl. percentiles)
            , m_targetLaplacianVar(laplacianVariance(m_targetProxy))
            , m_targetFeatures(extractTargetFeatures(targetImg))
        {
            targetImg.copyTo(m_targetImage);
            std::cerr << "[geos] Pre-computed regional features (4x4 grid)" << std::endl;
        }

        View target() override
        {
            return m_targetImage;
        }

        Data diff(View candidate) override
        {
            cv::UMat candProxy = resizeProxy(candidate);
            StyleFeatures candStyle = extractStyleFromBGR(candProxy);  // Use BGR for full 18D features
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
                    std::cout << "[geos] LUT curve estimated" << std::endl;
                }
            }

            // Initial loss measurement (with LUT applied)
            View candidate = body.view();
            result.loss = diff(candidate);

            // Stage 2: GEOS (Color/Tone) - fine-tune after LUT
            if (!config.skip_geos)
            {
                bool lutEstimated = link.lutCurve().isEstimated();

                // Dispatch based on optimizer selection
                // ACEO only works for modes with its 36 variable dials (DISPLAY, FULL_35D)
                // For SCENE_LINEAR (5 dials) and LINEAR_ONLY (partial), fall back to SPSA
                bool canUseAceo = (config.geos_mode == Mode::DISPLAY || config.geos_mode == Mode::FULL_35D);

                // Pass nullptr for targetFeatures if skip_regional is set
                const TargetFeatures* features = config.skip_regional ? nullptr : &m_targetFeatures;

                if (config.optimizer == Optimizer::HYBRID && canUseAceo)
                {
                    // HYBRID: ACEO for direction (pop), then SPSA for polish
                    std::cout << "[geos] HYBRID mode: ACEO for pop, SPSA for polish" << std::endl;

                    // Phase 1: ACEO - get to correct pop (fewer iterations)
                    Config aceoConfig = config;
                    aceoConfig.geos_max_iter = config.geos_max_iter / 2;  // Half iterations for ACEO
                    int aceoIters = optimizeAceo(
                        body, link, m_targetStyle, m_targetLaplacianVar, aceoConfig, progress, lutEstimated, features);

                    // Phase 2: SPSA - polish from ACEO's position (remaining iterations)
                    Config spsaConfig = config;
                    spsaConfig.geos_max_iter = config.geos_max_iter - aceoIters;  // Remaining budget
                    int spsaIters = optimizeGeos(
                        body, link, m_targetStyle, m_targetLaplacianVar, spsaConfig, progress, lutEstimated, features);

                    result.geos_iterations = aceoIters + spsaIters;
                    std::cout << "[geos] HYBRID complete: " << aceoIters << " ACEO + " << spsaIters << " SPSA" << std::endl;
                }
                else if (config.optimizer == Optimizer::ACEO && canUseAceo)
                {
                    result.geos_iterations = optimizeAceo(
                        body, link, m_targetStyle, m_targetLaplacianVar, config, progress, lutEstimated, features);
                }
                else
                {
                    // Default: SPSA - Pass regional features for DISPLAY mode (unless skipped)
                    result.geos_iterations = optimizeGeos(
                        body, link, m_targetStyle, m_targetLaplacianVar, config, progress, lutEstimated, features);
                }
            }

            // Stage 3: Edge (Sharpness)
            // Skip separate edge pass in FULL_35D mode - edge dials are optimized holistically
            bool holisticMode = (config.geos_mode == Mode::FULL_35D);
            if (!config.skip_edge && !holisticMode)
            {
                result.edge_evaluations = optimizeEdge(
                    body, link, m_targetLaplacianVar, config, progress);
            }
            else if (holisticMode)
            {
                std::cout << "[geos] Edge dials optimized holistically (no separate pass)" << std::endl;
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
        TargetFeatures m_targetFeatures;  // Regional features (4x4 grid)
    };

    // ============================================================
    // Factory
    // ============================================================

    pqtr::Hold<Task> make(View target)
    {
        return pqtr::Hold<Task>(new TaskImpl(target));
    }

} // namespace geos
