#include "VolumeUtils.h"
#include <cmath>

void generateVoxelSphere(VolumeData& volume, float radius) {
    const int sx = volume.getSizeX();
    const int sy = volume.getSizeY();
    const int sz = volume.getSizeZ();
    const float cx = sx / 2.0f;
    const float cy = sy / 2.0f;
    const float cz = sz / 2.0f;

    for (int z = 0; z < sz; ++z) {
        for (int y = 0; y < sy; ++y) {
            for (int x = 0; x < sx; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                float dz = z - cz;
                float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                volume.set(x, y, z, dist <= radius);
            }
        }
    }
}
