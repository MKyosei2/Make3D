#include "PartProcessor.h"
#include <windows.h>
#include <queue>

std::vector<PartRegion> extractRegionsFromMask(HBITMAP hBitmap) {
    std::vector<PartRegion> regions;

    BITMAP bmp = {};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    if (bmp.bmBitsPixel != 32) return regions;

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    DWORD* pixels = (DWORD*)bmp.bmBits;

    std::vector<int> labels(width * height, 0);
    int label = 1;

    auto get = [&](int x, int y) -> int& { return labels[y * width + x]; };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (get(x, y) > 0) continue;
            BYTE a = (pixels[y * width + x] >> 24) & 0xFF;
            if (a < 32) continue;

            std::queue<POINT> q;
            q.push({ x, y });
            get(x, y) = label;

            RECT box = { x, y, x, y };
            std::vector<POINT> outline;

            while (!q.empty()) {
                POINT p = q.front(); q.pop();
                outline.push_back(p);

                box.left = min(box.left, p.x);
                box.right = max(box.right, p.x);
                box.top = min(box.top, p.y);
                box.bottom = max(box.bottom, p.y);

                const int dx[4] = { 1, -1, 0, 0 };
                const int dy[4] = { 0, 0, 1, -1 };

                for (int i = 0; i < 4; ++i) {
                    int nx = p.x + dx[i];
                    int ny = p.y + dy[i];
                    if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
                    if (get(nx, ny) > 0) continue;

                    BYTE na = (pixels[ny * width + nx] >> 24) & 0xFF;
                    if (na < 32) continue;

                    get(nx, ny) = label;
                    q.push({ nx, ny });
                }
            }

            PartRegion region;
            region.boundingBox = box;
            region.outline = std::move(outline);
            region.label = label++;
            regions.push_back(region);
        }
    }

    return regions;
}

void drawRegionsToHDC(HDC hdc, const std::vector<PartRegion>& regions) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (const auto& region : regions) {
        Rectangle(hdc, region.boundingBox.left, region.boundingBox.top,
            region.boundingBox.right, region.boundingBox.bottom);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}
