#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

// グローバル列挙型: 複数視点の定義
enum class ViewDirection {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom,
    Count
};

// ボクセルボリュームデータ構造
class VolumeData {
public:
    VolumeData(int x, int y, int z)
        : sizeX(x), sizeY(y), sizeZ(z), data(x* y* z, false) {
    }

    VolumeData toVolumeData() const {
        return VolumeData(*this);
    }

    bool get(int x, int y, int z) const {
        return data[(z * sizeY + y) * sizeX + x];
    }

    void set(int x, int y, int z, bool value) {
        data[(z * sizeY + y) * sizeX + x] = value;
    }

    int getSizeX() const { return sizeX; }
    int getSizeY() const { return sizeY; }
    int getSizeZ() const { return sizeZ; }

private:
    int sizeX, sizeY, sizeZ;
    std::vector<bool> data;
};

// ユーティリティ関数
inline float clamp(float x, float a, float b) {
    return (x < a) ? a : (x > b) ? b : x;
}

struct Volume {
    int width, height, depth;
    std::vector<bool> voxels;
};