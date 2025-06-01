#pragma once
#include <vector>
#include <string>

struct Image2D {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels; // RGBA

    bool IsOpaque(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height) return false;
        return pixels[(y * width + x) * 4 + 3] > 128;
    }
};

Image2D LoadPNG(const char* filename);
