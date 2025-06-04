#pragma once

#include <windows.h>
#include <map>
#include <vector>
#include "MeshUtils.h" // MeshData を使用

// 視点の種類
enum class ViewDirection {
    Front,
    Back,
    Left,
    Right,
    Top,
    Bottom
};

// 画像データ構造
struct ImageData {
    HBITMAP hBitmap = nullptr;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
};

// パーツ選択用の矩形
struct SelectionRegion {
    int x, y, width, height;
};

// GUIの状態を保持する構造体
struct GUIState {
    // 視点別画像
    std::map<ViewDirection, ImageData> viewImages;

    // パーツ分類矩形
    std::vector<SelectionRegion> partSelectionRegions;

    // ポリゴン数指定
    int polygonCount = 5000;

    // 矩形選択状態
    bool drawingRegion = false;
    POINT dragStart = { 0, 0 };
    RECT currentDragRect = { 0, 0, 0, 0 };

    // プレビュー用のメッシュ描画状態
    bool enablePreview = false;
    float previewRotationX = 0.0f;
    float previewRotationY = 0.0f;
    MeshData* previewMesh = nullptr;
};
