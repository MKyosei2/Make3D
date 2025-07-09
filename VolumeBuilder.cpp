#include "VolumeBuilder.h"
#include <gdiplus.h>
#include <string>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

VolumeBuilder::VolumeBuilder() {}
VolumeBuilder::~VolumeBuilder() {}

bool VolumeBuilder::buildVolume(HBITMAP images[6], int silhouetteThreshold)
{
    int setCount = 0;
    int loadedCount = 0;

    UINT maxWidth = 0, maxHeight = 0;

    // 最大画像サイズを取得
    for (int i = 0; i < 6; ++i) {
        if (!images[i]) continue;
        Bitmap bmp(images[i], nullptr);
        if (bmp.GetLastStatus() != Ok) continue;

        maxWidth = std::max<UINT>(maxWidth, bmp.GetWidth());
        maxHeight = std::max<UINT>(maxHeight, bmp.GetHeight());
    }

    if (maxWidth == 0 || maxHeight == 0) {
        MessageBox(nullptr, L"画像サイズの取得に失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    const int thickness = 4; // Z方向の厚み（任意変更可）

    // VolumeData 初期化（Z方向に厚みあり）
    volume = VolumeData((int)maxWidth, (int)maxHeight, thickness);

    for (int i = 0; i < 6; ++i)
    {
        if (!images[i]) continue;
        loadedCount++;

        Bitmap bmp(images[i], nullptr);
        if (bmp.GetLastStatus() != Ok) {
            MessageBox(nullptr, L"画像の読み取りに失敗しました。", L"エラー", MB_OK | MB_ICONERROR);
            return false;
        }

        UINT width = bmp.GetWidth();
        UINT height = bmp.GetHeight();

        for (UINT y = 0; y < height; ++y) {
            for (UINT x = 0; x < width; ++x) {
                Color pixel;
                bmp.GetPixel(x, y, &pixel);

                // 不透明ピクセルに厚みを持たせて Z方向に複製
                if (pixel.GetAlpha() > 0) {
                    for (int z = 0; z < thickness; ++z) {
                        volume.set((int)x, (int)y, z, true);
                        setCount++;
                    }
                }
            }
        }

        // 単一画像のみ処理（複数画像時はループ継続）
        break;
    }

    if (loadedCount == 0) {
        MessageBox(nullptr, L"画像が1枚も読み込まれていません。", L"エラー", MB_OK | MB_ICONERROR);
        return false;
    }

    std::wstring msg = L"画像枚数: " + std::to_wstring(loadedCount) +
        L"\nボクセル数: " + std::to_wstring(setCount);
    MessageBox(nullptr, msg.c_str(), L"VolumeBuilder 完了", MB_OK);
    return true;
}

const VolumeData& VolumeBuilder::getVolume() const {
    return volume;
}
