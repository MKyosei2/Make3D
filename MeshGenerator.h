#pragma once
#include "VolumeUtils.h"
#include "MeshUtils.h"

class MeshGenerator {
public:
    MeshGenerator();

    void setTargetPolygonCount(int count);
    Mesh generate(const VolumeData& volume);

private:
    int targetPolygonCount;
};
