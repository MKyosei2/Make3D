#pragma once

#include "Make3DAdvancedCore.h"
#include "Make3DMeshTools.h"

#include <string>
#include <vector>

namespace make3d {

struct MeshPolishOptions {
    int smoothingIterations = 8;
    float smoothingStrength = 0.35f;
    float preserveBoundaryStrength = 0.15f;
    int minComponentTriangles = 16;
    bool keepLargestComponentOnly = true;
    bool cleanupBeforePolish = true;
    bool cleanupAfterPolish = true;
};

struct MeshPolishReport {
    int verticesBefore = 0;
    int verticesAfter = 0;
    int trianglesBefore = 0;
    int trianglesAfter = 0;
    int connectedComponentsBefore = 0;
    int connectedComponentsAfter = 0;
    int removedComponents = 0;
    int removedTriangles = 0;
    int smoothingIterations = 0;
    bool validForExport = false;
    std::vector<std::string> warnings;

    std::string ToMarkdown() const;
    std::string ToJson() const;
};

MeshPolishReport PolishMesh(MeshData& mesh, const MeshPolishOptions& options = MeshPolishOptions{});

} // namespace make3d
