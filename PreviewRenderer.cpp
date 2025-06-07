#include "PreviewRenderer.h"

PreviewRenderer::PreviewRenderer()
    : currentDirection(ViewDirection::Front) {
}

PreviewRenderer::~PreviewRenderer() {
}

void PreviewRenderer::setViewDirection(ViewDirection dir) {
    currentDirection = dir;
}

void PreviewRenderer::render(HDC hdc, const RECT& rect) {
    // 単純な描画テスト：視点に応じた色を塗るだけ
    COLORREF color = RGB(200, 200, 200);
    switch (currentDirection) {
    case ViewDirection::Front:  color = RGB(255, 0, 0); break;
    case ViewDirection::Back:   color = RGB(0, 255, 0); break;
    case ViewDirection::Left:   color = RGB(0, 0, 255); break;
    case ViewDirection::Right:  color = RGB(255, 255, 0); break;
    case ViewDirection::Top:    color = RGB(0, 255, 255); break;
    case ViewDirection::Bottom: color = RGB(255, 0, 255); break;
    default: break;
    }

    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}
