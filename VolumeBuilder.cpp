#include "VolumeBuilder.h"

Volume BuildVolumeFromSilhouette(const Image2D& image, ViewType view) {
    const int depth = 64;
    Volume vol;

    if (view == ViewType::Front || view == ViewType::Back) {
        vol.Resize(image.width, image.height, depth);
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                if (!image.IsOpaque(x, y)) continue;
                for (int z = 0; z < depth; ++z) {
                    vol.At(x, y, z) = 255;
                }
            }
        }
    }
    else if (view == ViewType::Left || view == ViewType::Right) {
        vol.Resize(depth, image.height, image.width);
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                if (!image.IsOpaque(x, y)) continue;
                for (int z = 0; z < depth; ++z) {
                    vol.At(z, y, x) = 255;
                }
            }
        }
    }
    else if (view == ViewType::Top || view == ViewType::Bottom) {
        vol.Resize(image.width, depth, image.height);
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                if (!image.IsOpaque(x, y)) continue;
                for (int z = 0; z < depth; ++z) {
                    vol.At(x, z, y) = 255;
                }
            }
        }
    }

    return vol;
}
