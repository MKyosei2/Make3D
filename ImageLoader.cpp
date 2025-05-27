#include "ImageLoader.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

uint32_t ReadUint32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

Image2D ImageLoader::Load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("PNG file not found");
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});

    if (data.size() < 8 || std::memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) != 0)
        throw std::runtime_error("Invalid PNG signature");

    // NOTE: ここでは実際のzlibデコードとチャンク解析は省略。IDAT処理を追加する必要あり。
    throw std::runtime_error("PNG inflate not implemented");
}