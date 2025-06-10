#include "BuildVolumeFromImages.h"
#include <windows.h>
#include "VolumeUtils.h" 

VolumeData BuildVolumeFromSingleImage(const Image& img) {
    int depth = 64;
    VolumeData volume(img.width, img.height, depth);

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            if (IsSilhouette(img.getPixel(x, y))) {
                for (int z = 0; z < depth; ++z) {
                    volume.set(x, y, z, true);
                }
            }
        }
    }

    return volume;
}

bool generateVolumeFromImages(const AppState& appState, VolumeData& volume) {
    int sizeX = volume.getSizeX();
    int sizeY = volume.getSizeY();
    int sizeZ = volume.getSizeZ();

    // 簡易的に全体を 0 クリア（未使用）
    for (int z = 0; z < sizeZ; ++z)
        for (int y = 0; y < sizeY; ++y)
            for (int x = 0; x < sizeX; ++x)
                volume.set(x, y, z, false);

    // 例: Front 画像で奥行きを削る処理（実装簡易化）
    HBITMAP hBmp = appState.images[(int)ViewDirection::Front];
    if (!hBmp) return false;

    BITMAP bmp;
    GetObject(hBmp, sizeof(BITMAP), &bmp);
    int width = bmp.bmWidth;
    int height = bmp.bmHeight;
    BYTE* bits = (BYTE*)bmp.bmBits;

    for (int z = 0; z < sizeZ; ++z) {
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                int imgX = x * width / sizeX;
                int imgY = y * height / sizeY;
                int pixelIdx = (imgY * bmp.bmWidthBytes) + (imgX * 3);

                BYTE r = bits[pixelIdx + 2];
                BYTE g = bits[pixelIdx + 1];
                BYTE b = bits[pixelIdx + 0];

                bool isSolid = (r + g + b) < 128 * 3; // 黒っぽいピクセルを内部とみなす
                if (isSolid) {
                    volume.set(x, y, z, true);
                }
            }
        }
    }

    return true;
}