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
    const int sizeX = volume.getSizeX();
    const int sizeY = volume.getSizeY();
    const int sizeZ = volume.getSizeZ();

    // 初期化：すべて true（AND 論理積用）
    for (int z = 0; z < sizeZ; ++z)
        for (int y = 0; y < sizeY; ++y)
            for (int x = 0; x < sizeX; ++x)
                volume.set(x, y, z, true);

    auto applyMask = [&](ViewDirection dir) {
        HBITMAP hBitmap = appState.images[(int)dir];
        if (!hBitmap) return;

        BITMAP bmp = {};
        GetObject(hBitmap, sizeof(BITMAP), &bmp);
        const int imgW = bmp.bmWidth;
        const int imgH = bmp.bmHeight;
        const int stride = ((imgW * 3 + 3) & ~3);

        std::vector<BYTE> pixels(stride * imgH);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biWidth = imgW;
        bmi.bmiHeader.biHeight = -imgH;

        HDC hdc = GetDC(nullptr);
        GetDIBits(hdc, hBitmap, 0, imgH, pixels.data(), &bmi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);

        for (int z = 0; z < sizeZ; ++z) {
            for (int y = 0; y < sizeY; ++y) {
                for (int x = 0; x < sizeX; ++x) {
                    int ix = 0, iy = 0;

                    switch (dir) {
                    case ViewDirection::Front:
                        ix = x * imgW / sizeX;
                        iy = y * imgH / sizeY;
                        break;
                    case ViewDirection::Back:
                        ix = (sizeX - 1 - x) * imgW / sizeX;
                        iy = y * imgH / sizeY;
                        break;
                    case ViewDirection::Left:
                        ix = z * imgW / sizeZ;
                        iy = y * imgH / sizeY;
                        break;
                    case ViewDirection::Right:
                        ix = (sizeZ - 1 - z) * imgW / sizeZ;
                        iy = y * imgH / sizeY;
                        break;
                    case ViewDirection::Top:
                        ix = x * imgW / sizeX;
                        iy = (sizeZ - 1 - z) * imgH / sizeZ;
                        break;
                    case ViewDirection::Bottom:
                        ix = x * imgW / sizeX;
                        iy = z * imgH / sizeZ;
                        break;
                    default:
                        continue;
                    }

                    if (ix < 0 || iy < 0 || ix >= imgW || iy >= imgH) continue;

                    int offset = iy * stride + ix * 3;
                    BYTE r = pixels[offset + 2];
                    BYTE g = pixels[offset + 1];
                    BYTE b = pixels[offset + 0];
                    bool solid = (r + g + b) < 128 * 3;

                    if (!solid)
                        volume.set(x, y, z, false); // どれかが false なら除外
                }
            }
        }
        };

    // 全方向適用（存在する画像だけ使う）
    applyMask(ViewDirection::Front);
    applyMask(ViewDirection::Back);
    applyMask(ViewDirection::Left);
    applyMask(ViewDirection::Right);
    applyMask(ViewDirection::Top);
    applyMask(ViewDirection::Bottom);

    return true;
}