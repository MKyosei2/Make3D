#pragma once
#include <vector>
#include <cstdint>
#include <string>

struct Image2D {
    int width, height;
    std::vector<uint8_t> pixels; // RGBAŒ`Ž®
};

Image2D LoadPNG(const char* filename);
