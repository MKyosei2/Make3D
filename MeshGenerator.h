#pragma once
#include <vector>
#include "VolumeBuilder.h" // VolumeData ‚ðŽg‚¤‚½‚ß

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
    Mesh simplifyMesh(const Mesh& mesh, int maxPolygons);

private:
    int targetPolygonCount = 1000;
};
