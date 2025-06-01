#pragma once
#include <vector>
#include <cstdint>

struct Volume3D {
    int width = 0;
    int height = 0;
    int depth = 0;
    std::vector<uint8_t> voxels;

    Volume3D() = default;
    Volume3D(int w, int h, int d) {
        Resize(w, h, d);
    }

    void Resize(int w, int h, int d) {
        width = w;
        height = h;
        depth = d;
        voxels.resize(w * h * d, 0);
    }

    // 書き込み用
    uint8_t& At(int x, int y, int z) {
        return voxels[(z * height + y) * width + x];
    }

    // 読み取り専用（const 対応）
    const uint8_t& At(int x, int y, int z) const {
        return voxels[(z * height + y) * width + x];
    }
};
