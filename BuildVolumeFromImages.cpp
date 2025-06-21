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

    // ① false で初期化（AND削除方式から塗り込み式へ）
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

        // ② 平均明度から動的しきい値決定
        long long total = 0;
        for (int y = 0; y < imgH; ++y)
            for (int x = 0; x < imgW; ++x) {
                int offset = y * stride + x * 3;
                BYTE r = pixels[offset + 2];
                BYTE g = pixels[offset + 1];
                BYTE b = pixels[offset + 0];
                total += r + g + b;
            }
        int avg = (int)(total / (imgW * imgH));
        int threshold = avg - 30;
        if (threshold < 30) threshold = 30;

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
                    int offset = iy * stride + ix * 3;
                    BYTE r = pixels[offset + 2];
                    BYTE g = pixels[offset + 1];
                    BYTE b = pixels[offset + 0];
                    int brightness = r + g + b;

                    if (brightness < threshold * 3) {
                        volume.set(x, y, z, true);
                        anyVoxelSet = true;
                    }
                }
            }
        }
        };

    // ③ 全方向マスク適用
    for (int i = 0; i < (int)ViewDirection::Count; ++i)
        applyMask((ViewDirection)i);

    // ④ 万が一 1 ボクセルも塗れていなければ、仮ボリューム出力
    if (!anyVoxelSet) {
        MessageBoxW(nullptr, L"画像から抽出できるボクセルが見つかりませんでした。\n仮の球体ボリュームを生成します。", L"代替生成", MB_OK);
        generateVoxelSphere(volume, sizeX / 2.5f);
    }
    int filled = 0;
    for (int z = 0; z < sizeZ; ++z)
        for (int y = 0; y < sizeY; ++y)
            for (int x = 0; x < sizeX; ++x)
                if (volume.get(x, y, z))
                    ++filled;

    // 💡 万が一全て false だったら「中央に 1 点だけ true を入れる」
    if (filled == 0) {
        MessageBoxW(nullptr, L"[最終救済] volume が空。中央を1ピクセル強制 ON", L"DEBUG", MB_OK);
        volume.set(sizeX / 2, sizeY / 2, sizeZ / 2, true);
    }
    return true;
}