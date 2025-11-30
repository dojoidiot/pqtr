# LABS TLDR

We have 45 dials in categories that can edit factors in an image:
- 5 fixed (exposure, temperature, tint, black_point, white_point)
- 36 color/tone (contrast, highlights, shadows, etc + selective color HSL)
- 4 detail (sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma)

These are based on the canonical set of image manipulations.

We have linear and display-referred processing modules.

The goal of LABS is to find how to tune an image by finding the tune parameters of the dials to make a RAW image look like a reference image. The parameters are called a vibe.

The tune files can then be run in the pipe on a RAW and quickly produce a final file.

GeoS provides the 45 dial factor hypercube independent variables (IVs) and the 12 feature hypersphere dependent variables (DVs). The objective function measures the angles of difference in the DVs using a cosine difference function.

The 12D feature vector includes:
- SVD singular values (σ₁, σ₂, σ₃) - energy distribution
- LCH means (μ_L, μ_C) - brightness and chroma
- LCH stds (std_L, std_C) - contrast and saturation spread
- Luminance skewness (skew_L) - high-key vs low-key
- Covariances (cov_LC, cov_HC) - color harmony
- **Lab a/b means (μ_a, μ_b)** - color cast penalty (green-magenta, blue-yellow)

GeoS needs an optimiser to reduce the angles of difference to optimal (as close to zero as possible on all DVs). We have three optimizers:

1. **SPSA** - Phased optimization, builds covariance matrix
2. **ACEO** - CMA-ES eigenspace optimization using prior covariance
3. **HYBRID** - ACEO for direction/pop, then SPSA for polish

The trained covariance matrix is stored in `etc/aceo_full.json` (45 dials, holistic optimization including edge).

Current status: Testing hybrid mode and 12D features.

All work should go into `tmp/` for visual review.
