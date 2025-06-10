#pragma once
#include <windows.h>
#include <vector>
#include "common.h" 

struct AlignmentParams {
    POINT center;
    SIZE size;
    bool valid;
};

struct Pixel {
    BYTE r, g, b, a;  // RGBA形式
};

bool IsSilhouette(const Pixel& pixel);

struct Image {
    int width, height;
    std::vector<Pixel> pixels;

    Pixel getPixel(int x, int y) const {
        return pixels[y * width + x];
    }
};

bool IsSilhouette(const Pixel& pixel);

// マスク自動抽出（α付き or αなし画像に対応）
HBITMAP ExtractMaskFromBitmap(HBITMAP hSrcBitmap);

// マスク領域の整合補正用
AlignmentParams computeAlignmentParams(const HBITMAP hBitmap);

// 内部ユーティリティ
bool GetMaskBoundingBox(HBITMAP hBitmap, RECT& outBoundingBox);
