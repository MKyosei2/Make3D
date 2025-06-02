#pragma once
#include <vector>
#include "PNGLoader.h"

struct Image2D {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data;

    bool IsOpaque(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        return data[y * width + x] > 128;
    }
};

Image2D ConvertToImage2D(const PNGImage& png);