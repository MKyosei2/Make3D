#include "MeshUtils.h"
#include "VolumeUtils.h"
#include <windows.h>
#include <cmath>

void renderMeshToHDC(HDC hdc, RECT rect, const MeshData& mesh, float rotX, float rotY) {
    const int w = rect.right - rect.left;
    const int h = rect.bottom - rect.top;

    auto project = [&](float x, float y, float z) -> POINT {
        // 回転
        float cx = cosf(rotY), sx = sinf(rotY);
        float cy = cosf(rotX), sy = sinf(rotX);
        float tx = x * cx + z * sx;
        float ty = y * cy - (x * sx - z * cx) * sy;
        float tz = y * sy + (x * sx - z * cx) * cy;

        float scale = 100.0f / (tz + 200.0f);
        int px = static_cast<int>(tx * scale + w / 2 + rect.left);
        int py = static_cast<int>(-ty * scale + h / 2 + rect.top);
        return { px, py };
        };

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(50, 50, 255));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    for (const auto& tri : mesh.triangles) {
        const auto& v0 = mesh.vertices[tri.v0];
        const auto& v1 = mesh.vertices[tri.v1];
        const auto& v2 = mesh.vertices[tri.v2];

        POINT p0 = project(v0.x, v0.y, v0.z);
        POINT p1 = project(v1.x, v1.y, v1.z);
        POINT p2 = project(v2.x, v2.y, v2.z);

        MoveToEx(hdc, p0.x, p0.y, nullptr);
        LineTo(hdc, p1.x, p1.y);
        LineTo(hdc, p2.x, p2.y);
        LineTo(hdc, p0.x, p0.y);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void generateMeshFromVolume(const VolumeData& volume, MeshData& mesh, int targetPolygons) {
    // 簡易な立方体メッシュを生成する仮実装（デバッグ用）

    mesh.vertices.clear();
    mesh.triangles.clear();

    // 1つだけ原点に立方体を作成
    float size = 10.0f;
    mesh.vertices = {
        { -size, -size, -size },
        {  size, -size, -size },
        {  size,  size, -size },
        { -size,  size, -size },
        { -size, -size,  size },
        {  size, -size,  size },
        {  size,  size,  size },
        { -size,  size,  size }
    };

    mesh.triangles = {
        { 0, 1, 2 }, { 0, 2, 3 },
        { 4, 5, 6 }, { 4, 6, 7 },
        { 0, 1, 5 }, { 0, 5, 4 },
        { 2, 3, 7 }, { 2, 7, 6 },
        { 1, 2, 6 }, { 1, 6, 5 },
        { 0, 3, 7 }, { 0, 7, 4 }
    };
}