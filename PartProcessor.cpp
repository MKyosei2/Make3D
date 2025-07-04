#include "PreviewRenderer.h"
#include <cmath>

PreviewRenderer::PreviewRenderer(HWND hwnd) : hwnd(hwnd) {}
PreviewRenderer::~PreviewRenderer() {}

void PreviewRenderer::setMesh(const Mesh& mesh) {
    currentMesh = mesh;
    InvalidateRect(hwnd, nullptr, TRUE);
}

void PreviewRenderer::render() {
    HDC hdc = GetDC(hwnd);
    drawMesh(hdc);
    ReleaseDC(hwnd, hdc);
}

void PreviewRenderer::drawMesh(HDC hdc) {
    if (currentMesh.vertices.empty()) return;

    auto project = [](const Vertex& v) -> POINT {
        return {
            static_cast<int>(v.x * 100 + 150),
            static_cast<int>(v.y * -100 + 150)
        };
        };

    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 255, 0));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (const Triangle& tri : currentMesh.triangles) {
        POINT p1 = project(currentMesh.vertices[tri.v0]);
        POINT p2 = project(currentMesh.vertices[tri.v1]);
        POINT p3 = project(currentMesh.vertices[tri.v2]);

        MoveToEx(hdc, p1.x, p1.y, nullptr); LineTo(hdc, p2.x, p2.y);
        MoveToEx(hdc, p2.x, p2.y, nullptr); LineTo(hdc, p3.x, p3.y);
        MoveToEx(hdc, p3.x, p3.y, nullptr); LineTo(hdc, p1.x, p1.y);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}