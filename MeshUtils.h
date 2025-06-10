#pragma once
#include <vector>

struct Vertex {
    float x, y, z;
};

struct Triangle {
    int v0, v1, v2;
    Vertex vertices[3];
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

Mesh subdivideMesh(const Mesh& inputMesh, int iterations);
void smoothMesh(Mesh& mesh, int iterations = 1);
