#include "PartProcessor.h"

std::vector<MeshPart> PartProcessor::splitMeshIntoParts(const Mesh& mesh) {
    // 仮実装：全体を一つのパーツとして返す
    MeshPart part;
    part.mesh = mesh;
    part.type = classifyPart(mesh);
    part.name = L"MainPart";
    return { part };
}

PartType PartProcessor::classifyPart(const Mesh& mesh) {
    // 非常に単純な分類処理（将来的に体積や頂点分布で改善）
    size_t v = mesh.vertices.size();
    if (v < 100) return PartType::Cube;
    if (v < 300) return PartType::Cylinder;
    if (v < 600) return PartType::Sphere;
    return PartType::Custom;
}
