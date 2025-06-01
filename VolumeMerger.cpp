#include "VolumeMerger.h"

Volume3D MergeVolumes(const std::vector<Volume3D>& volumes) {
    if (volumes.empty()) return Volume3D();

    int w = volumes[0].width;
    int h = volumes[0].height;
    int d = volumes[0].depth;

    Volume3D result(w, h, d);

    for (const auto& v : volumes) {
        for (int z = 0; z < d; ++z)
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    result.At(x, y, z) |= v.At(x, y, z);
    }

    return result;
}
