#include "autotune.hpp"
#include "../../inc/stb_image.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <iostream>

namespace copy::modules::autotune {

    static float srgb_to_linear(float v) {
        if (v <= 0.04045f) return v / 12.92f;
        else return std::pow((v + 0.055f) / 1.055f, 2.4f);
    }

    static float srgb_mean_brightness(const unsigned char* data, int width, int height, int channels) {
        double sum = 0.0;
        long count = (long)width * height;
        for (long i = 0; i < count; i++) {
            const unsigned char* px = &data[i * channels];
            float r = srgb_to_linear(px[0] / 255.0f);
            float g = srgb_to_linear(px[1] / 255.0f);
            float b = srgb_to_linear(px[2] / 255.0f);
            float luma = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            sum += luma;
        }
        return (float)(sum / count);
    }

    float raw_exposure_autotune(const std::string& raw_path, uint32_t preview_offset, uint32_t preview_length, 
                                const uint8_t* pipeline_srgb, int width, int height) {
        
        if (preview_offset == 0 || preview_length == 0) return 0.0f;

        FILE* f = fopen(raw_path.c_str(), "rb");
        if (!f) return 0.0f;

        if (fseek(f, preview_offset, SEEK_SET) != 0) {
            fclose(f);
            return 0.0f;
        }

        std::vector<uint8_t> buffer(preview_length);
        if (fread(buffer.data(), 1, preview_length, f) != preview_length) {
            fclose(f);
            return 0.0f;
        }
        fclose(f);

        int jw, jh, jc;
        unsigned char* jpeg_data = stbi_load_from_memory(buffer.data(), preview_length, &jw, &jh, &jc, 3);
        if (!jpeg_data) return 0.0f;

        float jpeg_b = srgb_mean_brightness(jpeg_data, jw, jh, 3);
        float pipe_b = srgb_mean_brightness(pipeline_srgb, width, height, 3);

        stbi_image_free(jpeg_data);

        float ev = 0.0f;
        if (pipe_b > 1e-6f && jpeg_b > 1e-6f) {
            ev = std::log2(jpeg_b / pipe_b);
        }

        printf("autotune: JPEG brightness: %.4f, Pipeline: %.4f, EV: %+.2f\n", jpeg_b, pipe_b, ev);
        return ev;
    }

}
