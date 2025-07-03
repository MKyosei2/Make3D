#pragma once
#include <string>
#include <vector>
#include "MeshGenerator.h"

enum class PartType {
    Undefined, Cube, Cylinder, Sphere, Custom
};

struct MeshPart {
    Mesh mesh;
    PartType type;
    std::wstring name;
};

class PartProcessor {
public:
    static std::vector<MeshPart> splitMeshIntoParts(const Mesh& mesh);
    static PartType classifyPart(const Mesh& mesh);
};