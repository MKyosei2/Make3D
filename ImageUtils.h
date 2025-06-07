#pragma once
#include <windows.h>
#include <vector>

struct AlignmentParams {
    POINT center;
    SIZE size;
    bool valid;
};

// マスク自動抽出（α付き or αなし画像に対応）
HBITMAP ExtractMaskFromBitmap(HBITMAP hSrcBitmap);

// マスク領域の整合補正用
AlignmentParams computeAlignmentParams(const HBITMAP hBitmap);

// 内部ユーティリティ
bool GetMaskBoundingBox(HBITMAP hBitmap, RECT& outBoundingBox);
