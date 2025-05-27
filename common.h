#pragma once
#include <vector>

struct Image2D {
    int width, height;
    std::vector<unsigned char> pixels; // RGBA or grayscale
};

struct Volume {
    int width, height, depth;
    std::vector<unsigned char> voxels; // 3Dボクセル濃度
};

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};
