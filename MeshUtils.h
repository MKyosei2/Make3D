#pragma once
#include "common.h"
#include "GUIState.h"

inline void AppendMesh(Mesh3D& dst, const Mesh3D& src) {
    int offset = (int)dst.vertices.size();
    dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());
    for (int idx : src.indices) dst.indices.push_back(offset + idx);
}

inline void NormalizeMesh(Mesh3D& mesh, ExportScaleUnit unit) {
    if (mesh.vertices.empty()) return;
    Vec3 min = mesh.vertices[0], max = mesh.vertices[0];
    for (auto& v : mesh.vertices) {
        min.x = std::min(min.x, v.x); max.x = std::max(max.x, v.x);
        min.y = std::min(min.y, v.y); max.y = std::max(max.y, v.y);
        min.z = std::min(min.z, v.z); max.z = std::max(max.z, v.z);
    }
    Vec3 center = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    float size = std::max({ max.x - min.x, max.y - min.y, max.z - min.z });
    float target = (unit == ExportScaleUnit::UnityMeters) ? 1.0f : 0.01f;
    for (auto& v : mesh.vertices) {
        v.x = ((v.x - center.x) / size) * target;
        v.y = ((v.y - center.y) / size) * target;
        v.z = ((v.z - center.z) / size) * target;
    }
}
