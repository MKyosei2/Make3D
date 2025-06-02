#include "ImageUtils.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <string.h>

bool LoadPNG(const char* filename, Image2D& outImage) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), {});
    if (buffer.size() < 8 || memcmp(&buffer[1], "PNG", 3) != 0) return false;

    // 非圧縮の32bit RGBAのみ対応（自作簡易PNGデコーダ）
    int width = 0, height = 0;
    for (size_t i = 8; i < buffer.size();) {
        uint32_t length = (buffer[i] << 24) | (buffer[i + 1] << 16) | (buffer[i + 2] << 8) | buffer[i + 3];
        const char* type = reinterpret_cast<const char*>(&buffer[i + 4]);

        if (memcmp(type, "IHDR", 4) == 0) {
            width = (buffer[i + 8] << 24) | (buffer[i + 9] << 16) | (buffer[i + 10] << 8) | buffer[i + 11];
            height = (buffer[i + 12] << 24) | (buffer[i + 13] << 16) | (buffer[i + 14] << 8) | buffer[i + 15];
        }
        if (memcmp(type, "IDAT", 4) == 0) {
            // 今回は zlib inflate 未対応のためエラーとする（PNG簡易対応）
            return false;
        }
        if (memcmp(type, "IEND", 4) == 0) break;
        i += 12 + length;
    }

    if (width == 0 || height == 0) return false;
    outImage.width = width;
    outImage.height = height;
    outImage.pixels.assign(width * height, 0xFFFFFFFF); // 仮: 全ピクセル不透明白

    return true;
}

bool Image2D::IsOpaque(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    uint32_t px = pixels[y * width + x];
    return ((px >> 24) & 0xFF) > 0; // αチャンネルチェック
}
