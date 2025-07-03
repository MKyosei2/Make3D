#pragma once
#include <Windows.h>
#include <vector>

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data;
    Image(int w = 0, int h = 0);
    void resize(int w, int h);
    bool get(int x, int y) const;
    void set(int x, int y, unsigned char value);
};

Image ConvertBitmapToImage(HBITMAP hBitmap);
Image ExtractMaskFromBitmap(HBITMAP hBitmap, BYTE threshold = 128);
bool IsSilhouette(BYTE r, BYTE g, BYTE b, BYTE a, BYTE threshold = 128);