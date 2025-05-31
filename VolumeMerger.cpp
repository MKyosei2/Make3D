#include "VolumeMerger.h"

Volume MergeVolumes(const std::vector<Volume>& volumes) {
    if (volumes.empty()) return {};

    int w = volumes[0].width;
    int h = volumes[0].height;
    int d = volumes[0].depth;

    Volume result(w, h, d);

    for (const auto& vol : volumes) {
        for (int z = 0; z < d; ++z) {
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    result.set(x, y, z, result.get(x, y, z) || vol.get(x, y, z));
                }
            }
        }
    }

    return result;
}
