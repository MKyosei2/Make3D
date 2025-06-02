#include "VolumeUtils.h"

// 正面・背面・側面などの複数画像から Volume を構築する
Volume BuildVolumeFromImages(const std::map<ViewType, Image2D>& images) {
    if (images.empty()) return Volume();

    // 基準サイズを取得（正面画像）
    auto it = images.find(ViewType::Front);
    if (it == images.end()) return Volume();
    const Image2D& front = it->second;
    int sx = front.width;
    int sy = front.height;
    int sz = (sx + sy) / 2; // 仮深度（後で改善可）

    Volume vol;
    vol.Resize(sx, sy, sz);

    // Front画像 → Z軸奥行き方向に投影
    for (int y = 0; y < sy; ++y) {
        for (int x = 0; x < sx; ++x) {
            if (front.At(x, y)) {
                for (int z = 0; z < sz; ++z) {
                    vol.At(x, y, z) = true;
                }
            }
        }
    }

    // 他の視点補完（Back, Left, Right, Top, Bottom）
    for (const auto& [view, img] : images) {
        if (view == ViewType::Front) continue;
        int w = img.width;
        int h = img.height;

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (!img.At(x, y)) continue;

                switch (view) {
                case ViewType::Back:
                    if (x < sx && y < sy)
                        for (int z = 0; z < sz; ++z)
                            vol.At(sx - 1 - x, y, sz - 1 - z) = true;
                    break;
                case ViewType::Left:
                    if (z < sz && y < sy)
                        for (int x_ = 0; x_ < sx; ++x_)
                            vol.At(x_, y, z) = true;
                    break;
                case ViewType::Right:
                    if (z < sz && y < sy)
                        for (int x_ = 0; x_ < sx; ++x_)
                            vol.At(x_, y, sz - 1 - z) = true;
                    break;
                case ViewType::Top:
                    if (x < sx && z < sz)
                        for (int y_ = 0; y_ < sy; ++y_)
                            vol.At(x, y_, z) = true;
                    break;
                case ViewType::Bottom:
                    if (x < sx && z < sz)
                        for (int y_ = 0; y_ < sy; ++y_)
                            vol.At(x, sy - 1 - y_, z) = true;
                    break;
                default:
                    break;
                }
            }
        }
    }

    return vol;
}

// 複数の Volume を1つに統合する
Volume MergeVolumes(const std::vector<Volume>& volumes) {
    if (volumes.empty()) return Volume();

    int w = volumes[0].width;
    int h = volumes[0].height;
    int d = volumes[0].depth;

    Volume result;
    result.Resize(w, h, d);

    for (const auto& v : volumes) {
        for (int z = 0; z < d; ++z)
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    result.At(x, y, z) |= v.At(x, y, z);
    }

    return result;
}