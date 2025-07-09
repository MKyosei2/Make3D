#include "VolumeBuilder.h"
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

VolumeBuilder::VolumeBuilder() {}
VolumeBuilder::~VolumeBuilder() {}

bool VolumeBuilder::buildVolume(HBITMAP images[6], int silhouetteThreshold)
{
    for (int i = 0; i < 6; ++i)
    {
        if (!images[i]) {
            MessageBox(nullptr, L"画像が読み込まれていない方向があります。", L"エラー", MB_OK | MB_ICONERROR);
            return false;
        }

        // GDI+ Bitmap に変換
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

                // 輪郭（黒）の条件: R,G,B < silhouetteThreshold
                if (pixel.GetR() < silhouetteThreshold &&
                    pixel.GetG() < silhouetteThreshold &&
                    pixel.GetB() < silhouetteThreshold)
                {
                    // TODO: ボクセルに変換処理（現状は仮のログ出力）
                    // ここで (x, y, i方向) を元に 3D位置に変換して保存する
                }
            }
        }
    }

    MessageBox(nullptr, L"モデルボリューム生成完了（仮）", L"成功", MB_OK);
    return true;
}
