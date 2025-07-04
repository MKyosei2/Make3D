#define NOMINMAX
#include "ImageProcessor.h"
#include <algorithm>
#include <Windows.h>
#include <cmath>

Image::Image(int w, int h) : width(w), height(h), data(w* h, 0) {}

void Image::resize(int w, int h) {
    width = w;
    height = h;
    data.resize(w * h, 0);
}

bool Image::get(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    return data[x + y * width] > 0;
}

void Image::set(int x, int y, unsigned char value) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    data[x + y * width] = value;
}

Image ConvertBitmapToImage(HBITMAP hBitmap, BYTE threshold) {
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    Image img(bmp.bmWidth, bmp.bmHeight);

    HDC hdc = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hBitmap);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<BYTE> pixels(bmp.bmWidth * bmp.bmHeight * 4);
    GetDIBits(memDC, hBitmap, 0, bmp.bmHeight, pixels.data(), &bmi, DIB_RGB_COLORS);

    for (int y = 0; y < bmp.bmHeight; ++y) {
        for (int x = 0; x < bmp.bmWidth; ++x) {
            BYTE b = pixels[4 * (x + y * bmp.bmWidth) + 0];
            BYTE g = pixels[4 * (x + y * bmp.bmWidth) + 1];
            BYTE r = pixels[4 * (x + y * bmp.bmWidth) + 2];
            BYTE a = pixels[4 * (x + y * bmp.bmWidth) + 3];
            if (IsSilhouette(r, g, b, a, threshold)) {
                img.set(x, y, 255);
            }
        }
    }

    DeleteDC(memDC);
    ReleaseDC(nullptr, hdc);
    return img;
}

Image ExtractMaskFromBitmap(HBITMAP hBitmap, BYTE threshold) {
    return ConvertBitmapToImage(hBitmap, threshold);
}

bool IsSilhouette(BYTE r, BYTE g, BYTE b, BYTE a, BYTE threshold) {
    BYTE brightness = static_cast<BYTE>(0.299 * r + 0.587 * g + 0.114 * b);
    return a > 0 && brightness < threshold;
}

void GetMaskBoundingBox(const Image& mask, int& minX, int& minY, int& maxX, int& maxY) {
    minX = mask.width; minY = mask.height;
    maxX = 0; maxY = 0;
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (mask.get(x, y)) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }
}

void computeAlignmentParams(const Image& mask, int& centerX, int& centerY, int& maxSize) {
    int minX, minY, maxX, maxY;
    GetMaskBoundingBox(mask, minX, minY, maxX, maxY);
    centerX = (minX + maxX) / 2;
    centerY = (minY + maxY) / 2;
    maxSize = std::max(maxX - minX, maxY - minY);
}
