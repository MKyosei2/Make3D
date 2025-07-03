#include "MeshGenerator.h"
#include "VolumeBuilder.h"
#include <array>
#include <cmath>

// Marching Cubes テーブル定義（ここでは簡略化。完全版は省略）
static const int edgeTable[256] = { /* ... */ };
static const int triTable[256][16] = { /* ... */ };

void MeshGenerator::setTargetPolygonCount(int count) {
    targetPolygonCount = count;
}

Mesh MeshGenerator::generate(const VolumeData& volume) {
    Mesh mesh;

    int w = volume.width;
    int h = volume.height;
    int d = volume.depth;

    for (int z = 0; z < d - 1; ++z) {
        for (int y = 0; y < h - 1; ++y) {
            for (int x = 0; x < w - 1; ++x) {
                int cubeIndex = 0;
                bool cube[8];
                for (int i = 0; i < 8; ++i) {
                    int dx = i & 1;
                    int dy = (i >> 1) & 1;
                    int dz = (i >> 2) & 1;
                    cube[i] = volume.get(x + dx, y + dy, z + dz);
                    if (cube[i]) cubeIndex |= (1 << i);
                }

                if (edgeTable[cubeIndex] == 0) continue;

                std::array<Vertex, 12> vertList;
                // vertList の補完（ここでは省略。必要に応じて線形補間）

                for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
                    int idx0 = triTable[cubeIndex][i];
                    int idx1 = triTable[cubeIndex][i + 1];
                    int idx2 = triTable[cubeIndex][i + 2];

                    Vertex v0 = vertList[idx0];
                    Vertex v1 = vertList[idx1];
                    Vertex v2 = vertList[idx2];

                    int baseIndex = static_cast<int>(mesh.vertices.size());
                    mesh.vertices.push_back(v0);
                    mesh.vertices.push_back(v1);
                    mesh.vertices.push_back(v2);

                    mesh.triangles.push_back({ baseIndex, baseIndex + 1, baseIndex + 2 });
                }
            }
        }
    }

    return mesh;
}
