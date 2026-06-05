#pragma once

#include "Make3DAdvancedCore.h"

#include <string>
#include <vector>

namespace make3d {

struct MeshQualityGateOptions {
    float maxEdgeToBoundsDiagonal = 1.05f;
    float maxTriangleAspectRatio = 65.0f;
    float maxLongEdgeTriangleRatio = 0.50f;
    float maxThinTriangleRatio = 0.08f;
    float maxSpikeTriangleRatio = 0.01f;
    int minimumTriangles = 4;
};

struct MeshQualityGateReport {
    bool ok = false;
    int vertices = 0;
    int triangles = 0;
    int longEdgeTriangles = 0;
    int thinTriangles = 0;
    int spikeLikeTriangles = 0;
    float boundsDiagonal = 0.0f;
    float maxEdgeLength = 0.0f;
    float maxTriangleAspectRatio = 0.0f;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

MeshQualityGateReport AnalyzeMeshQualityGate(
    const MeshData& mesh,
    const MeshQualityGateOptions& options = MeshQualityGateOptions{});

} // namespace make3d
