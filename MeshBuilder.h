#pragma once
#include "common.h"
#include "VolumeBuilder.h"

struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};

Mesh3D BuildMeshFromVolume(const Volume& vol);