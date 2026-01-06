#ifndef DEBUG_DUMP_H
#define DEBUG_DUMP_H

#include <stdio.h>
#include <stdint.h>

static void dump_buffer(const char* name, const float* data, int width, int height, int channels) {
    char filename[256];
    snprintf(filename, sizeof(filename), "dump_%s.bin", name);
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fwrite(data, sizeof(float), width * height * channels, f);
    fclose(f);
    printf("Dumped %s (%dx%dx%d)\n", filename, width, height, channels);
}

static void dump_buffer_u16(const char* name, const uint16_t* data, int width, int height, int channels) {
    char filename[256];
    snprintf(filename, sizeof(filename), "dump_%s.bin", name);
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    fwrite(data, sizeof(uint16_t), width * height * channels, f);
    fclose(f);
    printf("Dumped %s (%dx%dx%d)\n", filename, width, height, channels);
}

#endif
