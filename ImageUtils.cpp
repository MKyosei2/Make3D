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
    BITMAP bmp = {};
    GetObject(hSrcBitmap, sizeof(BITMAP), &bmp);
    int w = bmp.bmWidth;
    int h = bmp.bmHeight;
    HDC hdc = GetDC(nullptr);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<BYTE> pixels(w * h * 4);
    GetDIBits(hdc, hSrcBitmap, 0, h, pixels.data(), &bmi, DIB_RGB_COLORS);

    HBITMAP hDst = CreateCompatibleBitmap(hdc, w, h);
    HDC memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hDst);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            BYTE* px = &pixels[(y * w + x) * 4];
            BYTE a = px[3];
            int brightness = px[0] + px[1] + px[2];
            bool isFG = (a > 64) && (brightness < 180 * 3);
            COLORREF color = isFG ? RGB(10, 10, 10) : RGB(255, 255, 255);
            SetPixel(memDC, x, y, color);
        }
    }

    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    return hDst;
}