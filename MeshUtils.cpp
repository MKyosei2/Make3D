#include "MeshUtils.h"

// 単純なメッシュ生成（仮実装）
Mesh3D BuildMeshFromVolume(const Volume& vol) {
    Mesh3D mesh;
    int w = vol.width;
    int h = vol.height;
    int d = vol.depth;

    // 各ボクセルを立方体として追加（最小構成）
    for (int z = 0; z < d; ++z) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (vol.At(x, y, z)) {
                    // 原点に1頂点（簡略化）
                    mesh.vertices.push_back({ float(x), float(y), float(z) });
                    mesh.triangles.push_back((int)mesh.vertices.size() - 1);
                }
            }
        }
    }
    return mesh;
}

// ポリゴン数の削減（ランダム間引き）
Mesh3D ReduceMesh(const Mesh3D& mesh, int targetTriangles) {
    Mesh3D result;
    int step = std::max(1, (int)mesh.triangles.size() / std::max(1, targetTriangles));

    for (size_t i = 0; i < mesh.triangles.size(); i += step) {
        int idx = mesh.triangles[i];
        result.triangles.push_back((int)result.vertices.size());
        result.vertices.push_back(mesh.vertices[idx]);
    }
    return result;
}