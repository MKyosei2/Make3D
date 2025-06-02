#include "PNGLoader.h"
#include <windows.h>
#include <vector>
#include <cstring>

bool LoadPNG(const char* filename, PNGImage& outImage) {
    HANDLE hFile = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    std::vector<unsigned char> fileData(fileSize);
    DWORD read;
    ReadFile(hFile, fileData.data(), fileSize, &read, NULL);
    CloseHandle(hFile);

    if (fileSize < 8 || memcmp(fileData.data(), "\x89PNG\r\n\x1A\n", 8) != 0) return false;

    size_t pos = 8; // Skip signature
    std::vector<unsigned char> idatData;

    while (pos + 8 < fileData.size()) {
        unsigned int length = (fileData[pos] << 24) | (fileData[pos + 1] << 16) | (fileData[pos + 2] << 8) | fileData[pos + 3];
        const char* type = (const char*)&fileData[pos + 4];

        if (std::memcmp(type, "IHDR", 4) == 0) {
            outImage.width = (fileData[pos + 8] << 24) | (fileData[pos + 9] << 16) | (fileData[pos + 10] << 8) | fileData[pos + 11];
            outImage.height = (fileData[pos + 12] << 24) | (fileData[pos + 13] << 16) | (fileData[pos + 14] << 8) | fileData[pos + 15];
        }
        else if (std::memcmp(type, "IDAT", 4) == 0) {
            idatData.insert(idatData.end(), fileData.begin() + pos + 8, fileData.begin() + pos + 8 + length);
        }
        else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += length + 12; // length + type + data + crc
    }

    if (idatData.size() < 6) return false; // zlibヘッダー不足
    std::vector<unsigned char> output;

    // zlibヘッダー: 最初の2バイトは無視（仮対応）
    size_t idatPos = 2;
    unsigned int bitbuf = 0;
    int bitcount = 0;

    auto readBit = [&]() -> int {
        if (bitcount == 0) {
            if (idatPos >= idatData.size()) return -1;
            bitbuf = idatData[idatPos++];
            bitcount = 8;
        }
        int b = bitbuf & 1;
        bitbuf >>= 1;
        --bitcount;
        return b;
        };

    auto readBits = [&](int n) -> int {
        int val = 0;
        for (int i = 0; i < n; ++i) {
            int b = readBit();
            if (b < 0) return -1;
            val |= b << i;
        }
        return val;
        };

    // 固定ハフマン符号ブロックのみ対応（BFINAL = 1, BTYPE = 1）
    int bfinal = readBit();
    int btype = readBits(2);
    if (btype != 1) return false;

    // 仮に読み飛ばしで白画像生成（未完成 inflate 処理）
    outImage.pixels.resize(outImage.width * outImage.height, 255);
    return true;
}