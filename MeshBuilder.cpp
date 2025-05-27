#include "MeshBuilder.h"
#include "MarchingCubesTables.h"
#include <cstdlib>

static float isolevel = 127.5f;

Vertex interpolateVertex(int x, int y, int z, int edge, const Volume& vol) {
    // 簡略：エッジに関係なく中心補間位置を返す（今後改善可）
    return Vertex{ x + 0.5f, y + 0.5f, z + 0.5f, 0, 0, 1, 0, 0 };
}

Mesh3D GenerateMesh(const Volume& vol, int targetPolygons) {
    Mesh3D fullMesh;
    std::vector<int> cubeTriStart; // 各cubeが何個三角形を生成するか
    std::vector<std::tuple<int, int, int>> cubePos;

    for (int z = 0; z < vol.depth - 1; ++z) {
        for (int y = 0; y < vol.height - 1; ++y) {
            for (int x = 0; x < vol.width - 1; ++x) {
                int cubeIndex = 0;
                for (int i = 0; i < 8; ++i) {
                    int vx = x + DX[i], vy = y + DY[i], vz = z + DZ[i];
                    int idx = vz * vol.width * vol.height + vy * vol.width + vx;
                    if (vol.voxels[idx] > isolevel) cubeIndex |= (1 << i);
                }
                if (cubeIndex == 0 || cubeIndex == 255) continue;

                int triCount = 0;
                for (int i = 0; MC_TRI_TABLE[cubeIndex][i] != -1; i += 3) ++triCount;

                cubeTriStart.push_back((int)fullMesh.indices.size()); // 開始インデックス
                cubePos.emplace_back(x, y, z);

                for (int i = 0; MC_TRI_TABLE[cubeIndex][i] != -1; i += 3) {
                    Vertex v0 = interpolateVertex(x, y, z, MC_TRI_TABLE[cubeIndex][i], vol);
                    Vertex v1 = interpolateVertex(x, y, z, MC_TRI_TABLE[cubeIndex][i + 1], vol);
                    Vertex v2 = interpolateVertex(x, y, z, MC_TRI_TABLE[cubeIndex][i + 2], vol);
                    int base = (int)fullMesh.vertices.size();
                    fullMesh.vertices.push_back(v0);
                    fullMesh.vertices.push_back(v1);
                    fullMesh.vertices.push_back(v2);
                    fullMesh.indices.push_back(base);
                    fullMesh.indices.push_back(base + 1);
                    fullMesh.indices.push_back(base + 2);
                }
            }
        }
    }

    // 現在の三角形数
    int totalTriangles = (int)(fullMesh.indices.size() / 3);
    if (totalTriangles <= targetPolygons)
        return fullMesh;

    // 抽選削減：一部の三角形のみ取り出して軽量化
    Mesh3D reduced;
    int keepEvery = totalTriangles / targetPolygons + 1;
    for (int i = 0; i < totalTriangles; ++i) {
        if (i % keepEvery == 0 && reduced.indices.size() / 3 < targetPolygons) {
            int idx = i * 3;
            int base = (int)reduced.vertices.size();
            reduced.vertices.push_back(fullMesh.vertices[fullMesh.indices[idx]]);
            reduced.vertices.push_back(fullMesh.vertices[fullMesh.indices[idx + 1]]);
            reduced.vertices.push_back(fullMesh.vertices[fullMesh.indices[idx + 2]]);
            reduced.indices.push_back(base);
            reduced.indices.push_back(base + 1);
            reduced.indices.push_back(base + 2);
        }
    }

    return reduced;
}
