#include "MeshBuilder.h"
#include "MarchingCubesTables.h"

static float isolevel = 127.5f;

Vertex interpolateVertex(int x, int y, int z, int edge, const Volume& vol) {
    return Vertex{ x + 0.5f, y + 0.5f, z + 0.5f, 0, 0, 1, 0, 0 };
}

Mesh3D GenerateMesh(const Volume& vol, int targetPolygons) {
    Mesh3D mesh;
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

                for (int i = 0; MC_TRI_TABLE[cubeIndex][i] != -1; i += 3) {
                    Vertex v0 = interpolateVertex(x, y, z, MC_TRI_TABLE[cubeIndex][i], vol);
                    Vertex v1 = interpolateVertex(x, y, z, MC_TRI_TABLE[cubeIndex][i + 1], vol);
                    Vertex v2 = interpolateVertex(x, y, z, MC_TRI_TABLE[cubeIndex][i + 2], vol);
                    int base = mesh.vertices.size();
                    mesh.vertices.push_back(v0);
                    mesh.vertices.push_back(v1);
                    mesh.vertices.push_back(v2);
                    mesh.indices.push_back(base);
                    mesh.indices.push_back(base + 1);
                    mesh.indices.push_back(base + 2);
                }
            }
        }
    }
    return mesh;
}
