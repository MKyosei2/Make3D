#pragma once

#include "Make3DAdvancedCore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace make3d {

struct MeshCleanupOptions {
    float positionEpsilon = 0.0001f;
    float normalEpsilon = 0.001f;
    float uvEpsilon = 0.0001f;
    bool mergeDuplicateVertices = true;
    bool removeDegenerateTriangles = true;
    bool removeInvalidTriangles = true;
    bool compactUnusedVertices = true;
};

struct MeshValidationReport {
    int verticesBefore = 0;
    int verticesAfter = 0;
    int trianglesBefore = 0;
    int trianglesAfter = 0;
    int invalidTriangles = 0;
    int degenerateTriangles = 0;
    int duplicateVerticesMerged = 0;
    int unusedVerticesRemoved = 0;
    int boundaryEdges = 0;
    int nonManifoldEdges = 0;
    float surfaceArea = 0.0f;
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    float maxZ = 0.0f;
    bool validForExport = false;
    bool watertightCandidate = false;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

MeshValidationReport ValidateMesh(const MeshData& mesh);
MeshValidationReport CleanupMesh(MeshData& mesh, const MeshCleanupOptions& options = MeshCleanupOptions{});

} // namespace make3d
