#include "common.h"
#include "MeshBuilder.h"

Mesh3D BuildMeshFromVolume(const Volume& vol) {
    Mesh3D mesh;
    for (int z = 0; z < vol.depth; ++z) {
        for (int y = 0; y < vol.height; ++y) {
            for (int x = 0; x < vol.width; ++x) {
                if (vol.At(x, y, z)) {
                    Vertex v = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
                    mesh.vertices.push_back(v);
                }
            }
        }
    }
    // インデックスは仮：点だけの場合三角形は無し
    return mesh;
}
