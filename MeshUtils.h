#pragma once

#include <vector>
#include <windows.h>

struct Vertex {
    float x, y, z;
};

struct Triangle {
    int v0, v1, v2;
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
};

// メッシュを描画（簡易プレビュー用）
void renderMeshToHDC(HDC hdc, RECT rect, const MeshData& mesh, float rotX, float rotY);

// メッシュ生成（外部で実装）
void generateMeshFromVolume(const class VolumeData& volume, MeshData& mesh, int targetPolygons);
