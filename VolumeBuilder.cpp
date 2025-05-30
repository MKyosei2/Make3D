#include "VolumeBuilder.h"

Volume BuildVolumeFromSilhouette(const Image2D& image, int depth) {
    Volume vol{ image.width, image.height, depth };
    vol.voxels.resize(vol.width * vol.height * vol.depth, 0);

    for (int z = 0; z < depth; ++z) {
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                int idx2D = (y * image.width + x) * 4;
                unsigned char alpha = image.pixels[idx2D + 3];
                int idx3D = z * vol.width * vol.height + y * vol.width + x;
                vol.voxels[idx3D] = (alpha > 0) ? 255 : 0;
            }
        }
    }
    return vol;
}