#pragma once
#include <map>
#include <vector>
#include <string>

enum class PartType { Head, Body, Arm, Leg, Tail, Wing, Unknown };
enum class ViewType { Front, Back, Left, Right, Top, Bottom };

struct GUIState {
    std::map<PartType, std::map<ViewType, std::vector<std::string>>> partViewImages;
};

struct Vertex { float x, y, z; };
struct Mesh3D {
    std::vector<Vertex> vertices;
    std::vector<int> indices;
};

inline void AppendMesh(Mesh3D& dst, const Mesh3D& src) {
    int offset = (int)dst.vertices.size();
    dst.vertices.insert(dst.vertices.end(), src.vertices.begin(), src.vertices.end());
    for (int idx : src.indices) dst.indices.push_back(offset + idx);
}
