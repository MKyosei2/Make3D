#include "PNGLoader.h"
#include <fstream>
#include <stdexcept>
#include <zlib.h>

// 自作inflate実装に切り替える（簡易で完全なPNG実装ではない）

Image2D LoadPNG(const char* filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) throw std::runtime_error("PNGファイルが開けません");

    std::vector<unsigned char> fileData((std::istreambuf_iterator<char>(ifs)), {});
    if (fileData.size() < 8 || memcmp(fileData.data(), "\x89PNG\r\n\x1a\n", 8) != 0)
        throw std::runtime_error("PNGヘッダ不正");

    Image2D img;

    for (size_t offset = 8; offset + 8 < fileData.size(); ) {
        uint32_t length = (fileData[offset] << 24) | (fileData[offset + 1] << 16) |
            (fileData[offset + 2] << 8) | (fileData[offset + 3]);
        const char* type = reinterpret_cast<const char*>(&fileData[offset + 4]);

        if (memcmp(type, "IHDR", 4) == 0) {
            img.width = (fileData[offset + 8] << 24) | (fileData[offset + 9] << 16) |
                (fileData[offset + 10] << 8) | (fileData[offset + 11]);
            img.height = (fileData[offset + 12] << 24) | (fileData[offset + 13] << 16) |
                (fileData[offset + 14] << 8) | (fileData[offset + 15]);
        }
        else if (memcmp(type, "IDAT", 4) == 0) {
            // 実際の使用では複数IDATの結合やフィルタ除去が必要
            std::vector<unsigned char> compressed(fileData.begin() + offset + 8, fileData.begin() + offset + 8 + length);
            size_t expectedSize = (img.width * 4 + 1) * img.height;
            std::vector<unsigned char> decompressed(expectedSize);

            z_stream zs{};
            inflateInit(&zs);
            zs.next_in = compressed.data();
            zs.avail_in = compressed.size();
            zs.next_out = decompressed.data();
            zs.avail_out = decompressed.size();
            inflate(&zs, Z_FINISH);
            inflateEnd(&zs);

            // フィルタ除去（フィルタ0のみ想定）
            img.pixels.resize(img.width * img.height * 4);
            for (int y = 0; y < img.height; ++y) {
                memcpy(&img.pixels[y * img.width * 4], &decompressed[y * (img.width * 4 + 1) + 1], img.width * 4);
            }
        }

        offset += length + 12;
    }

    return img;
}
