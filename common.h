#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Vertex {
    float x, y, z;
};

struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};

struct Image2D {
    int width = 0, height = 0;
    std::vector<uint8_t> pixels; // RGBA (4バイト/ピクセル)

    bool IsOpaque(int x, int y, uint8_t threshold = 128) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return false;
        return pixels[(y * width + x) * 4 + 3] > threshold; // Aチャンネル
    }
};

struct Volume {
    int width = 0, height = 0, depth = 0;
    std::vector<uint8_t> voxels;

    void Resize(int w, int h, int d) {
        width = w;
        height = h;
        depth = d;
        voxels.resize(w * h * d);
    }

    uint8_t& At(int x, int y, int z) {
        return voxels[(z * height + y) * width + x];
    }

    const uint8_t& At(int x, int y, int z) const {
        return voxels[(z * height + y) * width + x];
    }
};

using Volume3D = Volume;