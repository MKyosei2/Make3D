#pragma once
#include <vector>
#include <map>

// 汎用ベクトル
struct Vec3 {
    float x, y, z;
};

struct Vertex {
    float x, y, z;
};

// Image2D: PNG画像構造
struct Image2D {
    int width;
    int height;
    std::vector<unsigned char> pixels; // RGBA
    bool IsOpaque(int x, int y) const {
        int idx = (y * width + x) * 4;
        return (idx + 3 < pixels.size()) ? pixels[idx + 3] > 128 : false;
    }
};

// Volume3D: ボクセル構造
struct Volume3D {
    int width = 0, height = 0, depth = 0;
    std::vector<bool> data;
    void Resize(int w, int h, int d) {
        width = w; height = h; depth = d;
        data.resize(w * h * d);
    }
    void Set(int x, int y, int z, bool val) {
        data[x + y * width + z * width * height] = val;
    }
    bool At(int x, int y, int z) const {
        return data[x + y * width + z * width * height];
    }
};

// メッシュ構造体
struct Mesh3D {
    std::vector<Vec3> vertices;
    std::vector<unsigned int> indices;
};

// パーツ種別
enum class PartType {
    Body,
    Head,
    Arm,
    Leg,
    Tail,
    Other
};

// 視点種別
enum class ViewType {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom
};

// FBX出力単位
enum class ExportScaleUnit {
    Centimeter,
    Meter
};