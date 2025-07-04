#include "MeshGenerator.h"
#include "VolumeBuilder.h"
#include <array>
#include <cmath>

extern const int edgeTable[256];
extern const int triTable[256][16];

void MeshGenerator::setTargetPolygonCount(int count) {
    targetPolygonCount = count;
}

Vertex interpolate(const Vertex& p1, const Vertex& p2) {
    return {
        (p1.x + p2.x) * 0.5f,
        (p1.y + p2.y) * 0.5f,
        (p1.z + p2.z) * 0.5f
    };
}

Mesh MeshGenerator::generate(const VolumeData& volume) {
    Mesh mesh;

    const int w = volume.width;
    const int h = volume.height;
    const int d = volume.depth;

    const std::array<std::array<int, 3>, 8> cornerOffsets = { {
        {{0, 0, 0}}, {{1, 0, 0}}, {{1, 1, 0}}, {{0, 1, 0}},
        {{0, 0, 1}}, {{1, 0, 1}}, {{1, 1, 1}}, {{0, 1, 1}}
    } };

    const std::array<std::pair<int, int>, 12> edgeConnections = { {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    } };

    for (int z = 0; z < d - 1; ++z) {
        for (int y = 0; y < h - 1; ++y) {
            for (int x = 0; x < w - 1; ++x) {
                int cubeIndex = 0;
                bool cube[8];
                Vertex positions[8];

                for (int i = 0; i < 8; ++i) {
                    int dx = cornerOffsets[i][0];
                    int dy = cornerOffsets[i][1];
                    int dz = cornerOffsets[i][2];
                    cube[i] = volume.get(x + dx, y + dy, z + dz);
                    if (cube[i]) cubeIndex |= (1 << i);
                    positions[i] = { (float)(x + dx), (float)(y + dy), (float)(z + dz) };
                }

                if (edgeTable[cubeIndex] == 0) continue;

                std::array<Vertex, 12> vertList;
                for (int i = 0; i < 12; ++i) {
                    if (edgeTable[cubeIndex] & (1 << i)) {
                        int v1 = edgeConnections[i].first;
                        int v2 = edgeConnections[i].second;
                        vertList[i] = interpolate(positions[v1], positions[v2]);
                    }
                }

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