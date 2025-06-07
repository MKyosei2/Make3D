#include "MeshUtils.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>

static int addMidpoint(std::unordered_map<uint64_t, int>& cache,
    std::vector<Vertex>& vertices,
    int i0, int i1,
    const Vertex& v0, const Vertex& v1) {
    uint64_t key = ((uint64_t)std::min(i0, i1) << 32) | std::max(i0, i1);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    Vertex mid = {
        (v0.x + v1.x) * 0.5f,
        (v0.y + v1.y) * 0.5f,
        (v0.z + v1.z) * 0.5f
    };
    int newIndex = (int)vertices.size();
    vertices.push_back(mid);
    cache[key] = newIndex;
    return newIndex;
}

Mesh subdivideMesh(const Mesh& input, int iterations) {
    Mesh current = input;
    for (int iter = 0; iter < iterations; ++iter) {
        std::unordered_map<uint64_t, int> cache;
        std::vector<Triangle> newTris;

        for (auto& tri : current.triangles) {
            int a = tri.v0, b = tri.v1, c = tri.v2;
            int ab = addMidpoint(cache, current.vertices, a, b, current.vertices[a], current.vertices[b]);
            int bc = addMidpoint(cache, current.vertices, b, c, current.vertices[b], current.vertices[c]);
            int ca = addMidpoint(cache, current.vertices, c, a, current.vertices[c], current.vertices[a]);

            newTris.push_back({ a, ab, ca });
            newTris.push_back({ b, bc, ab });
            newTris.push_back({ c, ca, bc });
            newTris.push_back({ ab, bc, ca });
        }

        current.triangles = std::move(newTris);
    }
    return current;
}

void smoothMesh(Mesh& mesh, int iterations) {
    for (int it = 0; it < iterations; ++it) {
        std::vector<Vertex> newVerts = mesh.vertices;
        std::vector<int> counts(mesh.vertices.size(), 0);

        for (auto& tri : mesh.triangles) {
            int v[3] = { tri.v0, tri.v1, tri.v2 };
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    if (i == j) continue;
                    newVerts[v[i]].x += mesh.vertices[v[j]].x;
                    newVerts[v[i]].y += mesh.vertices[v[j]].y;
                    newVerts[v[i]].z += mesh.vertices[v[j]].z;
                    counts[v[i]]++;
                }
            }
        }

        for (size_t i = 0; i < newVerts.size(); ++i) {
            if (counts[i] > 0) {
                newVerts[i].x /= (counts[i] + 1);
                newVerts[i].y /= (counts[i] + 1);
                newVerts[i].z /= (counts[i] + 1);
            }
        }

        mesh.vertices = std::move(newVerts);
    }
}
