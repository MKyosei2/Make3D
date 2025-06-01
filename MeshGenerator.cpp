#include "MeshGenerator.h"
#include <algorithm>

Mesh3D GenerateMeshFromImage(const Image2D& img, int polygonCount) {
    Mesh3D mesh;
    int step = std::max(1, (img.width * img.height) / polygonCount);

    for (int y = 0; y < img.height - step; y += step) {
        for (int x = 0; x < img.width - step; x += step) {
            if (!img.IsOpaque(x, y)) continue;

            int i0 = (int)mesh.vertices.size();
            mesh.vertices.push_back({ (float)x, (float)y, 0.0f });
            mesh.vertices.push_back({ (float)(x + step), (float)y, 0.0f });
            mesh.vertices.push_back({ (float)x, (float)(y + step), 0.0f });
            mesh.vertices.push_back({ (float)(x + step), (float)(y + step), 0.0f });

            mesh.indices.push_back(i0);
            mesh.indices.push_back(i0 + 1);
            mesh.indices.push_back(i0 + 2);
            mesh.indices.push_back(i0 + 2);
            mesh.indices.push_back(i0 + 1);
            mesh.indices.push_back(i0 + 3);
        }
    }

    return mesh;
}
