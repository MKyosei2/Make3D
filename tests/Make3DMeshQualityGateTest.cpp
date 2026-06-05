#include "Make3DMeshQualityGate.h"

#include <iostream>

namespace {

static void AddVertex(make3d::MeshData& mesh, float x, float y, float z) {
    mesh.positions.push_back(x);
    mesh.positions.push_back(y);
    mesh.positions.push_back(z);
    mesh.normals.push_back(0.0f);
    mesh.normals.push_back(1.0f);
    mesh.normals.push_back(0.0f);
    mesh.uvs.push_back(0.0f);
    mesh.uvs.push_back(0.0f);
}

static void AddTri(make3d::MeshData& mesh, unsigned a, unsigned b, unsigned c) {
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
}

static make3d::MeshData StableBox() {
    make3d::MeshData mesh;
    AddVertex(mesh, -0.5f, 0.0f, -0.5f);
    AddVertex(mesh,  0.5f, 0.0f, -0.5f);
    AddVertex(mesh,  0.5f, 1.0f, -0.5f);
    AddVertex(mesh, -0.5f, 1.0f, -0.5f);
    AddVertex(mesh, -0.5f, 0.0f,  0.5f);
    AddVertex(mesh,  0.5f, 0.0f,  0.5f);
    AddVertex(mesh,  0.5f, 1.0f,  0.5f);
    AddVertex(mesh, -0.5f, 1.0f,  0.5f);
    AddTri(mesh, 0, 1, 2); AddTri(mesh, 0, 2, 3);
    AddTri(mesh, 5, 4, 7); AddTri(mesh, 5, 7, 6);
    AddTri(mesh, 4, 0, 3); AddTri(mesh, 4, 3, 7);
    AddTri(mesh, 1, 5, 6); AddTri(mesh, 1, 6, 2);
    AddTri(mesh, 3, 2, 6); AddTri(mesh, 3, 6, 7);
    AddTri(mesh, 4, 5, 1); AddTri(mesh, 4, 1, 0);
    return mesh;
}

static make3d::MeshData SpikyMesh() {
    make3d::MeshData mesh;
    AddVertex(mesh, 0.0f, 0.0f, 0.0f);
    AddVertex(mesh, 0.01f, 0.0f, 0.0f);
    AddVertex(mesh, 50.0f, 0.001f, 0.0f);
    AddVertex(mesh, 0.0f, 1.0f, 0.0f);
    AddVertex(mesh, 0.01f, 1.0f, 0.0f);
    AddVertex(mesh, 50.0f, 1.001f, 0.0f);
    AddTri(mesh, 0, 1, 2);
    AddTri(mesh, 3, 4, 5);
    AddTri(mesh, 0, 3, 5);
    AddTri(mesh, 0, 5, 2);
    return mesh;
}

} // namespace

int main() {
    make3d::MeshQualityGateOptions options;
    options.maxEdgeToBoundsDiagonal = 0.95f;
    options.maxTriangleAspectRatio = 40.0f;
    options.maxLongEdgeTriangleRatio = 0.60f;
    options.maxThinTriangleRatio = 0.20f;
    options.maxSpikeTriangleRatio = 0.0f;

    auto stable = make3d::AnalyzeMeshQualityGate(StableBox(), options);
    if (!stable.ok) {
        std::cerr << "Stable box unexpectedly failed quality gate.\n" << stable.ToMarkdown() << "\n";
        return 1;
    }

    auto spiky = make3d::AnalyzeMeshQualityGate(SpikyMesh(), options);
    if (spiky.ok) {
        std::cerr << "Spiky mesh unexpectedly passed quality gate.\n" << spiky.ToMarkdown() << "\n";
        return 2;
    }
    if (spiky.spikeLikeTriangles <= 0 && spiky.thinTriangles <= 0) {
        std::cerr << "Spiky mesh was not classified as spike/thin output.\n" << spiky.ToMarkdown() << "\n";
        return 3;
    }

    return 0;
}
