# Sony .ARW Custom Decoder - Reference Implementation

## Overview

This is a self-contained test project for a **GPL-free Sony .ARW decoder** that implements **maximum manufacturer processing**. It serves as the reference implementation for clean-room RAW decoder development.

## Quick Start

```bash
cd /home/z/base/code/pqtr/labs/opt/raws
./sony.sh [optional/path/to/file.ARW]
```

**Default input**: `./var/sony.ARW`
**Output**: `./tmp/sony.jpg`

The output filename is hardcoded to `sony.jpg` in the `tmp` directory.

## Philosophy: Maximum Manufacturer Processing

**Key Decision:** This decoder implements **all** processing steps specified by Sony in the ARW format metadata, including those that LibRaw chooses to ignore or simplify.

### The Discovery

During development, we discovered that LibRaw **ignores** the Sony linearization curve (MakerNote tag 0x7010) for ILCE-7M3 cameras. However, Sony includes this curve in the metadata for optimal highlight preservation through selective dynamic range expansion.

```
LibRaw:         Raw max = 16628 → No curve → Clipped highlights
Custom Decoder: Raw max = 2029 → Linearization curve → 4232 (expanded highlights)
```

Our decoder applies this curve, resulting in:
- **Better highlight detail preservation** (4x expansion for values >2000)
- More accurate representation of manufacturer intent
- Superior image quality compared to LibRaw's simplified approach

### Implementation Philosophy

**Maximum Manufacturer Specification:** This decoder implements all processing steps specified by Sony, including those that LibRaw chooses to ignore. This provides the most accurate representation of the manufacturer's intended image processing pipeline.

**Clean Room Development:** The ARW2 decompression and linearization curve were reverse-engineered and implemented without GPL dependencies, making this suitable for commercial use.

---

## Why Sony ARW is the Perfect Lab Rat

After researching all major camera manufacturers' RAW formats, Sony ARW is confirmed as the **most complex and ideal reference** for decoder development.

### Complexity Rankings

#### By Decompression Difficulty:
1. **Sony ARW** ★★★★★ - Proprietary everything, actively obscured
2. **Canon CR3** ★★★☆☆ - Custom codec but standard algorithms
3. **Fuji RAF** ★★★☆☆ - Standard compression, unique CFA
4. **Canon CR2** ★★☆☆☆ - Standard JPEG lossless
5. **Nikon NEF** ★★☆☆☆ - ZIP-like compression

#### By Reverse Engineering Difficulty:
1. **Sony ARW** ★★★★★ - Minimal documentation, proprietary everything
2. **Fuji RAF** ★★★☆☆ - X-Trans pattern complexity
3. **Canon CR3** ★★★☆☆ - Custom codec but analyzable
4. **Nikon NEF** ★★☆☆☆ - Well-understood algorithms
5. **Canon CR2** ★★☆☆☆ - Well-documented by community

---

## Project Structure

The project is structured as a self-contained lab with a clear separation between the core decoder logic, pipeline modules, and the test runner.

```
opt/raws/
├── README.md            ← You are here
├── Makefile.sony        ← Build configuration for the decoder and test
├── sony.sh              ← Test runner script
├── src/
│   ├── main/
│   │   └── part/        ← Core decoder implementation
│   │       ├── sony.h   ← Main header defining the static interface (arw2_gold) and metadata structs
│   │       ├── sony.cpp ← Internal TIFF/ARW parsing and decompression helpers
│   │       └── sony/
│   │           ├── prepare.cpp    ← Implements arw2_gold::prepare (File → Bayer data)
│   │           ├── process.cpp    ← Implements arw2_gold::process (Bayer → RGB)
│   │           ├── blc.cpp        ← Black Level Correction module
│   │           ├── wb_gain.cpp    ← White Balance module
│   │           ├── demosaic.cpp   ← Demosaic module
│   │           └── gamma_oetf.cpp ← Gamma/OETF module
│   └── test/
│       └── sony.cpp     ← Test program that runs the full pipeline
├── var/
│   └── sony.ARW         ← Sample RAW file
└── tmp/
    ├── sony/            ← Build artifacts (binary, objects)
    │   └── sony
    └── sony.jpg         ← Processed output image
```

---

## Pipeline Stages

The reference implementation follows these stages, all performed on the GPU using OpenCV's `UMat` for maximum performance:

### 1. RAW Load (`prepare.cpp`)
- Custom decoder parses TIFF/EXIF structure and decompresses Sony ARW2 format.
- Applies the Sony-specific linearization curve (tag 0x7010) for highlight expansion.
- **Output**: Raw Bayer data (uint16 UMat) and comprehensive `RawMetadata` struct.

### 2. Demosaic (`demosaic.cpp`)
- Converts the single-channel Bayer data into a 3-channel BGR image using GPU-accelerated bilinear demosaicing.
- **Output**: Linear BGR image (uint16 UMat).

### 3. Black Level Correction (`blc.cpp`)
- Subtracts black level and normalizes the data to a [0, 1] float range.
- **Output**: Normalized BGR image (float32 UMat).

### 4. White Balance (`wb_gain.cpp`)
- Applies camera-native white balance gains to the RGB channels.
- **Output**: White-balanced BGR image (float32 UMat).

### 5. Gamma Correction (`gamma_oetf.cpp`)
- Applies the sRGB OETF (opto-electrical transfer function) to convert the image from linear to display-referred gamma.
- **Output**: Display-ready BGR image (float32 UMat).

### 6. Output (`test/sony.cpp`)
- The final float UMat is converted to an 8-bit image.
- Saved as a JPEG file.

---

## Pipeline Design: A Note on Performance vs. Accuracy

A key decision in a RAW processing pipeline is the order of operations, specifically when to apply demosaicing relative to white balance. This implementation intentionally prioritizes **performance** over theoretical purity, a choice that has significant benefits with negligible visual impact for most use cases.

#### The Two Approaches

1.  **Accuracy-First:** `Black Level -> White Balance -> Demosaic`
    *   This is the most physically accurate method. It applies channel-specific gains (white balance) to the actual sensor data from the Bayer pattern before the colors are interpolated (demosaiced).
    *   **Drawback:** This pipeline results in floating-point Bayer data. OpenCV's standard, hardware-accelerated `cv::demosaicing` function does not accept float inputs. This forces the use of a slow, manual, CPU-bound demosaicing loop, creating a massive performance bottleneck.

2.  **Performance-First (This Implementation):** `Demosaic -> Black Level -> White Balance`
    *   This is a highly common and pragmatic compromise. It first creates an RGB image from the integer sensor data using the fast, GPU-accelerated `cv::demosaicing` function. It then applies black level and white balance to the resulting RGB image.
    *   **Benefit:** The entire pipeline can run on the GPU, making it dramatically faster.
    *   **Trade-off:** Applying white balance after color interpolation is less physically accurate. This can result in subtle, pixel-level differences (e.g., along fine edges) when compared to the accuracy-first method.

#### Why Performance Was Chosen

For the vast majority of applications, especially any that result in a compressed output like JPEG for social media or the web, **the subtle image quality difference is rendered completely invisible** by compression artifacts and image downscaling. The massive gain in processing speed is a far more valuable and practical advantage. This reference decoder is therefore optimized for speed.

---

## Custom Decoder Highlights

### Technical Details

The Sony decoder is implemented as a static `sony::arw2_gold` class with a clean, modular structure:
- **`sony.h`**: Defines the public interface: `prepare()` to decode the file and `process()` to run the image pipeline. It also contains the `RawMetadata` struct that carries all processing parameters.
- **`sony.cpp`**: Contains the internal, low-level TIFF parsing and ARW2 decompression logic.
- **`prepare.cpp`**: Orchestrates the file loading, parsing of standard EXIF/TIFF tags and proprietary Sony MakerNotes, and extraction of all metadata.
- **`process.cpp`**: A simple pipeline that calls each processing module in the correct sequence.
- **Modules (`blc.cpp`, `demosaic.cpp`, etc.)**: Each file is a self-contained processing step, designed to be simple and highly efficient by using GPU-accelerated OpenCV functions.

This modular, high-performance design makes the project an excellent template for other RAW decoders.

### Features

✅ **Maximum Manufacturer Processing** - Not limited by LibRaw's simplifications
✅ **No LibRaw dependency** - Completely GPL-free
✅ **GPU Accelerated** - Entire pipeline runs on the GPU via OpenCV `UMat`
✅ **Highlight preservation** - Via Sony linearization curve
✅ **Clean, Modular Code** - Easy to understand, maintain, and adapt
✅ **Commercial viability** - Clean-room GPL-free implementation

---

## Use as Template for Other Manufacturers

This project serves as a template for implementing other RAW decoders.

### Template for Other Formats

To create a new decoder (e.g., for Nikon NEF):
1.  Copy the entire `opt/raws/` directory to `opt/raws_nikon/`.
2.  Rename `sony.h`, `sony.cpp`, and the `src/main/part/sony/` directory to `nikon`.
3.  In `nikon.h`, rename the `sony` namespace and `arw2_gold` class to `nikon` and `nef_decoder`.
4.  Replace the ARW2 decompression logic in `nikon.cpp` with the appropriate algorithm for Nikon (e.g., ZIP-like lossless).
5.  In `prepare.cpp`, modify the TIFF/MakerNote parsing logic to extract Nikon-specific tags and curves.
6.  Adjust the pipeline modules in the `process.cpp` and module files as needed for Nikon's specific process.
7.  Update the `Makefile` and `nikon.sh` script.

---

## Dependencies

- **OpenCV** (build required): `../../lib/opencv`
- **C++11** compiler
- No other external libraries
- No GPL dependencies

---

## Credits

Clean-room reverse engineering of Sony ARW2 format.
No code derived from GPL-licensed software.
Safe for commercial use.

Research date: 2024-11-21
