#include "BuildVolumeFromImages.h"
#include <windows.h>
#include "VolumeUtils.h" 
#include <vector>

struct RGBPixel {
    BYTE b, g, r;
};

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

    // ボリューム初期化
    for (int z = 0; z < sizeZ; ++z)
        for (int y = 0; y < sizeY; ++y)
            for (int x = 0; x < sizeX; ++x)
                volume.set(x, y, z, false);

    HBITMAP hBmp = appState.images[(int)ViewDirection::Front];
    if (!hBmp) return false;

    // GDIコンテキスト作成
    BITMAP bmp = {};
    GetObject(hBmp, sizeof(BITMAP), &bmp);

    HDC hdc = GetDC(nullptr);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight; // 上下反転
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24; // RGB
    bmi.bmiHeader.biCompression = BI_RGB;

    int stride = ((bmp.bmWidth * 3 + 3) & ~3);
    std::vector<BYTE> pixelData(stride * bmp.bmHeight);

    if (!GetDIBits(hdc, hBmp, 0, bmp.bmHeight, pixelData.data(), &bmi, DIB_RGB_COLORS)) {
        ReleaseDC(nullptr, hdc);
        return false;
    }

    ReleaseDC(nullptr, hdc);

    // ボリュームに埋め込み（しきい値判定）
    for (int z = 0; z < sizeZ; ++z) {
        for (int y = 0; y < sizeY; ++y) {
            for (int x = 0; x < sizeX; ++x) {
                int imgX = x * bmp.bmWidth / sizeX;
                int imgY = y * bmp.bmHeight / sizeY;

                int offset = imgY * stride + imgX * 3;
                BYTE b = pixelData[offset + 0];
                BYTE g = pixelData[offset + 1];
                BYTE r = pixelData[offset + 2];

                // シルエット判定：暗いピクセルを塗りつぶし
                bool isSolid = (r + g + b) < 128 * 3;
                if (isSolid) {
                    for (int dz = 0; dz < sizeZ; ++dz) {
                        volume.set(x, y, dz, true);
                    }
                }
            }
        }
    }

    return true;
}