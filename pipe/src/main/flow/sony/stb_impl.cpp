// stb_impl.cpp - Single compilation unit for stb headers
//
// Separating STB_IMAGE_IMPLEMENTATION avoids recompilation on every build.

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "stb_image.h"
#pragma GCC diagnostic pop
