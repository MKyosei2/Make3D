#include "PreviewRenderer.h"

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

    // 仮描画：頂点を点で表示
    for (const Vertex& v : currentMesh.vertices) {
        int x = static_cast<int>(v.x * 100 + 200);
        int y = static_cast<int>(v.y * 100 + 200);
        Ellipse(hdc, x - 2, y - 2, x + 2, y + 2);
    }
}
