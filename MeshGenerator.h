#pragma once
#include <vector>
#include <array>

class MeshGenerator
{
public:
    struct Vertex {
        float x, y, z;
    };

    bool GenerateMesh(
        const std::vector<unsigned char>& volume,
        int width, int height, int depth,
        std::vector<Vertex>& outVertices,
        std::vector<unsigned int>& outIndices,
        int& outCubeCount);
};
