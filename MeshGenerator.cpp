#include "MeshGenerator.h"
Mesh3D GenerateMeshFromVolume(const Volume3D& vol, int polygonCount) {
    Mesh3D mesh;
    mesh.vertices.push_back({ 0, 0, 0 });
    mesh.vertices.push_back({ 1, 0, 0 });
    mesh.vertices.push_back({ 0, 1, 0 });
    mesh.indices = { 0, 1, 2 };
    return mesh;
}