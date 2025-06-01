#include "PartVolumeBuilder.h"

Volume3D BuildVolumeFromImages(const std::map<ViewType, Image2D>& images, int sx, int sy, int sz) {
    Volume3D vol(sx, sy, sz);

    // Front (Z=0〜)
    if (images.count(ViewType::Front)) {
        const auto& img = images.at(ViewType::Front);
        for (int y = 0; y < sy; ++y) {
            for (int x = 0; x < sx; ++x) {
                if (img.IsOpaque(x * img.width / sx, y * img.height / sy)) {
                    for (int z = 0; z < sz / 4; ++z)
                        vol.Set(x, y, z, 255);
                }
            }
        }
    }

    // Back (Z=max〜)
    if (images.count(ViewType::Back)) {
        const auto& img = images.at(ViewType::Back);
        for (int y = 0; y < sy; ++y) {
            for (int x = 0; x < sx; ++x) {
                if (img.IsOpaque(x * img.width / sx, y * img.height / sy)) {
                    for (int z = sz - 1; z > sz * 3 / 4; --z)
                        vol.Set(x, y, z, 255);
                }
            }
        }
    }

    // 同様に Left / Right / Top / Bottom に対応可能（略）

    return vol;
}
