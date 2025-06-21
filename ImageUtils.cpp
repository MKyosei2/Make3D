#include "ImageUtils.h"
#include <windows.h>
#include <cmath>

bool IsSilhouette(const Pixel& pixel) {
    // アルファ付き画像では透明度も考慮（透過ならfalse）
    if (pixel.a < 64) return false;

    int brightness = pixel.r + pixel.g + pixel.b;
    return brightness < 180 * 3;  // ← やや緩めたしきい値（暗ければOK）
}

bool GetMaskBoundingBox(HBITMAP hBitmap, RECT& outBox) {
    BITMAP bmp = {};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    if (bmp.bmBitsPixel != 32) return false;

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    DWORD* pixels = (DWORD*)bmp.bmBits;

    int minX = width, minY = height, maxX = 0, maxY = 0;
    bool found = false;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            DWORD pixel = pixels[y * width + x];
            BYTE alpha = (pixel >> 24) & 0xFF;
            if (alpha > 32) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
                found = true;
            }
        }
    }

    if (!found) return false;

    outBox = { minX, minY, maxX, maxY };
    return true;
}

AlignmentParams computeAlignmentParams(const HBITMAP hBitmap) {
    AlignmentParams ap = {};
    RECT box = {};
    if (!GetMaskBoundingBox(hBitmap, box)) {
        ap.valid = false;
        return ap;
    }

    ap.center.x = (box.left + box.right) / 2;
    ap.center.y = (box.top + box.bottom) / 2;
    ap.size.cx = box.right - box.left;
    ap.size.cy = box.bottom - box.top;
    ap.valid = true;
    return ap;
}

// 輝度差からマスクを自動抽出（非常に単純なエッジ判定）
HBITMAP ExtractMaskFromBitmap(HBITMAP hSrcBitmap) {
    if (!hSrcBitmap) return nullptr;

    BITMAP bmp = {};
    if (GetObject(hSrcBitmap, sizeof(BITMAP), &bmp) == 0 || bmp.bmBitsPixel != 32)
        return nullptr;

    int w = bmp.bmWidth;
    int h = bmp.bmHeight;

    HDC hdc = GetDC(NULL);
    HDC srcDC = CreateCompatibleDC(hdc);
    HDC dstDC = CreateCompatibleDC(hdc);

    HBITMAP hDst = CreateCompatibleBitmap(hdc, w, h);
    if (!hDst) {
        DeleteDC(srcDC);
        DeleteDC(dstDC);
        ReleaseDC(NULL, hdc);
        return nullptr;
    }

    HBITMAP oldSrc = (HBITMAP)SelectObject(srcDC, hSrcBitmap);
    HBITMAP oldDst = (HBITMAP)SelectObject(dstDC, hDst);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            COLORREF c = GetPixel(srcDC, x, y);
            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);
            int lum = (int)(0.3 * r + 0.59 * g + 0.11 * b);

            // 横の隣接ピクセルと輝度差を比較（簡易エッジ検出）
            if (x < w - 1) {
                COLORREF c2 = GetPixel(srcDC, x + 1, y);
                int lum2 = (int)(0.3 * GetRValue(c2) + 0.59 * GetGValue(c2) + 0.11 * GetBValue(c2));
                int diff = abs(lum - lum2);

                if (diff > 16)
                    SetPixel(dstDC, x, y, RGB(255, 255, 255)); // マスク（白）
                else
                    SetPixel(dstDC, x, y, RGB(0, 0, 0)); // 背景（黒）
            }
            else {
                SetPixel(dstDC, x, y, RGB(0, 0, 0));
            }
        }
    }

    SelectObject(srcDC, oldSrc);
    SelectObject(dstDC, oldDst);
    DeleteDC(srcDC);
    DeleteDC(dstDC);
    ReleaseDC(NULL, hdc);

    return hDst;
}