#pragma once
#include "common.h"

struct Vertex {
    float x, y, z;
};

struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};

Mesh3D GenerateMeshFromImage(const Image2D& image, int polygonCount);
