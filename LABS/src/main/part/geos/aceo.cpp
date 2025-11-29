// aceo.cpp
// ACEO: Adaptive Covariance Evolver Optimiser
//
// Key insight: The 36D dial space has only ~6 effective dimensions (95% variance).
// We exploit this by:
//   1. Eigendecompose the prior correlation matrix
//   2. Optimize in reduced eigenspace (6D instead of 36D)
//   3. Transform solutions back to dial space
//
// This dramatically reduces the search space complexity and allows
// the optimizer to follow the natural structure of the loss landscape.

#include "aceo.hpp"
#include "spsa.hpp"  // For readDials/writeDials/Theta
#include <random>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace geos::internal
{
    // ============================================================
    // Reduced dimensionality (captures 95% variance)
    // ============================================================
    constexpr int EIGEN_DIM = 8;  // Top 8 eigenvectors (99% variance)

    using Matrix36 = std::array<float, ACEO_DIAL_COUNT * ACEO_DIAL_COUNT>;
    using Vector36 = std::array<float, ACEO_DIAL_COUNT>;
    using VectorK = std::array<float, EIGEN_DIM>;
    using MatrixK = std::array<float, ACEO_DIAL_COUNT * EIGEN_DIM>;  // 36 x K projection matrix

    // ============================================================
    // Eigendecomposition using Jacobi method
    // ============================================================

    // Jacobi rotation for symmetric eigenvalue decomposition
    void jacobiRotate(Matrix36& A, Matrix36& V, int p, int q)
    {
        const int n = ACEO_DIAL_COUNT;
        float app = A[p * n + p];
        float aqq = A[q * n + q];
        float apq = A[p * n + q];

        if (std::abs(apq) < 1e-10f) return;

        float theta = (aqq - app) / (2.0f * apq);
        float t = (theta >= 0 ? 1.0f : -1.0f) / (std::abs(theta) + std::sqrt(theta * theta + 1.0f));
        float c = 1.0f / std::sqrt(t * t + 1.0f);
        float s = t * c;

        // Update A
        A[p * n + p] = app - t * apq;
        A[q * n + q] = aqq + t * apq;
        A[p * n + q] = A[q * n + p] = 0.0f;

        for (int j = 0; j < n; j++)
        {
            if (j != p && j != q)
            {
                float ajp = A[j * n + p];
                float ajq = A[j * n + q];
                A[j * n + p] = A[p * n + j] = c * ajp - s * ajq;
                A[j * n + q] = A[q * n + j] = s * ajp + c * ajq;
            }
        }

        // Update V (eigenvectors)
        for (int i = 0; i < n; i++)
        {
            float vip = V[i * n + p];
            float viq = V[i * n + q];
            V[i * n + p] = c * vip - s * viq;
            V[i * n + q] = s * vip + c * viq;
        }
    }

    // Full Jacobi eigendecomposition
    // Returns eigenvalues in diagonal of A, eigenvectors in columns of V
    void jacobi(Matrix36& A, Matrix36& V, int maxIter = 100)
    {
        const int n = ACEO_DIAL_COUNT;

        // Initialize V to identity
        V.fill(0.0f);
        for (int i = 0; i < n; i++)
            V[i * n + i] = 1.0f;

        for (int iter = 0; iter < maxIter; iter++)
        {
            // Find largest off-diagonal element
            float maxVal = 0.0f;
            int p = 0, q = 1;
            for (int i = 0; i < n; i++)
            {
                for (int j = i + 1; j < n; j++)
                {
                    if (std::abs(A[i * n + j]) > maxVal)
                    {
                        maxVal = std::abs(A[i * n + j]);
                        p = i;
                        q = j;
                    }
                }
            }

            if (maxVal < 1e-10f) break;  // Converged

            jacobiRotate(A, V, p, q);
        }
    }

    // ============================================================
    // Eigenspace state
    // ============================================================

    struct EigenSpace
    {
        std::array<float, ACEO_DIAL_COUNT> eigenvalues;      // All 36 eigenvalues (descending)
        std::array<int, ACEO_DIAL_COUNT> eigenOrder;         // Indices sorted by eigenvalue
        MatrixK projection;                                   // Top K eigenvectors (36 x K)
        Vector36 mean;                                        // Mean in dial space (0.5)
        bool valid = false;
    };

    bool computeEigenSpace(const Matrix36& correlation, EigenSpace& es)
    {
        // Copy correlation matrix (Jacobi modifies in-place)
        Matrix36 A = correlation;
        Matrix36 V;

        // Compute eigendecomposition
        jacobi(A, V, 200);

        // Extract eigenvalues from diagonal
        for (int i = 0; i < ACEO_DIAL_COUNT; i++)
            es.eigenvalues[i] = A[i * ACEO_DIAL_COUNT + i];

        // Sort indices by eigenvalue (descending)
        std::iota(es.eigenOrder.begin(), es.eigenOrder.end(), 0);
        std::sort(es.eigenOrder.begin(), es.eigenOrder.end(),
                  [&](int a, int b) { return es.eigenvalues[a] > es.eigenvalues[b]; });

        // Extract top K eigenvectors as projection matrix
        // projection[i][k] = V[i, eigenOrder[k]] (column k of V reordered)
        for (int i = 0; i < ACEO_DIAL_COUNT; i++)
        {
            for (int k = 0; k < EIGEN_DIM; k++)
            {
                int col = es.eigenOrder[k];
                es.projection[i * EIGEN_DIM + k] = V[i * ACEO_DIAL_COUNT + col];
            }
        }

        // Mean is neutral (0.5 for all dials)
        es.mean.fill(0.5f);
        es.valid = true;

        // Log eigenspace info
        float totalVar = 0.0f;
        for (int i = 0; i < ACEO_DIAL_COUNT; i++)
            totalVar += es.eigenvalues[i];

        float capturedVar = 0.0f;
        for (int k = 0; k < EIGEN_DIM; k++)
            capturedVar += es.eigenvalues[es.eigenOrder[k]];

        std::cerr << "[ACEO-EIGEN] Computed eigenspace: " << EIGEN_DIM << "D captures "
                  << (capturedVar / totalVar * 100.0f) << "% variance" << std::endl;

        return true;
    }

    // Project dial-space vector to eigenspace: z = P^T (x - mean)
    void projectToEigen(const EigenSpace& es, const Vector36& x, VectorK& z)
    {
        for (int k = 0; k < EIGEN_DIM; k++)
        {
            float sum = 0.0f;
            for (int i = 0; i < ACEO_DIAL_COUNT; i++)
            {
                sum += es.projection[i * EIGEN_DIM + k] * (x[i] - es.mean[i]);
            }
            z[k] = sum;
        }
    }

    // Unproject eigenspace vector to dial-space: x = mean + P z
    void unprojectFromEigen(const EigenSpace& es, const VectorK& z, Vector36& x)
    {
        for (int i = 0; i < ACEO_DIAL_COUNT; i++)
        {
            float sum = es.mean[i];
            for (int k = 0; k < EIGEN_DIM; k++)
            {
                sum += es.projection[i * EIGEN_DIM + k] * z[k];
            }
            x[i] = std::clamp(sum, 0.0f, 1.0f);
        }
    }

    // ============================================================
    // JSON parsing for prior covariance
    // ============================================================

    bool loadPriorCovariance(const std::string& path, Matrix36& matrix)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "[ACEO] Failed to open: " << path << std::endl;
            return false;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        // Find correlation_matrix array
        size_t pos = content.find("\"correlation_matrix\"");
        if (pos == std::string::npos)
        {
            std::cerr << "[ACEO] correlation_matrix not found in JSON" << std::endl;
            return false;
        }

        pos = content.find('[', pos);
        if (pos == std::string::npos) return false;
        pos++;

        int idx = 0;
        for (int row = 0; row < ACEO_DIAL_COUNT && idx < ACEO_DIAL_COUNT * ACEO_DIAL_COUNT; row++)
        {
            pos = content.find('[', pos);
            if (pos == std::string::npos) break;
            pos++;

            for (int col = 0; col < ACEO_DIAL_COUNT; col++)
            {
                while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n' || content[pos] == '\t'))
                    pos++;

                size_t end = pos;
                while (end < content.size() && (std::isdigit(content[end]) || content[end] == '.' ||
                       content[end] == '-' || content[end] == 'e' || content[end] == 'E' || content[end] == '+'))
                    end++;

                if (end > pos)
                    matrix[idx++] = std::stof(content.substr(pos, end - pos));

                pos = end;
                if (pos < content.size() && content[pos] == ',')
                    pos++;
            }

            pos = content.find(']', pos);
            if (pos != std::string::npos) pos++;
        }

        return idx == ACEO_DIAL_COUNT * ACEO_DIAL_COUNT;
    }

    // ============================================================
    // Dial conversion
    // ============================================================

    void aceoToTheta(const Vector36& aceo, Theta& theta)
    {
        theta.fill(0.5f);
        for (int i = 0; i < ACEO_DIAL_COUNT; i++)
            theta[ACEO_DIAL_MAP[i]] = aceo[i];
    }

    void thetaToAceo(const Theta& theta, Vector36& aceo)
    {
        for (int i = 0; i < ACEO_DIAL_COUNT; i++)
            aceo[i] = theta[ACEO_DIAL_MAP[i]];
    }

    // ============================================================
    // Eigenspace CMA-ES Optimizer
    // ============================================================

    int optimizeAceo(
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
        std::normal_distribution<float> normal(0.0f, 1.0f);

        // Load prior covariance
        Matrix36 priorCorr;
        std::string priorPath = "etc/aceo.json";
        if (!loadPriorCovariance(priorPath, priorCorr))
        {
            std::cerr << "[ACEO] Failed to load prior, falling back to SPSA" << std::endl;
            return optimizeGeos(body, link, targetStyle, targetLaplacianVar, config, progress, lutEstimated, targetFeatures);
        }

        // Compute eigenspace
        EigenSpace es;
        if (!computeEigenSpace(priorCorr, es))
        {
            std::cerr << "[ACEO] Eigendecomposition failed, falling back to SPSA" << std::endl;
            return optimizeGeos(body, link, targetStyle, targetLaplacianVar, config, progress, lutEstimated, targetFeatures);
        }

        // CMA-ES parameters for reduced space
        const int lambda = 4 + static_cast<int>(3 * std::log(EIGEN_DIM));  // ~10 for 8D
        const int mu = lambda / 2;
        float sigma = 0.3f;  // Step size in eigenspace

        // Eigenvalue-weighted step sizes (explore more along high-variance directions)
        VectorK eigenSteps;
        float maxEig = es.eigenvalues[es.eigenOrder[0]];
        for (int k = 0; k < EIGEN_DIM; k++)
        {
            float ev = es.eigenvalues[es.eigenOrder[k]];
            eigenSteps[k] = std::sqrt(ev / maxEig);  // Scale by sqrt of relative eigenvalue
        }

        // Recombination weights
        std::vector<float> weights(mu);
        float sumW = 0.0f;
        for (int i = 0; i < mu; i++)
        {
            weights[i] = std::log(static_cast<float>(mu + 0.5f)) - std::log(static_cast<float>(i + 1));
            sumW += weights[i];
        }
        for (int i = 0; i < mu; i++)
            weights[i] /= sumW;

        float muEff = 1.0f / std::inner_product(weights.begin(), weights.end(), weights.begin(), 0.0f);

        // Adaptation parameters
        float cc = (4.0f + muEff / EIGEN_DIM) / (EIGEN_DIM + 4.0f + 2.0f * muEff / EIGEN_DIM);
        float cs = (muEff + 2.0f) / (EIGEN_DIM + muEff + 5.0f);
        float c1 = 2.0f / ((EIGEN_DIM + 1.3f) * (EIGEN_DIM + 1.3f) + muEff);
        float cmu = std::min(1.0f - c1, 2.0f * (muEff - 2.0f + 1.0f / muEff) / ((EIGEN_DIM + 2.0f) * (EIGEN_DIM + 2.0f) + muEff));
        float damps = 1.0f + 2.0f * std::max(0.0f, std::sqrt((muEff - 1.0f) / (EIGEN_DIM + 1.0f)) - 1.0f) + cs;
        float chiN = std::sqrt(static_cast<float>(EIGEN_DIM)) * (1.0f - 1.0f / (4.0f * EIGEN_DIM) + 1.0f / (21.0f * EIGEN_DIM * EIGEN_DIM));

        // Initialize mean in eigenspace (project current dials)
        Theta startTheta;
        readDials(link, startTheta);
        Vector36 startAceo;
        thetaToAceo(startTheta, startAceo);

        VectorK mean;
        projectToEigen(es, startAceo, mean);

        // Evolution paths
        VectorK ps, pc;
        ps.fill(0.0f);
        pc.fill(0.0f);

        // Covariance matrix in eigenspace (start with identity * eigenvalue scaling)
        std::array<float, EIGEN_DIM * EIGEN_DIM> C;
        C.fill(0.0f);
        for (int k = 0; k < EIGEN_DIM; k++)
        {
            // Scale by eigenvalue for proper step sizes
            float ev = es.eigenvalues[es.eigenOrder[k]];
            C[k * EIGEN_DIM + k] = 1.0f;  // Identity - let sigma handle scaling
        }

        // Cholesky of C (initially identity)
        std::array<float, EIGEN_DIM * EIGEN_DIM> B;
        B = C;  // For identity, B = C

        // Evaluate initial loss
        float initialLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[ACEO-EIGEN] Initial loss: " << initialLoss << std::endl;
        std::cerr << "[ACEO-EIGEN] Population: λ=" << lambda << " μ=" << mu << std::endl;
        std::cerr << "[ACEO-EIGEN] Eigenspace dim: " << EIGEN_DIM << std::endl;

        VectorK bestMean = mean;
        float bestLoss = initialLoss;

        // Population storage
        std::vector<VectorK> population(lambda);
        std::vector<float> fitness(lambda);
        std::vector<int> ranking(lambda);

        int maxGen = config.geos_max_iter / lambda;
        int evalCount = 0;
        int itersSinceImprovement = 0;

        for (int gen = 0; gen < maxGen; gen++)
        {
            // Sample lambda offspring in eigenspace
            for (int i = 0; i < lambda; i++)
            {
                // Sample: x = mean + sigma * D * N(0,I) where D = diag(eigenSteps)
                for (int k = 0; k < EIGEN_DIM; k++)
                {
                    float z = normal(rng);
                    population[i][k] = mean[k] + sigma * eigenSteps[k] * z;
                }

                // Unproject to dial space
                Vector36 aceo;
                unprojectFromEigen(es, population[i], aceo);

                // Evaluate
                Theta theta;
                aceoToTheta(aceo, theta);
                writeDials(link, theta);
                fitness[i] = evaluateLoss(body, targetStyle);
                evalCount++;

                if (fitness[i] < bestLoss)
                {
                    float improvement = bestLoss - fitness[i];
                    if (improvement > 0.0001f)
                    {
                        itersSinceImprovement = 0;
                        if (gen % 5 == 0 || fitness[i] < 0.01f)
                        {
                            std::cerr << "[ACEO-EIGEN] gen=" << gen << " i=" << i
                                      << " loss=" << fitness[i] << std::endl;
                        }
                    }
                    bestLoss = fitness[i];
                    bestMean = population[i];
                }
            }

            // Sort by fitness
            std::iota(ranking.begin(), ranking.end(), 0);
            std::sort(ranking.begin(), ranking.end(),
                      [&](int a, int b) { return fitness[a] < fitness[b]; });

            // Update mean
            VectorK oldMean = mean;
            mean.fill(0.0f);
            for (int i = 0; i < mu; i++)
            {
                int idx = ranking[i];
                for (int k = 0; k < EIGEN_DIM; k++)
                    mean[k] += weights[i] * population[idx][k];
            }

            // Evolution path for sigma (ps)
            VectorK meanDiff;
            for (int k = 0; k < EIGEN_DIM; k++)
                meanDiff[k] = (mean[k] - oldMean[k]) / sigma;

            float psNorm = 0.0f;
            for (int k = 0; k < EIGEN_DIM; k++)
            {
                ps[k] = (1.0f - cs) * ps[k] + std::sqrt(cs * (2.0f - cs) * muEff) * meanDiff[k];
                psNorm += ps[k] * ps[k];
            }
            psNorm = std::sqrt(psNorm);

            // Step size adaptation (CSA)
            sigma *= std::exp((cs / damps) * (psNorm / chiN - 1.0f));
            sigma = std::clamp(sigma, 0.01f, 2.0f);

            itersSinceImprovement++;

            // Progress callback
            if (progress)
            {
                Progress p;
                p.stage = Progress::Stage::GEOS;
                p.phase = Progress::Phase::HUGE;
                p.iteration = evalCount;
                p.max_iterations = config.geos_max_iter;
                p.loss.spectral = bestLoss;

                if (!progress(p))
                    break;
            }

            // Early termination (use tighter threshold than config for deeper optimization)
            float targetThreshold = std::min(config.geos_threshold, 0.002f);  // 0.2%
            if (bestLoss < targetThreshold)
            {
                std::cerr << "[ACEO-EIGEN] Converged at gen=" << gen << " loss=" << bestLoss << std::endl;
                break;
            }

            if (itersSinceImprovement >= 30)
            {
                std::cerr << "[ACEO-EIGEN] Stalled at gen=" << gen << " loss=" << bestLoss << std::endl;
                break;
            }

            if (sigma < 0.005f)
            {
                std::cerr << "[ACEO-EIGEN] Sigma converged at gen=" << gen << " σ=" << sigma << std::endl;
                break;
            }
        }

        // Restore best solution
        Vector36 bestAceo;
        unprojectFromEigen(es, bestMean, bestAceo);
        Theta bestTheta;
        aceoToTheta(bestAceo, bestTheta);
        writeDials(link, bestTheta);

        float finalLoss = evaluateLoss(body, targetStyle);
        std::cerr << "[ACEO-EIGEN] Final loss: " << finalLoss << " (evals=" << evalCount << ")" << std::endl;

        return evalCount;
    }

} // namespace geos::internal
