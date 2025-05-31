#include "MeshBuilder.h"
#include "MarchingCubesTables.h"

static float isoLevel = 0.5f;

static Vertex Interpolate(const Vec3& p1, const Vec3& p2) {
    return { (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f, (p1.z + p2.z) * 0.5f };
}

Mesh3D BuildMeshFromVolume(const Volume& vol) {
    Mesh3D mesh;
    const int DX[8] = { 0,1,1,0,0,1,1,0 };
    const int DY[8] = { 0,0,1,1,0,0,1,1 };
    const int DZ[8] = { 0,0,0,0,1,1,1,1 };

    for (int z = 0; z < vol.depth - 1; ++z) {
        for (int y = 0; y < vol.height - 1; ++y) {
            for (int x = 0; x < vol.width - 1; ++x) {
                int cubeIndex = 0;
                float values[8];
                Vec3 points[8];
                for (int i = 0; i < 8; ++i) {
                    int xi = x + DX[i];
                    int yi = y + DY[i];
                    int zi = z + DZ[i];
                    points[i] = { (float)xi, (float)yi, (float)zi };
                    values[i] = vol.get(xi, yi, zi) ? 1.0f : 0.0f;
                    if (values[i] > isoLevel) cubeIndex |= (1 << i);
                }

                int* tri = MC_TRI_TABLE[cubeIndex];
                for (int i = 0; tri[i] != -1; i += 3) {
                    Vertex v0 = Interpolate(points[tri[i]], points[tri[i + 1]]);
                    Vertex v1 = Interpolate(points[tri[i + 1]], points[tri[i + 2]]);
                    Vertex v2 = Interpolate(points[tri[i + 2]], points[tri[i]]);

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
