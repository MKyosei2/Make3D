#include "BuildVolumeFromImages.h"
#include "ImageUtils.h"
#include "common.h"

#include <map>
#include <algorithm>
#include <cstring>

namespace BuildVolumeFromImages {

    VolumeData* buildVolumeFromMultipleImages(const std::map<ViewDirection, ImageData>& images) {
        const int sizeX = 128;
        const int sizeY = 128;
        const int sizeZ = 128;

        VolumeData* volume = new VolumeData(sizeX, sizeY, sizeZ);

        // 初期化（全ボクセルを true に）
        for (int z = 0; z < sizeZ; ++z) {
            for (int y = 0; y < sizeY; ++y) {
                for (int x = 0; x < sizeX; ++x) {
                    volume->set(x, y, z, true);
                }
            }
        }

        // 各視点画像を使ってボクセルを絞り込む（AND演算）
        for (const auto& [dir, img] : images) {
            if (!img.pixels || img.width <= 0 || img.height <= 0) continue;

            for (int z = 0; z < sizeZ; ++z) {
                for (int y = 0; y < sizeY; ++y) {
                    for (int x = 0; x < sizeX; ++x) {
                        // 各視点ごとの射影処理
                        int u = 0, v = 0;

                        switch (dir) {
                        case ViewDirection::Front:
                            u = x * img.width / sizeX;
                            v = y * img.height / sizeY;
                            break;
                        case ViewDirection::Back:
                            u = (sizeX - 1 - x) * img.width / sizeX;
                            v = y * img.height / sizeY;
                            break;
                        case ViewDirection::Left:
                            u = z * img.width / sizeZ;
                            v = y * img.height / sizeY;
                            break;
                        case ViewDirection::Right:
                            u = (sizeZ - 1 - z) * img.width / sizeZ;
                            v = y * img.height / sizeY;
                            break;
                        case ViewDirection::Top:
                            u = x * img.width / sizeX;
                            v = (sizeZ - 1 - z) * img.height / sizeZ;
                            break;
                        case ViewDirection::Bottom:
                            u = x * img.width / sizeX;
                            v = z * img.height / sizeZ;
                            break;
                        }

                        int pixelIndex = (v * img.width + u) * 4;
                        unsigned char alpha = img.pixels[pixelIndex + 3];
                        bool isSilhouette = (alpha > 128);

                        if (!isSilhouette) {
                            volume->set(x, y, z, false);  // この視点から見えない → ボクセル除去
                        }
                    }
                }
            }
        }

        return volume;
    }

} // namespace BuildVolumeFromImages
