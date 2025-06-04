#include "ImageUtils.h"
#include <windows.h>
#include <wingdi.h>
#include <fstream>

bool loadPNGImage(const std::wstring& filename, ImageData& outImage) {
    HBITMAP hBitmap = (HBITMAP)LoadImageW(nullptr, filename.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hBitmap) return false;

    BITMAP bmp = {};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    outImage.hBitmap = hBitmap;
    outImage.width = bmp.bmWidth;
    outImage.height = bmp.bmHeight;

    const int pixelCount = bmp.bmWidth * bmp.bmHeight * 4;
    outImage.pixels = new unsigned char[pixelCount];
    memcpy(outImage.pixels, bmp.bmBits, pixelCount);

    return true;
}

void freeImage(ImageData& image) {
    if (image.pixels) {
        delete[] image.pixels;
        image.pixels = nullptr;
    }

    if (image.hBitmap) {
        DeleteObject(image.hBitmap);
        image.hBitmap = nullptr;
    }

    image.width = 0;
    image.height = 0;
}
