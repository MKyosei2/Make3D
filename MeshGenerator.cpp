#include "MeshGenerator.h"
#include <cmath>

Mesh3D GenerateMeshFromImage(const Image2D& image, int polygonCount) {
    Mesh3D mesh;

    int step = std::max(1, (image.width * image.height) / polygonCount);
    for (int y = 0; y < image.height - 1; y += step) {
        for (int x = 0; x < image.width - 1; x += step) {
            int i0 = mesh.vertices.size();
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
