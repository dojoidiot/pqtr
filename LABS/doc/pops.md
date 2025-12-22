# POPS (Pipeline Of Plugins System) Architecture

The POPS model is a canonical, 5-stage image processing pipeline designed to transform scene-linear RAW data into a final, display-referred image that mimics a target look (e.g., a camera’s embedded JPEG).

The architecture is based on the modular design of professional photo editing software like Darktable, where different aspects of image correction are separated into a logical sequence of expert modules. Each module in the POPS pipeline is a self-contained plugin with a `learn` and `apply` phase.

## Canonical Pipeline Stages

The pipeline consists of the following 5 stages, executed in order:

**`RAWS -> LOUD -> DRUM -> TONE -> TUNE`**

---

### 1. RAWS (Input Deconstruction)

*   **Purpose:** To convert the raw sensor data into a standardized, scene-linear RGB format. This is the foundation of the entire pipeline.
*   **Operations:** Black point subtraction, white balance, demosaicing, and applying a standard camera input color matrix.
*   **Implementation:** The `head.cpp` module, which uses a GPU-accelerated pipeline.
*   **Learning Strategy:** This stage is considered a "given." Its parameters (white balance, color matrix) are read directly from the RAW file’s metadata and are not optimized in the pipeline.

### 2. LOUD (Global Loudness)

*   **Purpose:** To apply a global brightness correction, or “loudness,” amplifying the signal to anchor the image’s mid-tones to match the target reference.
*   **Implementation:** `loud.cpp`
*   **Learning Strategy:** A simple multiplicative correction factor is learned by comparing the average luminance of the input image to the average luminance of the reference JPEG.

### 3. DRUM (Local Tone Mapping)

*   **Purpose:** To enhance local contrast and detail, counteracting the “global slam” effect of later stages. This is the key stage for improving perceived sharpness and detail.
*   **Implementation:** `drum.cpp`
*   **Algorithm:** Contrast Limited Adaptive Histogram Equalization (CLAHE), which operates on image tiles to enhance local contrast without amplifying noise.
*   **Learning Strategy:** The primary parameter (`clip_limit`) is determined by parsing the camera’s Dynamic Range Optimizer (DRO) setting from the EXIF metadata. This uses the camera’s own “expert” setting for local contrast. A future improvement would be to train a small neural network to predict the optimal CLAHE parameters.

### 4. TONE (Global Tone Curve)

*   **Purpose:** To apply the primary artistic tone curve. This stage is responsible for the overall “look” of the image, mapping the high dynamic range data to a standard dynamic range for display.
*   **Implementation:** `tone.cpp`
*   **Algorithm:** A 1D Look-Up Table (LUT).
*   **Learning Strategy:** The 1D LUT is learned by matching the luminance histogram of the `DRUM` stage output to the luminance histogram of the reference JPEG.

### 5. TUNE (Color Grading)

*   **Purpose:** To apply fine-grained color corrections and adjustments after the tonal structure is set.
*   **Implementation:** `tune.cpp`
*   **Architectural Improvement (HSV-based Learning):** Instead of a generic RGB-to-RGB 3D LUT, the `TUNE` stage now operates in the more perceptual HSV (Hue, Saturation, Value) color space. This better reflects the "expert advice" to handle color dimensions independently.
    1.  **RGB to HSV:** Input pixels are converted to HSV.
    2.  **Orthogonal Correction:** The learning process is broken down into learning separate corrections for each channel:
        *   **Hue:** A 1D LUT corrects hue shifts.
        *   **Saturation:** A 1D LUT corrects saturation, allowing for advanced gamut control like desaturating the most extreme colors ("pulling through white").
        *   **Value:** A final 1D LUT makes minor brightness adjustments.
    3.  **HSV to RGB:** The corrected HSV pixel is converted back to RGB.
*   **Learning Strategy:** The three 1D LUTs for H, S, and V are learned by analyzing the correspondence between the `TONE` stage output and a "tone-matched" reference JPEG. This isolates the learning to pure color and value transformations, making the process orthogonal to the `TONE` stage.

---

## Learning Strategy: Enforcing Orthogonality (Sequential Residual Learning)

To ensure that each stage of the POPS pipeline is maximally effective and responsible only for its specific domain, we employ a strategy of “mathematical orthogonality” through sequential residual learning.

**The Problem:**
In a naive learning setup where each stage (`LOUD`, `TONE`, `TUNE`) simply compares its input against the *final* target JPEG, each stage attempts to correct for all accumulated errors from prior stages. This leads to a “collective blame” scenario, where plugins might “fight” each other, learning compensations for errors outside their intended scope (e.g., the `TUNE` plugin might learn to fix a luminance error rather than a color error).

**The Principle: Orthogonality in Action**
Each plugin should ideally operate on a distinct dimension of the image, with minimal interference from others. For example:
*   `LOUD` is responsible for global brightness.
*   `DRUM` is responsible for local contrast.
*   `TONE` is responsible for global tone mapping (luminance distribution).
*   `TUNE` is responsible for global color correction (chrominance).

**The Solution: Progressive Learning Targets**

Instead of comparing every stage's output to the final reference JPEG directly, we will progressively refine the learning target for subsequent stages. This ensures each stage learns to correct only the residual error specific to its domain.

**Example: Learning for the TUNE (Color Grading) Stage**

*   **Current (Inefficient) Approach:** `tune::learn(TONE_output, final_JPEG)`
    *   Here, `TUNE` tries to learn both tonal *and* color corrections, even though `TONE` should have handled the tonality.

*   **Improved (Orthogonal) Approach:**
    1.  Take the output of the `TONE` stage (`tone_rgb`).
    2.  Generate a **“tone-matched reference”**: This is the original `final_JPEG` modified such that its luminance histogram exactly matches the luminance histogram of our `tone_rgb` output. This step effectively removes all *tonal* differences between `tone_rgb` and this new reference.
    3.  **`tune::learn(TONE_output, tone_matched_reference)`**
        *   Now, when `TUNE` learns, the tonal differences have been removed from its target. It can focus purely on correcting the *color* discrepancies, making its learning truly orthogonal to the `TONE` stage’s responsibility.

This approach will be extended to other stages as necessary, ensuring that each plugin learns to perform its specialized role optimally within the pipeline.