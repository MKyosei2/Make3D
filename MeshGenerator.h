#pragma once
#include <vector>
#include "VolumeUtils.h"
#include "MeshUtils.h"

struct XYZ {
    float x, y, z;
};

struct TRIANGLE {
    XYZ p[3];
};

struct GRIDCELL {
    XYZ p[8];
    double val[8];
};

struct Cube {
    float value[8];
    XYZ position[8];
};

std::vector<Triangle> GenerateMeshWithMarchingCubes(const Volume& volume, float isoLevel);
class MeshGenerator {
public:
    MeshGenerator();

    void setTargetPolygonCount(int count);
    Mesh generate(const VolumeData& volume);

private:
    int targetPolygonCount;
};