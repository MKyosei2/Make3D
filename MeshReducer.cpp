#include "common.h"
#include "MeshReducer.h"

Mesh3D ReduceMesh(const Mesh3D& input, int maxVertices) {
    Mesh3D result;
    int step = std::max(1, static_cast<int>(input.vertices.size() / maxVertices));
    for (size_t i = 0; i < input.vertices.size(); i += step) {
        result.vertices.push_back(input.vertices[i]);
    }
    // ポリゴンは削除（点のみ）
    return result;
}