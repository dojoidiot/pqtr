# POPS (Pipeline Of Plugins System) Architecture

The POPS model is a canonical, 5-stage image processing pipeline designed to transform scene-linear RAW data into a final, display-referred image that mimics a target look (e.g., a camera's embedded JPEG).

The architecture is based on the modular design of professional photo editing software like Darktable, where different aspects of image correction are separated into a logical sequence of expert modules. Each module in the POPS pipeline is a self-contained plugin with a `learn` and `apply` phase.

## Canonical Pipeline Stages

The pipeline consists of the following 5 stages, executed in order:

**`RAWS -> LOUD -> DRUM -> TONE -> TUNE`**

---

### 1. RAWS (Input Deconstruction)

*   **Purpose:** To convert the raw sensor data into a standardized, scene-linear RGB format. This is the foundation of the entire pipeline.
*   **Operations:** Black point subtraction, white balance, demosaicing, and applying a standard camera input color matrix.
*   **Implementation:** The `head.cpp` module, which uses a GPU-accelerated pipeline.
*   **Learning Strategy:** This stage is considered a "given." Its parameters (white balance, color matrix) are read directly from the RAW file's metadata and are not optimized in the pipeline.

### 2. LOUD (Global Loudness)

*   **Purpose:** To apply a global brightness correction, or "loudness," amplifying the signal to anchor the image's mid-tones to match the target reference.
*   **Implementation:** `loud.cpp`
*   **Learning Strategy:** A simple multiplicative correction factor is learned by comparing the average luminance of the input image to the average luminance of the reference JPEG.

### 3. DRUM (Local Tone Mapping)

*   **Purpose:** To enhance local contrast and detail, counteracting the "global slam" effect of later stages. This is the key stage for improving perceived sharpness and detail.
*   **Implementation:** `drum.cpp`
*   **Algorithm:** Contrast Limited Adaptive Histogram Equalization (CLAHE), which operates on image tiles to enhance local contrast without amplifying noise.
*   **Learning Strategy:** The primary parameter (`clip_limit`) is determined by parsing the camera's Dynamic Range Optimizer (DRO) setting from the EXIF metadata. This uses the camera's own "expert" setting for local contrast. A future improvement would be to train a small neural network to predict the optimal CLAHE parameters.

### 4. TONE (Global Tone Curve)

*   **Purpose:** To apply the primary artistic tone curve. This stage is responsible for the overall "look" of the image, mapping the high dynamic range data to a standard dynamic range for display.
*   **Implementation:** `tone.cpp`
*   **Algorithm:** A 1D Look-Up Table (LUT).
*   **Learning Strategy:** The 1D LUT is learned by matching the luminance histogram of the `DRUM` stage output to the luminance histogram of the reference JPEG.

### 5. TUNE (Color Grading)

*   **Purpose:** To apply fine-grained color corrections and adjustments after the tonal structure is set.
*   **Implementation:** `tune.cpp`
*   **Algorithm:** A 3D Look-Up Table (LUT).
*   **Learning Strategy:** The 3D LUT is learned by analyzing the color correspondence between the `TONE` stage output and the reference JPEG. It learns the residual color transformation needed to perfectly match the target.
