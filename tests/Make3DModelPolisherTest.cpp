#include "Make3DModelPolisher.h"

#include <iostream>

static make3d::MeshData BuildMeshWithSmallIsland() {
    make3d::MeshData mesh;
    mesh.positions = {
        0, 0, 0,
        1, 0, 0,
        1, 1, 0,
        0, 1, 0,
        10, 10, 0,
        11, 10, 0,
        10, 11, 0
    };
    mesh.normals.assign(7 * 3, 0.0f);
    for (int i = 0; i < 7; ++i) mesh.normals[static_cast<size_t>(i) * 3 + 2] = 1.0f;
    mesh.uvs.assign(7 * 2, 0.0f);
    mesh.indices = {
        0, 1, 2,
        0, 2, 3,
        4, 5, 6
    };
    return mesh;
}

static int Fail(const char* message) {
    std::cerr << "[FAIL] " << message << "\n";
    return 1;
}

int main() {
    make3d::MeshData mesh = BuildMeshWithSmallIsland();
    make3d::MeshPolishOptions options;
    options.keepLargestComponentOnly = true;
    options.smoothingIterations = 2;
    options.smoothingStrength = 0.1f;
    make3d::MeshPolishReport report = make3d::PolishMesh(mesh, options);

    if (report.trianglesBefore != 3) return Fail("unexpected triangle count before polish");
    if (report.trianglesAfter != 2) return Fail("small island was not removed");
    if (report.connectedComponentsBefore < 2) return Fail("components were not detected");
    if (report.connectedComponentsAfter != 1) return Fail("expected a single remaining component");
    if (!report.validForExport) return Fail("polished mesh should be valid for export");

    std::cout << "[PASS] Make3D model polisher test\n";
    std::cout << report.ToMarkdown() << "\n";
    return 0;
}
