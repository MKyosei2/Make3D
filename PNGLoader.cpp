#include "PNGLoader.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

Image2D LoadPNG(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("ファイルが開けません: " + std::string(filename));

    uint8_t header[8];
    file.read(reinterpret_cast<char*>(header), 8);
    if (memcmp(header, "\x89PNG\x0D\x0A\x1A\x0A", 8) != 0)
        throw std::runtime_error("PNGではありません: " + std::string(filename));

    Image2D img;
    img.width = 1;
    img.height = 1;
    img.pixels = { 255, 255, 255, 255 };
    return img;
}
