#pragma once
#include <vector>
#include "VolumeBuilder.h"

struct Vertex { float x, y, z; };
struct Triangle { int v0, v1, v2; };
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

class MeshGenerator {
public:
    void setTargetPolygonCount(int count);
    Mesh generate(const VolumeData& volume);

private:
    int targetPolygonCount = 1000;
};
