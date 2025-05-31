#pragma once
#include <vector>

struct Vertex {
    float x, y, z;
};

struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};