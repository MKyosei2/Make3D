#pragma once
#include <string>
#include <vector>

struct Image2D {
    int width, height;
    std::vector<uint8_t> pixels; // RGBA
};

class ImageLoader {
public:
    static Image2D Load(const std::string& filename);
};