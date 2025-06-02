#include "PartVolumeBuilder.h"

Volume3D BuildVolumeFromImages(const std::map<ViewType, Image2D>& images) {
    int w = 64, h = 64, d = 32;
    Volume3D vol;
    vol.Resize(w, h, d);

    for (int z = 0; z < d; ++z)
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                bool visible = true;
                if (auto it = images.find(ViewType::Front); it != images.end())
                    visible = visible && it->second.IsOpaque(x, y);
                if (auto it = images.find(ViewType::Back); it != images.end())
                    visible = visible && it->second.IsOpaque(w - 1 - x, y);
                if (auto it = images.find(ViewType::Left); it != images.end())
                    visible = visible && it->second.IsOpaque(z, y);
                if (auto it = images.find(ViewType::Right); it != images.end())
                    visible = visible && it->second.IsOpaque(d - 1 - z, y);
                if (auto it = images.find(ViewType::Top); it != images.end())
                    visible = visible && it->second.IsOpaque(x, d - 1 - z);
                if (auto it = images.find(ViewType::Bottom); it != images.end())
                    visible = visible && it->second.IsOpaque(x, z);

                if (visible)
                    vol.At(x, y, z) = 1;
            }
    return vol;
}
