/*
    autotune.h - JPEG filmic matching

    Single auto-tune method: analyze on-camera JPEG to derive filmic parameters.
*/

#ifndef AUTOTUNE_H
#define AUTOTUNE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Result of JPEG filmic auto-tune */
typedef struct {
    int valid;              /* 1 if analysis succeeded */
    float grey;             /* Grey point (linear, ~0.1845) */
    float black_ev;         /* Black point in EV relative to grey */
    float white_ev;         /* White point in EV relative to grey */
    float dynamic_range;    /* white_ev - black_ev */
    float exposure_ev;      /* Exposure adjustment to match JPEG brightness */
} FilmicAutotuneResult;

/*
    Analyze on-camera JPEG to derive filmic parameters.

    Returns filmic parameters that will produce output matching the JPEG:
    - grey: middle grey point
    - black_ev: shadow rolloff point
    - white_ev: highlight rolloff point
    - exposure_ev: exposure compensation to match brightness
*/
FilmicAutotuneResult jpeg_filmic_autotune(const char* jpeg_path);

/*
    Compare pipeline output to JPEG and calculate optimal EV adjustment.

    Usage:
    1. Run pipeline with current exposure
    2. Call jpeg_exposure_match() with output PNG data and JPEG path
    3. Add returned EV to camera style exposure
    4. Store adjusted exposure in cameras.c for future use

    Parameters:
    - jpeg_path: path to reference on-camera JPEG
    - pipeline_srgb: pipeline output (uint8 sRGB, 3 channels, same as PNG)
    - width, height: image dimensions

    Returns: EV adjustment to add to current exposure
*/
float jpeg_exposure_match(const char* jpeg_path,
                          const unsigned char* pipeline_srgb,
                          int width, int height);

/*
    Extract embedded JPEG preview from RAW file using known offset/length.

    Parameters:
    - raw_path: path to RAW file
    - preview_offset: byte offset of JPEG in file (from EXIF parsing)
    - preview_length: length of JPEG data
    - out_jpeg_path: where to write the extracted JPEG

    Returns: 0 on success, -1 on failure
*/
int extract_embedded_jpeg(const char* raw_path, unsigned int preview_offset,
                          unsigned int preview_length, const char* out_jpeg_path);

/*
    Auto-tune exposure from embedded JPEG in RAW file.

    Main entry point for fully automatic calibration:
    1. Extract embedded JPEG preview from RAW using offset/length
    2. Compare pipeline output brightness to embedded JPEG
    3. Return EV adjustment to match

    Parameters:
    - raw_path: path to RAW file (ARW, CR2, NEF, etc.)
    - preview_offset: byte offset of embedded JPEG (from EXIF parsing)
    - preview_length: length of embedded JPEG
    - pipeline_srgb: pipeline output (uint8 sRGB, 3 channels)
    - width, height: pipeline output dimensions

    Returns: EV adjustment to add to current exposure
*/
float raw_exposure_autotune(const char* raw_path,
                            unsigned int preview_offset,
                            unsigned int preview_length,
                            const unsigned char* pipeline_srgb,
                            int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* AUTOTUNE_H */
