#include "PNGLoader.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

Image2D LoadPNG(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("PNG file not found");
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});

    if (data.size() < 8 || std::memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) != 0)
        throw std::runtime_error("Invalid PNG signature");

    throw std::runtime_error("PNG inflate not implemented");
}
