#include "PNGLoader.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <cassert>

// CRCチェック・チャンク結合用
static uint32_t ReadUint32BE(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

// IDAT結合 → inflate（zlib圧縮解除）
std::vector<uint8_t> InflateZlib(const std::vector<uint8_t>& input);

// PNGフィルタ解除
void UnfilterScanlines(std::vector<uint8_t>& out, const std::vector<uint8_t>& in, int width, int height, int bpp) {
    size_t stride = width * bpp;
    out.resize(height * stride);
    const uint8_t* src = in.data();
    uint8_t* dst = out.data();
    for (int y = 0; y < height; ++y) {
        const uint8_t* prev = (y == 0) ? nullptr : &dst[(y - 1) * stride];
        const uint8_t* scan = &src[y * (stride + 1)];
        uint8_t type = scan[0];
        const uint8_t* s = &scan[1];
        uint8_t* d = &dst[y * stride];

        switch (type) {
        case 0: // None
            std::memcpy(d, s, stride);
            break;
        case 1: // Sub
            for (int i = 0; i < stride; ++i)
                d[i] = s[i] + (i >= bpp ? d[i - bpp] : 0);
            break;
        case 2: // Up
            for (int i = 0; i < stride; ++i)
                d[i] = s[i] + (prev ? prev[i] : 0);
            break;
        case 3: // Average
            for (int i = 0; i < stride; ++i) {
                uint8_t a = (i >= bpp) ? d[i - bpp] : 0;
                uint8_t b = (prev ? prev[i] : 0);
                d[i] = s[i] + ((a + b) >> 1);
            }
            break;
        case 4: // Paeth
            for (int i = 0; i < stride; ++i) {
                int a = (i >= bpp) ? d[i - bpp] : 0;
                int b = (prev ? prev[i] : 0);
                int c = (prev && i >= bpp) ? prev[i - bpp] : 0;
                int p = a + b - c;
                int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                d[i] = s[i] + pr;
            }
            break;
        default:
            throw std::runtime_error("Unsupported PNG filter type");
        }
    }
}
// 非圧縮データの読み込み用構造体
struct BitReader {
    const uint8_t* data;
    size_t size;
    size_t bytePos = 0;
    uint32_t bitBuf = 0;
    int bitCount = 0;

    BitReader(const uint8_t* data, size_t size) : data(data), size(size) {}

    uint32_t ReadBits(int n) {
        while (bitCount < n) {
            if (bytePos >= size) throw std::runtime_error("Read past end of stream");
            bitBuf |= data[bytePos++] << bitCount;
            bitCount += 8;
        }
        uint32_t out = bitBuf & ((1 << n) - 1);
        bitBuf >>= n;
        bitCount -= n;
        return out;
    }

    uint32_t ReadBit() {
        return ReadBits(1);
    }

    void AlignByte() {
        bitBuf = 0;
        bitCount = 0;
    }
};

// 固定ハフマンのシンボル読み取り
int ReadHuffmanSymbol(BitReader& br) {
    int code = 0, len = 1;
    while (true) {
        code |= br.ReadBit() << (len - 1);
        if (len == 7 && code < 24) return code + 256;
        if (len == 8 && code >= 48 && code < 192) return code - 48;
        if (len == 9 && code >= 400) return code - 256;
        if (++len > 9) break;
    }
    throw std::runtime_error("Invalid fixed Huffman code");
}

// 長さ → 実際の値への変換
int DecodeLength(BitReader& br, int symbol) {
    static const int baseLength[] = {
        3, 4, 5, 6, 7, 8, 9, 10,
        11,13,15,17,19,23,27,31,
        35,43,51,59,67,83,99,115,
        131,163,195,227,258
    };
    static const int extraBits[] = {
        0,0,0,0,0,0,0,0,1,1,1,1,
        2,2,2,2,3,3,3,3,4,4,4,4,
        5,5,5,5,0
    };
    int i = symbol - 257;
    return baseLength[i] + br.ReadBits(extraBits[i]);
}

int DecodeDistance(BitReader& br, int symbol) {
    static const int baseDist[] = {
        1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
        17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32
    };
    static const int extraBits[] = {
        0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
        7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14
    };
    return baseDist[symbol] + br.ReadBits(extraBits[symbol]);
}

// inflate本体
std::vector<uint8_t> InflateZlib(const std::vector<uint8_t>& input) {
    if (input.size() < 2) throw std::runtime_error("Invalid zlib header");
    uint8_t cmf = input[0], flg = input[1];
    if ((cmf & 0x0F) != 8) throw std::runtime_error("Only deflate compressed zlib supported");

    BitReader br(input.data() + 2, input.size() - 2);
    std::vector<uint8_t> output;

    bool lastBlock = false;
    while (!lastBlock) {
        lastBlock = br.ReadBit();
        int blockType = br.ReadBits(2);
        if (blockType == 0) {
            // 非圧縮ブロック（未対応）
            br.AlignByte();
            throw std::runtime_error("Uncompressed block not supported in this minimal inflate");
        }
        else if (blockType == 1) {
            // 固定ハフマン
            while (true) {
                int symbol = ReadHuffmanSymbol(br);
                if (symbol < 256) {
                    output.push_back((uint8_t)symbol);
                }
                else if (symbol == 256) {
                    break; // ブロック終端
                }
                else if (symbol <= 285) {
                    int length = DecodeLength(br, symbol);
                    int distSymbol = br.ReadBits(5);
                    int distance = DecodeDistance(br, distSymbol);
                    if (distance > output.size())
                        throw std::runtime_error("Invalid distance");
                    for (int i = 0; i < length; ++i) {
                        output.push_back(output[output.size() - distance]);
                    }
                }
                else {
                    throw std::runtime_error("Invalid symbol in fixed Huffman");
                }
            }
        }
        else {
            throw std::runtime_error("Unsupported block type in inflate");
        }
    }
    return output;
}

// --- メイン処理 ---
Image2D LoadPNG(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open PNG");

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), {});
    if (data.size() < 8 || std::memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8) != 0)
        throw std::runtime_error("Invalid PNG signature");

    int width = 0, height = 0, bitDepth = 0, colorType = 0;
    std::vector<uint8_t> idat;

    size_t pos = 8;
    while (pos + 8 < data.size()) {
        uint32_t len = ReadUint32BE(&data[pos]);
        const char* type = (const char*)&data[pos + 4];
        const uint8_t* chunk = &data[pos + 8];
        if (std::memcmp(type, "IHDR", 4) == 0) {
            width = ReadUint32BE(chunk);
            height = ReadUint32BE(chunk + 4);
            bitDepth = chunk[8];
            colorType = chunk[9];
            if (bitDepth != 8 || colorType != 6) throw std::runtime_error("Only 8bit RGBA PNG supported");
        }
        else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), chunk, chunk + len);
        }
        else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += len + 12; // length(4) + type(4) + data(len) + CRC(4)
    }

    std::vector<uint8_t> decompressed = InflateZlib(idat);
    std::vector<uint8_t> pixels;
    UnfilterScanlines(pixels, decompressed, width, height, 4);

    return Image2D{ width, height, pixels };
}
