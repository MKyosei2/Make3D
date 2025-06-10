#include "ImageUtils.h"
#include <windows.h>
#include <cmath>

bool IsSilhouette(const Pixel& pixel) {
    return (pixel.r + pixel.g + pixel.b) < 128 * 3;  // 黒っぽいものをシルエットと判定
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
    if (bmp.bmBitsPixel != 32) return nullptr;

    int w = bmp.bmWidth;
    int h = bmp.bmHeight;
    DWORD* src = (DWORD*)bmp.bmBits;

    HBITMAP hDst = CreateCompatibleBitmap(GetDC(0), w, h);
    BITMAP dstBmp = {};
    GetObject(hDst, sizeof(BITMAP), &dstBmp);
    DWORD* dst = (DWORD*)dstBmp.bmBits;

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            DWORD c = src[y * w + x];
            BYTE r = (BYTE)(c >> 16), g = (BYTE)(c >> 8), b = (BYTE)c;
            int lum = (int)(0.3 * r + 0.59 * g + 0.11 * b);

            DWORD cR = src[y * w + (x + 1)];
            BYTE r2 = (BYTE)(cR >> 16), g2 = (BYTE)(cR >> 8), b2 = (BYTE)cR;
            int lum2 = (int)(0.3 * r2 + 0.59 * g2 + 0.11 * b2);

            int diff = abs(lum - lum2);

            BYTE a = (diff > 16) ? 255 : 0;
            dst[y * w + x] = (a << 24) | (255 << 16) | (255 << 8) | 255;
        }
    }

    return hDst;
}
