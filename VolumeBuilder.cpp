#include "VolumeBuilder.h"
#include <vector>
#include <fstream>
#include <gdiplus.h>
#include <windows.h>

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

VolumeBuilder::VolumeBuilder() {}

bool VolumeBuilder::BuildFromImage(const std::wstring& imagePath, int width, int height, int depth)
{
    volumeData.clear();
    volumeData.resize(width * height * depth, 0);

    // GDI+ 初期化
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        return false;
    }

    // 画像読み込み
    Bitmap* bitmap = Bitmap::FromFile(imagePath.c_str());
    if (!bitmap || bitmap->GetLastStatus() != Ok) {
        GdiplusShutdown(gdiplusToken);
        return false;
    }

    // サイズ調整（必要なら）
    int imgW = bitmap->GetWidth();
    int imgH = bitmap->GetHeight();

    // 輝度をZ軸方向にマッピング
    for (int y = 0; y < height && y < imgH; ++y)
    {
        for (int x = 0; x < width && x < imgW; ++x)
        {
            Color color;
            if (bitmap->GetPixel(x, y, &color) != Ok) continue;

            // 輝度 = 0〜255 の範囲で Z の奥行きにマッピング
            BYTE luminance = static_cast<BYTE>((color.GetR() + color.GetG() + color.GetB()) / 3);
            int maxZ = static_cast<int>((luminance / 255.0f) * depth);

            for (int z = 0; z < maxZ; ++z)
            {
                int index = z * width * height + y * width + x;
                if (index < volumeData.size())
                {
                    volumeData[index] = 1;
                }
            }
        }
    }

    delete bitmap;
    GdiplusShutdown(gdiplusToken);
    return true;
}

const std::vector<unsigned char>& VolumeBuilder::GetVolumeData() const
{
    return volumeData;
}
