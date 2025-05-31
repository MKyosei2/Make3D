#include "PNGLoader.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdint>

static uint32_t ReadUint32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static void ReadChunk(std::ifstream& file, std::string& type, std::vector<uint8_t>& data) {
    uint8_t lengthBytes[4];
    file.read((char*)lengthBytes, 4);
    uint32_t length = ReadUint32(lengthBytes);

    char typeBytes[5] = {};
    file.read(typeBytes, 4);
    type = std::string(typeBytes, 4);

    data.resize(length);
    file.read((char*)data[0], length);
    uint8_t crc[4];
    file.read((char*)crc, 4);
}

// 簡易zlibストリーム（Deflate非圧縮ブロックのみ）
static std::vector<uint8_t> InflateNoCompression(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output;
    size_t i = 2; // zlibヘッダーをスキップ

    while (i + 4 <= input.size()) {
        uint8_t bfinal = input[i] & 1;
        uint8_t btype = (input[i] >> 1) & 0x03;
        ++i;

        if (btype != 0) {
            throw std::runtime_error("Unsupported Deflate block type");
        }

        if (i + 4 > input.size()) throw std::runtime_error("Invalid Deflate block");

        uint16_t len = input[i] | (input[i + 1] << 8);
        uint16_t nlen = input[i + 2] | (input[i + 3] << 8);
        i += 4;

        if ((len ^ 0xFFFF) != nlen)
            throw std::runtime_error("LEN and NLEN mismatch in Deflate");

        if (i + len > input.size())
            throw std::runtime_error("Deflate block overrun");

        output.insert(output.end(), input.begin() + i, input.begin() + i + len);
        i += len;

        if (bfinal) break;
    }

    return output;
}

Image2D LoadPNG(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("ファイルが開けません: " + std::string(filename));

    uint8_t header[8];
    file.read(reinterpret_cast<char*>(header), 8);
    if (memcmp(header, "\x89PNG\r\n\x1a\n", 8) != 0)
        throw std::runtime_error("PNGではありません: " + std::string(filename));

    int width = 0, height = 0;
    int bitDepth = 0, colorType = 0;

    std::vector<uint8_t> idatData;

    while (file) {
        std::string chunkType;
        std::vector<uint8_t> chunkData;
        ReadChunk(file, chunkType, chunkData);

        if (chunkType == "IHDR") {
            width = ReadUint32(&chunkData[0]);
            height = ReadUint32(&chunkData[4]);
            bitDepth = chunkData[8];
            colorType = chunkData[9];
            if (bitDepth != 8 || colorType != 6)
                throw std::runtime_error("8bit RGBA形式以外には対応していません");
        }
        else if (chunkType == "IDAT") {
            idatData.insert(idatData.end(), chunkData.begin(), chunkData.end());
        }
        else if (chunkType == "IEND") {
            break;
        }
    }

    auto decompressed = InflateNoCompression(idatData);
    size_t rowBytes = width * 4;
    size_t expectedSize = (rowBytes + 1) * height;

    if (decompressed.size() < expectedSize)
        throw std::runtime_error("展開サイズが不足しています");

    Image2D image;
    image.width = width;
    image.height = height;
    image.pixels.resize(width * height * 4);

    for (int y = 0; y < height; ++y) {
        size_t srcRow = y * (rowBytes + 1);
        uint8_t filter = decompressed[srcRow];
        if (filter != 0)
            throw std::runtime_error("フィルタータイプ0（無圧縮）以外には対応していません");

        std::memcpy(&image.pixels[y * rowBytes], &decompressed[srcRow + 1], rowBytes);
    }

    return image;
}
