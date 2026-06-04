#include "Make3DMeshTools.h"

#include <iostream>

static make3d::MeshData BuildDirtyQuad() {
    make3d::MeshData mesh;
    mesh.positions = {
        0, 0, 0,
        1, 0, 0,
        1, 1, 0,
        0, 1, 0,
        5, 5, 5
    };
    mesh.normals = {
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 0, 1,
        0, 1, 0
    };
    mesh.uvs = {
        0, 0,
        1, 0,
        1, 1,
        0, 1,
        0, 0
    };
    mesh.indices = {
        0, 1, 2,
        0, 2, 3,
        0, 0, 1,
        0, 1, 99
    };
    return mesh;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::MeshData mesh = BuildDirtyQuad();
    make3d::MeshValidationReport before = make3d::ValidateMesh(mesh);
    if (before.verticesBefore != 5) return Fail("unexpected initial vertex count");
    if (before.trianglesBefore != 4) return Fail("unexpected initial triangle count");
    if (before.invalidTriangles == 0) return Fail("invalid triangle was not detected");
    if (before.degenerateTriangles == 0) return Fail("degenerate triangle was not detected");

    make3d::MeshCleanupOptions options;
    make3d::MeshValidationReport after = make3d::CleanupMesh(mesh, options);
    if (after.verticesAfter != 4) return Fail("unused vertex was not removed");
    if (after.trianglesAfter != 2) return Fail("invalid/degenerate triangles were not removed");
    if (!after.validForExport) return Fail("cleaned mesh should be valid for export");
    if (mesh.positions.size() / 3 != 4) return Fail("mesh was not compacted");
    if (mesh.indices.size() / 3 != 2) return Fail("mesh triangles were not compacted");

    std::cout << "[PASS] Make3D mesh tools test\n";
    std::cout << after.ToMarkdown() << "\n";
    return 0;
}
