#pragma once

#include <vector>

// 単純な3Dボクセル構造（bool格納）
class VolumeData {
public:
    VolumeData(int x, int y, int z)
        : sizeX(x), sizeY(y), sizeZ(z), data(x* y* z, false) {
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
