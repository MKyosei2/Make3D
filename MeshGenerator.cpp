#include "MeshGenerator.h"
#include "MeshUtils.h"
#include "VolumeUtils.h"

MeshGenerator::MeshGenerator() : targetPolygonCount(5000) {}

void MeshGenerator::setTargetPolygonCount(int count) {
    targetPolygonCount = count;
}

MeshData* MeshGenerator::generate(const VolumeData* volume) {
    if (!volume) return nullptr;

    MeshData* mesh = new MeshData();

    // マーチングキューブでメッシュを生成
    generateMeshFromVolume(*volume, *mesh, targetPolygonCount);

    return mesh;
}
