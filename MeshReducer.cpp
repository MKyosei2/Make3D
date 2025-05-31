#include "MeshReducer.h"
#include <unordered_map>

Mesh3D ReduceMesh(const Mesh3D& mesh, int targetPolygonCount) {
    Mesh3D result;

    if ((int)mesh.indices.size() / 3 <= targetPolygonCount) {
        return mesh; // Šù‚É–Ú•WˆÈ‰º‚È‚ç‚»‚Ì‚Ü‚Ü•Ô‚·
    }

    std::unordered_map<size_t, int> vertexMap;
    auto hashVertex = [](const Vertex& v) {
        return std::hash<float>()(v.x) ^ std::hash<float>()(v.y) << 1 ^ std::hash<float>()(v.z) << 2;
        };

    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        if ((int)result.indices.size() / 3 >= targetPolygonCount) break;

        int tri[3];
        for (int j = 0; j < 3; ++j) {
            Vertex v = mesh.vertices[mesh.indices[i + j]];
            size_t hv = hashVertex(v);
            auto it = vertexMap.find(hv);
            if (it == vertexMap.end()) {
                int idx = result.vertices.size();
                result.vertices.push_back(v);
                vertexMap[hv] = idx;
                tri[j] = idx;
            }
            else {
                tri[j] = it->second;
            }
        }

        result.indices.push_back(tri[0]);
        result.indices.push_back(tri[1]);
        result.indices.push_back(tri[2]);
    }

    return result;
}
