#pragma once

#include <vector>
#include <windows.h>
#include "VolumeUtils.h"  // ★ VolumeData が必要なため
#include "common.h"       // ★ 念のため（VolumeDataの定義場所）

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

// ★★ 修正ここ：戻り値 void を明示 ★★
void generateMeshFromVolume(const VolumeData& volume, MeshData& mesh, int targetPolygons);
