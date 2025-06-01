#include "PNGLoader.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <cstring>

// CRC テーブル生成（省略してもよいが形式的に必要）
static uint32_t crc32_table[256];
static void InitCRCTable() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
}

// 可変バッファにチャンクを読み込む
static std::vector<uint8_t> ReadChunk(std::ifstream& ifs, char outType[5]) {
    uint32_t length = 0;
    ifs.read(reinterpret_cast<char*>(&length), 4);
    length = _byteswap_ulong(length);
    ifs.read(outType, 4);
    outType[4] = 0;
    std::vector<uint8_t> data(length);
    ifs.read(reinterpret_cast<char*>(data.data()), length);
    uint32_t crc;
    ifs.read(reinterpret_cast<char*>(&crc), 4); // ignore CRC
    return data;
}

// zlibの簡易解凍器（固定ハフマンのみ対応、8bit literal + LZ77）
static std::vector<uint8_t> InflateZlib(const std::vector<uint8_t>& input);

// フィルタ復元（サブセット：フィルタ0,1,2のみ対応）
static void Unfilter(Image2D& img, int bytesPerPixel) {
    const int stride = img.width * bytesPerPixel;
    std::vector<uint8_t> raw = img.pixels;
    img.pixels.clear();
    img.pixels.resize(img.height * stride);

    for (int y = 0; y < img.height; ++y) {
        uint8_t filter = raw[y * (stride + 1)];
        uint8_t* dst = &img.pixels[y * stride];
        const uint8_t* src = &raw[y * (stride + 1) + 1];

        if (filter == 0) {
            std::memcpy(dst, src, stride);
        }
        else if (filter == 1) { // Sub
            for (int x = 0; x < stride; ++x)
                dst[x] = src[x] + (x >= bytesPerPixel ? dst[x - bytesPerPixel] : 0);
        }
        else if (filter == 2) { // Up
            const uint8_t* prev = (y > 0) ? &img.pixels[(y - 1) * stride] : nullptr;
            for (int x = 0; x < stride; ++x)
                dst[x] = src[x] + (prev ? prev[x] : 0);
        }
        else {
            throw std::runtime_error("Unsupported PNG filter");
        }
    }
}

Image2D LoadPNG(const char* path) {
    InitCRCTable();

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("PNG file not found");

    uint8_t signature[8];
    ifs.read(reinterpret_cast<char*>(signature), 8);
    if (std::memcmp(signature, "\x89PNG\r\n\x1a\n", 8) != 0)
        throw std::runtime_error("Invalid PNG signature");

    int width = 0, height = 0;
    std::vector<uint8_t> idat_data;

    while (true) {
        char chunkType[5];
        std::vector<uint8_t> chunk = ReadChunk(ifs, chunkType);

        if (strcmp(chunkType, "IHDR") == 0) {
            width = _byteswap_ulong(*reinterpret_cast<uint32_t*>(&chunk[0]));
            height = _byteswap_ulong(*reinterpret_cast<uint32_t*>(&chunk[4]));
            // Bit depth, color type, compression, filter, interlace も必要に応じて確認
        }
        else if (strcmp(chunkType, "IDAT") == 0) {
            idat_data.insert(idat_data.end(), chunk.begin(), chunk.end());
        }
        else if (strcmp(chunkType, "IEND") == 0) {
            break;
        }
    }

    std::vector<uint8_t> inflated = InflateZlib(idat_data);

    Image2D img;
    img.width = width;
    img.height = height;
    img.pixels = std::move(inflated);
    Unfilter(img, 4); // 32bit RGBA想定

    return img;
}
// Bit読み出し用ユーティリティ
class BitReader {
    const std::vector<uint8_t>& data;
    size_t bytePos = 2; // skip zlibヘッダー2バイト
    int bitPos = 0;

public:
    BitReader(const std::vector<uint8_t>& d) : data(d) {}

    int ReadBits(int n) {
        int result = 0;
        for (int i = 0; i < n; ++i) {
            if (bytePos >= data.size()) return -1;
            int bit = (data[bytePos] >> bitPos) & 1;
            result |= (bit << i);
            ++bitPos;
            if (bitPos >= 8) {
                bitPos = 0;
                ++bytePos;
            }
        }
        return result;
    }

    int ReadByteAlign() {
        bitPos = 0;
        return 0;
    }

    size_t GetBytePos() const {
        return bytePos;
    }
};

// 固定ハフマン用デコードテーブル（288リテラル+距離コード）
struct HuffmanTable {
    std::vector<int> codes;

    HuffmanTable(bool isLiteral) {
        int size = isLiteral ? 288 : 32;
        codes.resize(1 << 9, -1);
        int code = 0;

        int lens[288];
        for (int i = 0; i <= 143; ++i) lens[i] = 8;
        for (int i = 144; i <= 255; ++i) lens[i] = 9;
        for (int i = 256; i <= 279; ++i) lens[i] = 7;
        for (int i = 280; i <= 287; ++i) lens[i] = 8;

        for (int i = 0; i < size; ++i) {
            int len = lens[i];
            if (len == 0) continue;
            int rev = 0;
            for (int j = 0; j < len; ++j) {
                rev = (rev << 1) | ((i >> j) & 1);
            }
            int base = rev << (9 - len);
            for (int j = 0; j < (1 << (9 - len)); ++j) {
                codes[base | j] = i;
            }
        }
    }

    int Decode(BitReader& br) {
        int bits = br.ReadBits(9);
        return codes[bits];
    }
};

// Bit読み出し用ユーティリティ
class BitReader {
    const std::vector<uint8_t>& data;
    size_t bytePos = 2; // skip zlibヘッダー2バイト
    int bitPos = 0;

public:
    BitReader(const std::vector<uint8_t>& d) : data(d) {}

    int ReadBits(int n) {
        int result = 0;
        for (int i = 0; i < n; ++i) {
            if (bytePos >= data.size()) return -1;
            int bit = (data[bytePos] >> bitPos) & 1;
            result |= (bit << i);
            ++bitPos;
            if (bitPos >= 8) {
                bitPos = 0;
                ++bytePos;
            }
        }
        return result;
    }

    int ReadByteAlign() {
        bitPos = 0;
        return 0;
    }

    size_t GetBytePos() const {
        return bytePos;
    }
};

// 固定ハフマン用デコードテーブル（288リテラル+距離コード）
struct HuffmanTable {
    std::vector<int> codes;

    HuffmanTable(bool isLiteral) {
        int size = isLiteral ? 288 : 32;
        codes.resize(1 << 9, -1);
        int code = 0;

        int lens[288];
        for (int i = 0; i <= 143; ++i) lens[i] = 8;
        for (int i = 144; i <= 255; ++i) lens[i] = 9;
        for (int i = 256; i <= 279; ++i) lens[i] = 7;
        for (int i = 280; i <= 287; ++i) lens[i] = 8;

        for (int i = 0; i < size; ++i) {
            int len = lens[i];
            if (len == 0) continue;
            int rev = 0;
            for (int j = 0; j < len; ++j) {
                rev = (rev << 1) | ((i >> j) & 1);
            }
            int base = rev << (9 - len);
            for (int j = 0; j < (1 << (9 - len)); ++j) {
                codes[base | j] = i;
            }
        }
    }

    int Decode(BitReader& br) {
        int bits = br.ReadBits(9);
        return codes[bits];
    }
};

std::vector<uint8_t> InflateZlib(const std::vector<uint8_t>& input) {
    BitReader br(input);
    std::vector<uint8_t> output;

    bool finalBlock = false;
    while (!finalBlock) {
        finalBlock = br.ReadBits(1);
        int type = br.ReadBits(2);

        if (type == 1) {
            // 固定ハフマン
            HuffmanTable litTable(true);
            HuffmanTable distTable(false);

            while (true) {
                int sym = litTable.Decode(br);
                if (sym < 256) {
                    output.push_back((uint8_t)sym);
                }
                else if (sym == 256) {
                    break; // ブロック終了
                }
                else if (sym > 256 && sym <= 285) {
                    int len = 0, dist = 0;
                    static const int len_base[] = {
                        3,4,5,6,7,8,9,10,11,13,15,17,
                        19,23,27,31,35,43,51,59,
                        67,83,99,115,131,163,195,227,258
                    };
                    static const int len_extra[] = {
                        0,0,0,0,0,0,0,0,1,1,1,1,
                        2,2,2,2,3,3,3,3,
                        4,4,4,4,5,5,5,5,0
                    };
                    sym -= 257;
                    len = len_base[sym] + br.ReadBits(len_extra[sym]);

                    int dist_sym = distTable.Decode(br);
                    static const int dist_base[] = {
                        1,2,3,4,5,7,9,13,17,25,33,49,
                        65,97,129,193,257,385,513,769,
                        1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
                    };
                    static const int dist_extra[] = {
                        0,0,0,0,1,1,2,2,3,3,4,4,
                        5,5,6,6,7,7,8,8,
                        9,9,10,10,11,11,12,12,13,13
                    };
                    dist = dist_base[dist_sym] + br.ReadBits(dist_extra[dist_sym]);

                    for (int i = 0; i < len; ++i) {
                        size_t pos = output.size();
                        output.push_back(output[pos - dist]);
                    }
                }
                else {
                    throw std::runtime_error("Invalid symbol");
                }
            }

            br.ReadByteAlign(); // 次ブロックのための境界合わせ
        }
        else {
            throw std::runtime_error("Only fixed Huffman blocks are supported");
        }
    }

    return output;
}
