#include "BuildVolumeFromImages.h"
#include <windows.h>
#include "VolumeUtils.h"
#include <vector>
#include <cmath>

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

    // 初期化：すべて false に
    for (int z = 0; z < sizeZ; ++z)
        for (int y = 0; y < sizeY; ++y)
            for (int x = 0; x < sizeX; ++x)
                volume.set(x, y, z, false);

    bool anyVoxelSet = false;

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

        // 平均輝度を使って動的しきい値を決定
        long long total = 0;
        for (int y = 0; y < imgH; ++y)
            for (int x = 0; x < imgW; ++x) {
                int ofs = y * stride + x * 3;
                total += pixels[ofs + 0] + pixels[ofs + 1] + pixels[ofs + 2];
            }

        int avg = static_cast<int>(total / (imgW * imgH));
        int threshold = std::clamp(avg - 30, 16, 255);  // 自動調整

        for (int z = 0; z < sizeZ; ++z) {
            for (int y = 0; y < sizeY; ++y) {
                for (int x = 0; x < sizeX; ++x) {
                    int ix = 0, iy = 0;
                    switch (dir) {
                    case ViewDirection::Front:  ix = x * imgW / sizeX; iy = y * imgH / sizeY; break;
                    case ViewDirection::Back:   ix = (sizeX - 1 - x) * imgW / sizeX; iy = y * imgH / sizeY; break;
                    case ViewDirection::Left:   ix = z * imgW / sizeZ; iy = y * imgH / sizeY; break;
                    case ViewDirection::Right:  ix = (sizeZ - 1 - z) * imgW / sizeZ; iy = y * imgH / sizeY; break;
                    case ViewDirection::Top:    ix = x * imgW / sizeX; iy = (sizeZ - 1 - z) * imgH / sizeZ; break;
                    case ViewDirection::Bottom: ix = x * imgW / sizeX; iy = z * imgH / sizeZ; break;
                    default: continue;
                    }

                    if (ix < 0 || iy < 0 || ix >= imgW || iy >= imgH) continue;
                    int ofs = iy * stride + ix * 3;
                    int brightness = pixels[ofs + 0] + pixels[ofs + 1] + pixels[ofs + 2];

                    if (brightness < threshold * 3) {
                        volume.set(x, y, z, true);
                        anyVoxelSet = true;
                    }
                }
            }
        }
        };

    // 全方向にマスクを適用
    for (int i = 0; i < (int)ViewDirection::Count; ++i)
        applyMask((ViewDirection)i);

    // Voxelがまったく存在しない場合、代替で中央立方体を生成
    if (!anyVoxelSet) {
        MessageBoxW(nullptr, L"画像からボクセルを抽出できなかったため、中央に仮のボクセル立方体を生成します。", L"代替処理", MB_OK);
        int x0 = sizeX / 3, x1 = 2 * sizeX / 3;
        int y0 = sizeY / 3, y1 = 2 * sizeY / 3;
        int z0 = sizeZ / 3, z1 = 2 * sizeZ / 3;
        for (int z = z0; z < z1; ++z)
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                    volume.set(x, y, z, true);
    }

    return true;
}