#include "PreviewRenderer.h"
#include "GUIState.h"
#include "MeshUtils.h"
#include <windows.h>
#include <string>
#include <map>
#include <cmath>

extern GUIState guiState;

void drawMeshPreview(HDC hdc, RECT rect) {
    if (!guiState.previewMesh) return;

    FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    const float angleX = guiState.previewRotationX;
    const float angleY = guiState.previewRotationY;

    renderMeshToHDC(hdc, rect, *guiState.previewMesh, angleX, angleY);
}

void renderPreview(HDC hdc, RECT clientRect) {
    if (guiState.enablePreview && guiState.previewMesh) {
        drawMeshPreview(hdc, clientRect);
        return;
    }

    // 画像プレビュー（簡易）
    const int padding = 20;
    const int imageWidth = 100;
    const int imageHeight = 100;
    int x = padding;
    int y = padding;

    for (const auto& [dir, imgData] : guiState.viewImages) {
        if (!imgData.hBitmap) continue;

        HDC memDC = CreateCompatibleDC(hdc);
        HGDIOBJ oldBmp = SelectObject(memDC, imgData.hBitmap);
        StretchBlt(hdc, x, y, imageWidth, imageHeight, memDC, 0, 0,
            imgData.width, imgData.height, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteDC(memDC);

        RECT labelRect = { x, y + imageHeight + 2, x + imageWidth, y + imageHeight + 20 };
        DrawText(hdc, L"Image", -1, &labelRect, DT_CENTER | DT_TOP);

        x += imageWidth + padding;
        if (x + imageWidth > clientRect.right) {
            x = padding;
            y += imageHeight + 40;
        }
    }

    // 矩形描画（パーツ分類）
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);

    for (const auto& region : guiState.partSelectionRegions) {
        Rectangle(hdc, region.x, region.y, region.x + region.width, region.y + region.height);
    }

    if (guiState.drawingRegion) {
        RECT r = guiState.currentDragRect;
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
