# LABS TLDR

We have 45 dials in categories that can edit factors in an image:
- 5 fixed (exposure, temperature, tint, black_point, white_point)
- 36 color/tone (contrast, highlights, shadows, etc + selective color HSL)
- 4 detail (sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma)

These are based on the canonical set of image manipulations.

We have linear and display-referred processing modules.

The goal of LABS is to find how to tune an image by finding the tune parameters of the dials to make a RAW image look like a reference image. The parameters are called a vibe.

The tune files can then be run in the pipe on a RAW and quickly produce a final file.

GeoS provides the 45 dial factor hypercube independent variables (IVs) and the 10 feature hypersphere dependent variables (DVs). The objective function measures the angles of difference in the DVs using a cosine difference function.

GeoS needs an optimiser to reduce the angles of difference to optimal (as close to zero as possible on all DVs). We have used SPSA however it can't handle covariance and the dials have strong covariance. ACEO can manage covariance using the CMA-ES method. We use SPSA to create the initial covariance matrix, then ACEO to train the covariance on pictures.

The trained covariance matrix is stored in `etc/aceo.json` (currently 41 dials, needs upgrade to 45).

We are now in integration testing. We need tune to work properly so that it uses the pipe and produces pipe outputs. There is a test mode where we produce all pipe artefacts, then there is operating mode where it just produces the tail image.

All work should go into `LABS/tmp/var/tune` for visual review.
