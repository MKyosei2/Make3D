#include "BuildVolumeFromImages.h"
#include "ImageLoader.h"

Volume3D BuildVolumeFromImages(const std::map<ViewType, Image2D>& images) {
    if (images.find(ViewType::Front) == images.end()) return Volume3D();
    int width = images.at(ViewType::Front).width;
    int height = images.at(ViewType::Front).height;
    int depth = width;  // 深さ＝横幅で初期化（立方体ベース）

    Volume3D vol;
    vol.Resize(width, height, depth);

    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                bool solid = false;

                // 正面（Z正方向）
                auto it = images.find(ViewType::Front);
                if (it != images.end()) {
                    const auto& img = it->second;
                    if (img.IsOpaque(x, y)) solid = true;
                }

                // 背面（Z負方向）
                it = images.find(ViewType::Back);
                if (it != images.end()) {
                    const auto& img = it->second;
                    if (img.IsOpaque(width - 1 - x, y)) solid = solid && true;  // ANDで削る方式
                }

                // 左（X負方向）
                it = images.find(ViewType::Left);
                if (it != images.end()) {
                    const auto& img = it->second;
                    if (!img.IsOpaque(z, y)) solid = false;
                }

                // 右（X正方向）
                it = images.find(ViewType::Right);
                if (it != images.end()) {
                    const auto& img = it->second;
                    if (!img.IsOpaque(depth - 1 - z, y)) solid = false;
                }

                // 上（Y負方向）
                it = images.find(ViewType::Top);
                if (it != images.end()) {
                    const auto& img = it->second;
                    if (!img.IsOpaque(x, depth - 1 - z)) solid = false;
                }

                // 下（Y正方向）
                it = images.find(ViewType::Bottom);
                if (it != images.end()) {
                    const auto& img = it->second;
                    if (!img.IsOpaque(x, z)) solid = false;
                }

                if (solid) vol.Set(x, y, z, true);
            }
        }
    }

    return vol;
}
